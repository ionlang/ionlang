#include <ionlang/passes/pass.h>
#include <ionlang/misc/type_factory.h>

namespace ionlang {
    CharLiteral::CharLiteral(char value) :
        Value(ValueKind::Character, type_factory::typeChar()),
        value(value) {
        //
    }

    void CharLiteral::accept(Pass &visitor) {
        visitor.visitCharValue(this->dynamicCast<CharLiteral>());
    }

    char CharLiteral::getValue() const noexcept {
        return this->value;
    }

    void CharLiteral::setValue(char value) noexcept {
        this->value = value;
    }
}