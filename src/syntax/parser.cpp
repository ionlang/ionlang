#include <ionlang/lexical/classifier.h>
#include <ionlang/syntax/parser.h>

namespace ionlang {
    bool Parser::is(TokenKind tokenKind) noexcept {
        return this->tokenStream.get().kind == tokenKind;
    }

    bool Parser::isNext(TokenKind tokenKind) {
        return this->tokenStream.peek()->kind == tokenKind;
    }

    bool Parser::expect(TokenKind tokenKind) {
        if (this->is(tokenKind)) {
            return true;
        }

        this->diagnosticBuilder
            ->bootstrap(diagnostic::syntaxUnexpectedToken)
            ->setSourceLocation(this->makeSourceLocation())
            ->formatMessage(
                Grammar::findTokenKindNameOr(tokenKind),
                Grammar::findTokenKindNameOr(this->tokenStream.get().kind)
            )
            ->finish();

        return false;
    }

    bool Parser::skipOver(TokenKind tokenKind) {
        if (!this->expect(tokenKind)) {
            return false;
        }

        this->tokenStream.skip();

        return true;
    }

    void Parser::beginSourceLocationMapping() noexcept {
        this->sourceLocationMappingStartStack.push(std::make_pair(
            this->tokenStream.get().lineNumber,
            this->tokenStream.get().startPosition
        ));
    }

    ionshared::SourceLocation Parser::makeSourceLocation() {
        if (this->sourceLocationMappingStartStack.empty()) {
            throw std::runtime_error("Source mapping starting point stack is empty");
        }

        std::pair<uint32_t, uint32_t> mappingStart =
            this->sourceLocationMappingStartStack.top();

        Token currentToken = this->tokenStream.get();

        return ionshared::SourceLocation{
            ionshared::Span{
                mappingStart.first,
                currentToken.lineNumber - mappingStart.first
            },

            ionshared::Span{
                mappingStart.second,
                currentToken.getEndPosition() - mappingStart.second
            }
        };
    }

    ionshared::SourceLocation Parser::finishSourceLocation() {
        ionshared::SourceLocation sourceLocation = this->makeSourceLocation();

        this->sourceLocationMappingStartStack.pop();

        return sourceLocation;
    }

    std::shared_ptr<ErrorMarker> Parser::makeErrorMarker() {
        std::shared_ptr<ErrorMarker> errorMarker = std::make_shared<ErrorMarker>();

        errorMarker->sourceLocation = this->makeSourceLocation();

        return errorMarker;
    }

    void Parser::finishSourceLocationMapping(const std::shared_ptr<Construct>& construct) {
        construct->sourceLocation = this->finishSourceLocation();
    }

    Parser::Parser(
        TokenStream stream,
        std::shared_ptr<ionshared::DiagnosticBuilder> diagnosticBuilder
    ) noexcept :
        tokenStream(std::move(stream)),
        diagnosticBuilder(std::move(diagnosticBuilder)),
        sourceLocationMappingStartStack() {
        //
    }

    std::shared_ptr<ionshared::DiagnosticBuilder> Parser::getDiagnosticBuilder() const noexcept {
        return this->diagnosticBuilder;
    }

    AstPtrResult<> Parser::parseTopLevelFork(const std::shared_ptr<Module>& parent) {
        this->beginSourceLocationMapping();

        switch (this->tokenStream.get().kind) {
            case TokenKind::KeywordFunction: {
                return util::getResultValue(this->parseFunction(parent));
            }

            case TokenKind::KeywordGlobal: {
                return util::getResultValue(this->parseGlobal(parent));
            }

            case TokenKind::KeywordExtern: {
                return util::getResultValue(this->parseExtern(parent));
            }

            case TokenKind::KeywordStruct: {
                return util::getResultValue(this->parseStruct(parent));
            }

            default: {
                this->diagnosticBuilder
                    ->bootstrap(diagnostic::internalUnexpectedToken)
                    ->setSourceLocation(this->makeSourceLocation())
                    ->finish();

                return this->makeErrorMarker();
            }
        }
    }

    AstPtrResult<Global> Parser::parseGlobal(const std::shared_ptr<Module>& parent) {
        this->beginSourceLocationMapping();

        IONLANG_PARSER_ASSERT(this->skipOver(TokenKind::KeywordGlobal))

        // TODO: Not proper parent.
        AstPtrResult<Type> typeResult = this->parseType(parent);

        IONLANG_PARSER_ASSERT(util::hasValue(typeResult))

        std::optional<std::string> name = this->parseName();

        IONLANG_PARSER_ASSERT(name.has_value())

        AstPtrResult<Expression<>> valueResult;

        // Global is being initialized inline with a value. Parse & process the value.
        if (this->is(TokenKind::SymbolEqual)) {
            // Skip the equal symbol before continuing parsing.
            this->tokenStream.skip();

            // TODO: Not proper parent.
            // TODO: What about full expressions on globals?
            valueResult = this->parseLiteral(parent);

            IONLANG_PARSER_ASSERT(util::hasValue(valueResult))
        }

        IONLANG_PARSER_ASSERT(this->skipOver(TokenKind::SymbolSemiColon))

        std::shared_ptr<Global> global = Construct::makeChild<Global>(
            parent,
            util::getResultValue(typeResult),
            *name,
            util::getResultValue(valueResult)
        );

        this->finishSourceLocationMapping(global);

        return global;
    }

    AstPtrResult<Struct> Parser::parseStruct(const std::shared_ptr<Module>& parent) {
        this->beginSourceLocationMapping();

        IONLANG_PARSER_ASSERT(this->skipOver(TokenKind::KeywordStruct))

        std::optional<std::string> structNameResult = this->parseName();

        IONLANG_PARSER_ASSERT(structNameResult.has_value())
        IONLANG_PARSER_ASSERT(this->skipOver(TokenKind::SymbolBraceL))

        Fields fields = ionshared::util::makePtrSymbolTable<Type>();

        while (!this->is(TokenKind::SymbolBraceR)) {
            // TODO: Not proper parent.
            AstPtrResult<Type> fieldTypeResult = this->parseType(parent);

            IONLANG_PARSER_ASSERT(util::hasValue(fieldTypeResult))

            std::optional<std::string> fieldNameResult = this->parseName();

            IONLANG_PARSER_ASSERT(fieldNameResult.has_value())
            IONLANG_PARSER_ASSERT(this->skipOver(TokenKind::SymbolSemiColon))

            /**
             * A field with the same name was already previously
             * parsed and set on the fields map.
             */
            if (fields->contains(*fieldNameResult)) {
                // TODO: Replace strings, since 'structFieldRedefinition' takes in field and struct names.
                this->diagnosticBuilder
                    ->bootstrap(diagnostic::structFieldRedefinition)
                    ->setSourceLocation(this->makeSourceLocation())
                    ->finish();

                return this->makeErrorMarker();
            }

            fields->set(*fieldNameResult, util::getResultValue(fieldTypeResult));
        }

        IONLANG_PARSER_ASSERT(this->skipOver(TokenKind::SymbolBraceR))

        return Construct::makeChild<Struct>(parent, *structNameResult, fields);
    }

    AstPtrResult<Block> Parser::parseBlock(const std::shared_ptr<Construct>& parent) {
        this->beginSourceLocationMapping();

        IONLANG_PARSER_ASSERT(this->skipOver(TokenKind::SymbolBraceL))

        std::shared_ptr<Block> block = Construct::makeChild<Block>(parent);

        while (!this->is(TokenKind::SymbolBraceR)) {
            AstPtrResult<Statement> statement = this->parseStatement(block);

            IONLANG_PARSER_ASSERT(util::hasValue(statement))

            block->appendStatement(util::getResultValue(statement));
        }

        this->tokenStream.skip();

        return block;
    }

    AstPtrResult<Module> Parser::parseModule() {
        // TODO: This should be present anywhere IONLANG_PARSER_ASSERT is used, because it invokes the finalizer.
        this->beginSourceLocationMapping();

        IONLANG_PARSER_ASSERT(this->skipOver(TokenKind::KeywordModule))

        std::optional<std::string> id = this->parseName();

        IONLANG_PARSER_ASSERT(id.has_value())
        IONLANG_PARSER_ASSERT(this->skipOver(TokenKind::SymbolBraceL))

        Scope globalScope =
            std::make_shared<ionshared::SymbolTable<std::shared_ptr<Construct>>>();

        std::shared_ptr<Module> module = std::make_shared<Module>(*id, std::make_shared<Context>(globalScope));

        while (!this->is(TokenKind::SymbolBraceR)) {
            AstPtrResult<> topLevelConstructResult = this->parseTopLevelFork(module);

            // TODO: Make notice if it has no value? Or is it enough with the notice under 'parseTopLevel()'?
            if (util::hasValue(topLevelConstructResult)) {
                std::shared_ptr<Construct> topLevelConstruct = util::getResultValue(topLevelConstructResult);
                std::optional<std::string> name = util::findConstructId(topLevelConstruct);

                IONLANG_PARSER_ASSERT(name.has_value())

                // TODO: Ensure we're not re-defining something, issue a notice otherwise.
                globalScope->set(*name, topLevelConstruct);
            }

            // No more tokens to process.
            if (!this->tokenStream.hasNext() && !this->is(TokenKind::SymbolBraceR)) {
                this->getDiagnosticBuilder()
                    ->bootstrap(diagnostic::syntaxUnexpectedToken)

                    ->formatMessage(
                        Grammar::findTokenKindNameOr(TokenKind::SymbolBraceR),
                        Grammar::findTokenKindNameOr(this->tokenStream.get().kind)
                    )

                    ->setSourceLocation(this->makeSourceLocation())
                    ->finish();

                return this->makeErrorMarker();
            }
        }

        IONLANG_PARSER_ASSERT(this->skipOver(TokenKind::SymbolBraceR))

        this->finishSourceLocationMapping(module);

        return module;
    }

    AstPtrResult<VariableDeclStmt> Parser::parseVariableDecl(const std::shared_ptr<Block>& parent) {
        this->beginSourceLocationMapping();

        bool isTypeInferred = false;
        AstPtrResult<Type> typeResult{};

        if (this->is(TokenKind::KeywordLet)) {
            isTypeInferred = true;
            this->tokenStream.skip();
        }
        else {
            // TODO: Not proper parent.
            typeResult = this->parseType(parent);

            IONLANG_PARSER_ASSERT(util::hasValue(typeResult))
        }

        std::optional<std::string> name = this->parseName();

        IONLANG_PARSER_ASSERT(name.has_value())
        IONLANG_PARSER_ASSERT(this->skipOver(TokenKind::SymbolEqual))

        AstPtrResult<Expression<>> valueResult = this->parseExpression(parent);

        IONLANG_PARSER_ASSERT(util::hasValue(valueResult))

        PtrResolvable<Type> finalType = isTypeInferred
            ? util::getResultValue(valueResult)->type
            : Resolvable<Type>::make(util::getResultValue(typeResult));

        std::shared_ptr<VariableDeclStmt> variableDecl =
            Construct::makeChild<VariableDeclStmt>(
                parent,
                finalType,
                *name,
                util::getResultValue(valueResult)
            );

//        /**
//         * Variable declaration construct owns the type. Assign
//         * the type's parent.
//         */
//        if (!isTypeInferred) {
//            finalType->parent = variableDecl;
//        }

        // Register the statement on the resulting block's symbol table.
        parent->symbolTable->set(variableDecl->name, variableDecl);

        IONLANG_PARSER_ASSERT(this->skipOver(TokenKind::SymbolSemiColon))

        this->finishSourceLocationMapping(variableDecl);

        return variableDecl;
    }
}
