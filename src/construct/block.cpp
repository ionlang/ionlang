#include <utility>
#include <ionlang/passes/pass.h>
#include <ionlang/misc/statement_builder.h>

namespace ionlang {
    Block::Block(
        ionshared::Ptr<Construct> parent,
        std::vector<ionshared::Ptr<Statement>> statements,
        const ionshared::PtrSymbolTable<VariableDeclStatement> &symbolTable
    ) :
        ChildConstruct<Construct>(std::move(parent), ConstructKind::Block),
        ionshared::Scoped<VariableDeclStatement>(symbolTable),
        statements(std::move(statements)) {
        //
    }

    void Block::accept(Pass &visitor) {
        // TODO: Cast fails.
//        visitor.visitScopeAnchor(this->dynamicCast<ionshared::Scoped<Construct>>());
        visitor.visitBlock(this->dynamicCast<Block>());
    }

    Ast Block::getChildNodes() {
        return Construct::convertChildren(this->statements);
    }

    std::vector<ionshared::Ptr<Statement>> &Block::getStatements() noexcept {
        return this->statements;
    }

    void Block::setStatements(std::vector<ionshared::Ptr<Statement>> statements) {
        this->statements = std::move(statements);
    }

    void Block::appendStatement(const ionshared::Ptr<Statement> &statement) {
        this->statements.push_back(statement);

        /**
         * Variable declaration statements should be registered on
         * the local symbol table.
         */
        if (statement->getStatementKind() == StatementKind::VariableDeclaration) {
            ionshared::Ptr<VariableDeclStatement> variableDecl =
                statement->dynamicCast<VariableDeclStatement>();

            this->getSymbolTable()->set(variableDecl->getId(), variableDecl);
        }
    }

    uint32_t Block::relocateStatements(Block &target, uint32_t from) {
        uint32_t count = 0;

        for (uint32_t i = from; i < this->statements.size(); i++) {
            target.getStatements().push_back(this->statements[i]);
            this->statements.erase(this->statements.begin() + i - 1);
            count++;
        }

        return count;
    }

    ionshared::Ptr<Block> Block::split(uint32_t atOrder) {
        // TODO: If insts are empty, atOrder parameter is ignored (useless). Address that.

        if (!this->statements.empty() && (atOrder < 0 || atOrder > this->statements.size() - 1)) {
            throw std::out_of_range("Provided order is outsize of bounds");
        }

        std::vector<ionshared::Ptr<Statement>> statements = {};

        if (!this->statements.empty()) {
            auto from = this->statements.begin() + atOrder;
            auto to = this->statements.end();

            statements = std::vector<ionshared::Ptr<Statement>>(from, to);

            // Erase the instructions from the local basic block.
            this->statements.erase(from, to);
        }

        ionshared::Ptr<Block> newBlock = std::make_shared<Block>(Block{
            this->getParent(),
            statements

            // TODO: The symbol table needs to be relocated as well!
        });

        return newBlock;
    }

    std::optional<uint32_t> Block::locate(ionshared::Ptr<Statement> construct) const {
        return ionshared::util::locateInVector<ionshared::Ptr<Statement>>(
            this->statements,
            std::move(construct)
        );
    }

    ionshared::Ptr<StatementBuilder> Block::createBuilder() {
        return std::make_shared<StatementBuilder>(this->dynamicCast<Block>());
    }

    std::vector<ionshared::Ptr<Statement>> Block::findTerminals() const {
        std::vector<ionshared::Ptr<Statement>> terminalStatements = {};

        for (const auto &statement : this->statements) {
            if (statement->isTerminal()) {
                terminalStatements.push_back(statement);
            }
        }

        return terminalStatements;
    }

    ionshared::OptPtr<Statement> Block::findFirstStatement() noexcept {
        if (!this->statements.empty()) {
            return this->statements.front();
        }

        return std::nullopt;
    }

    ionshared::OptPtr<Statement> Block::findLastStatement() noexcept {
        if (!this->statements.empty()) {
            return this->statements.back();
        }

        return std::nullopt;
    }

    bool Block::isFunctionBody() const {
        ionshared::Ptr<Construct> parent = this->getParent();

        // TODO: Better way to know if this is a function body.
        if (parent == nullptr) {
            throw std::runtime_error("Unexpected parent construct to be nullptr");
        }

        /**
         * A block is considered a function body if it's parent is
         * a function construct.
         */
        return parent->getConstructKind() == ConstructKind::Function;
    }

    ionshared::OptPtr<Function> Block::findParentFunction() {
        ionshared::OptPtr<Function> parentFunction = std::nullopt;
        std::queue<ionshared::Ptr<ChildConstruct<Block>>> childrenOfBlockQueue = {};

        childrenOfBlockQueue.push(this->staticCast<ChildConstruct<Block>>());

        while (!childrenOfBlockQueue.empty()) {
            ionshared::Ptr<ChildConstruct<Block>> childOfBlock =
                childrenOfBlockQueue.front();

            childrenOfBlockQueue.pop();

            ionshared::Ptr<Construct> parent = childOfBlock->getParent();

            if (parent->getConstructKind() == ConstructKind::Function) {
                parentFunction = parent->dynamicCast<Function>();

                break;
            }
            else if (dynamic_cast<ChildConstruct<Block> *>(parent.get())) {
                childrenOfBlockQueue.push(parent->dynamicCast<ChildConstruct<Block>>());
            }
        }

        return parentFunction;
    }
}
