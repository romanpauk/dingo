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

struct overloaded_factory {
    A operator()(int value) const { return A{value * 2}; }
    A operator()(float value) const { return A{static_cast<int>(value) + 100}; }
};

////

int main() {
    using namespace dingo;
    ////
    container<> container;
    // Register int that will be passed to the callable below
    container.register_type<scope<external>, storage<int>>(2);
    // Register A that will be instantiated by calling provided lambda function
    // with arguments resolved using the container.
    container.register_type<scope<unique>, storage<A>>(
        callable([](int value) { return A{value * 2}; }));
    assert(container.resolve<A>().value == 4);
    // Explicit signatures also work for overloaded functors.
    assert(container.construct<A>(callable<A(int)>(overloaded_factory{})).value ==
           4);
    ////
}
