#include <ionlang/passes/pass.h>

namespace ionlang {
    VariableDeclStatement::VariableDeclStatement(const VariableDeclStatementOpts& opts) noexcept :
        Statement(opts.parent, StatementKind::VariableDeclaration),
        ionshared::Named{opts.id},
        type(opts.type),
        value(opts.value) {
        //
    }

    void VariableDeclStatement::accept(Pass& visitor) {
        visitor.visitVariableDecl(this->dynamicCast<VariableDeclStatement>());
    }

    Ast VariableDeclStatement::getChildNodes() {
        return {
            this->type,
            this->value
        };
    }
}
