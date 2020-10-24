#include <ionlang/passes/pass.h>

namespace ionlang {
    std::shared_ptr<VariableDeclStmt> VariableDeclStmt::make(
        const PtrResolvable<Type>& type,
        const std::string& id,
        const std::shared_ptr<Expression<>>& value
    ) noexcept {
        std::shared_ptr<VariableDeclStmt> result =
            std::make_shared<VariableDeclStmt>(type, id, value);

        type->parent = result;
        value->parent = result;

        return result;
    }

    VariableDeclStmt::VariableDeclStmt(
        PtrResolvable<Type> type,
        std::string id,
        std::shared_ptr<Expression<>> value
    ) noexcept :
        Statement(StatementKind::VariableDeclaration),
        ionshared::Named{std::move(id)},
        type(std::move(type)),
        value(std::move(value)) {
        //
    }

    void VariableDeclStmt::accept(Pass& visitor) {
        visitor.visitVariableDecl(this->dynamicCast<VariableDeclStmt>());
    }

    Ast VariableDeclStmt::getChildNodes() {
        return {
            this->type,
            this->value
        };
    }
}
