//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#include <dingo/container.h>
#include <dingo/storage/shared.h>

////
// Interface that will be resolved
struct IA {
    virtual ~IA() {}
};
// Struct implementing the interface
struct A : IA {};

////
#define MAYBE_UNUSED(x) (void)(x)

int main() {
    using namespace dingo;
    ////
    container<> container;

    // Register struct A, resolvable as interface IA
    container.register_type<scope<shared>, storage<A>, interface<IA>>();
    // Resolve instance A through interface IA
    IA& instance = container.resolve<IA&>();
    assert(dynamic_cast<A*>(&instance));
    ////

    MAYBE_UNUSED(instance);
}
