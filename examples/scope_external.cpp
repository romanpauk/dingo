//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#include <dingo/container.h>
#include <dingo/storage/external.h>

////
struct A {
} instance;

////
int main() {
    using namespace dingo;
    ////
    container<> container;
    // Register existing instance of A, stored as a pointer.
    container.register_type<scope<external>, storage<A*>>(&instance);
    // Resolution will return an existing instance of A casted to required type.
    assert(&container.resolve<A&>() == container.resolve<A*>());
    ////
}
