#include <ionlang/passes/codegen/ionir_codegen_pass.h>

namespace ionlang {
    void IonIrCodegenPass::visit(ionshared::Ptr<Construct> node) {
        /**
         * Only instruct the node to visit this instance and
         * not its children, since they're already visited by
         * the other member methods.
         */
        node->accept(*this);
    }

    void IonIrCodegenPass::visitExtern(ionshared::Ptr<Extern> node) {
        if (node->getPrototype() == nullptr) {
            throw std::runtime_error("Unexpected external definition's prototype to be null");
        }

        ionshared::OptPtr<ionir::Function> existingDefinition =
            moduleBuffer->get()->lookupFunction(node->getPrototype()->getId();

        if (ionshared::Util::hasValue(existingDefinition)) {
            throw std::runtime_error("Re-definition of extern not allowed");
        }

        // Visit the prototype.
        this->visitPrototype(node->getPrototype());

        // No need to push the resulting function onto the stack.
    }

    void IonIrCodegenPass::visitPrototype(ionshared::Ptr<Prototype> node) {
        // Retrieve argument count from the argument vector.
        uint32_t argumentCount = node->getArgs()->getItems().getSize();

        // Create the argument buffer vector.
        std::vector<llvm::Type *> arguments = {};

        // Attempt to retrieve an existing function.
        ionshared::OptPtr<ionir::Function> ionIrFunction =
            this->moduleBuffer->get()->lookupFunction(node->getId());

        // A function with a matching identifier already exists.
        if (ionshared::Util::hasValue(ionIrFunction)) {
            // Function already has a body, disallow re-definition.
            if (ionIrFunction->getBasicBlockList().empty()) {
                throw std::runtime_error("Cannot re-define function");
            }
                // If the function takes a different number of arguments, reject.
            else if (ionIrFunction->arg_size() != argumentCount) {
                throw std::runtime_error("Re-definition of function with a different amount arguments");
            }
        }
        // Otherwise, function will be created.
        else {
            for (uint32_t i = 0; i < argumentCount; ++i) {
                // TODO: Wrong type.
                arguments.push_back(llvm::Type::getDoubleTy(**this->contextBuffer));
            }

            // Visit and pop the return type.
            this->visitType(node->getReturnType());

            llvm::Type *llvmReturnType = this->typeStack.pop();

            // TODO: Support for infinite arguments and hard-coded return type.
            // Create the function type.
            llvm::FunctionType *type =
                llvm::FunctionType::get(llvmReturnType, arguments, node->getArgs()->getIsVariable());

            // Cast the value to a function, since we know getCallee() will return a function.
            ionIrFunction =
                llvm::dyn_cast<llvm::Function>((*this->moduleBuffer)->getOrInsertFunction(node->getId(), type).getCallee());
        }

        // Begin processing arguments. Argument count must be the same.
        if (argumentCount != ionIrFunction->arg_size()) {
            throw std::runtime_error("Expected argument count to be the same as the function's argument count");
        }

        int i = 0;

        for (auto &arg : ionIrFunction->args()) {
            // TODO: getItems() no longer a vector; cannot index by index, only key.
            // Retrieve the name element from the argument tuple.
            //            std::string name = node->getArgs()->getItems()[i].second;

            // Name the argument.
            //            arg.setName(name);

            // Increment the counter to prepare for next iteration.
            i++;
        }

        this->valueStack.push(ionIrFunction);
    }
}
