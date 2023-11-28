//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#include <dingo/container.h>
#include <dingo/factory/callable.h>
#include <dingo/storage/external.h>
#include <dingo/storage/unique.h>

////
struct A {
    int value;
};

////

int main() {
    using namespace dingo;
    ////
    container<> container;
    // Register double that will be passed to lambda function below
    container.register_type<scope<external>, storage<int>>(2);
    // Register A that will be instantiated by calling provided lambda function
    // with arguments resolved using the container.
    container.register_type<scope<unique>, storage<A>>(
        callable([](int value) { return A{value * 2}; }));
    assert(container.resolve<A>().value == 4);
    ////
}
