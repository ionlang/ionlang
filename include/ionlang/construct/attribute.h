#pragma once

#include <ionshared/misc/helpers.h>
#include <ionshared/misc/named.h>
#include <ionlang/construct/pseudo/child_construct.h>

namespace ionlang {
    struct Pass;

    struct Attribute : ConstructWithParent<>, ionshared::Named {
        explicit Attribute(std::string id);

        void accept(Pass& visitor) override;
    };

    typedef std::vector<std::shared_ptr<Attribute>> Attributes;
}
