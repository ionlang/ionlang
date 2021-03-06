#pragma once

#include <utility>
#include <string>
#include <ionshared/misc/helpers.h>
#include <ionshared/tracking/symbol_table.h>
#include <ionlang/construct/type.h>

namespace ionlang {
    struct ArgumentList : ScopedConstruct {
        static std::shared_ptr<ArgumentList> make(
            const ionshared::PtrSymbolTable<Construct>& symbolTable =
                ionshared::util::makePtrSymbolTable<Construct>(),

            bool isVariable = false
        ) noexcept;

        bool isVariable;

        explicit ArgumentList(
            const ionshared::PtrSymbolTable<Construct>& symbolTable =
                ionshared::util::makePtrSymbolTable<Construct>(),

            bool isVariable = false
        );

        void accept(Pass& visitor) override;

        Ast getChildNodes() override;
    };
}
