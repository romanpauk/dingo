//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#include <dingo/container.h>
#include <dingo/storage/shared.h>

////
struct A {};
////

int main() {
    using namespace dingo;

    ////
    container<> container;
    // Register struct A with shared scope
    container.register_type<scope<shared>, storage<A>>();
    // Resolution will always return the same A instance
    assert(container.resolve<A*>() == &container.resolve<A&>());
    ////
}
