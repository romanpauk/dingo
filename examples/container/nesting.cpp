//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#include <dingo/container.h>
#include <dingo/storage/external.h>
#include <dingo/storage/unique.h>

////
struct A {
    int value;
};
struct B {
    int value;
};

////

int main() {
    using namespace dingo;
    ////
    container<> base_container;
    base_container.register_type<scope<external>, storage<int>>(1);
    base_container.register_type<scope<unique>, storage<A>>();
    // Resolving A will use A{1} to construct A
    assert(base_container.resolve<A>().value == 1);

    base_container.register_type<scope<unique>, storage<B>>()
        .register_type<scope<external>, storage<int>>(
            2); // Override value of int for struct B
    // Resolving B will use B{2} to construct B
    assert(base_container.resolve<B>().value == 2);

    // Create a nested container, overriding B (effectively removing override in
    // base_container)
    container<> nested_container(&base_container);
    nested_container.register_type<scope<unique>, storage<B>>();
    // Resolving B using nested container will use B{1} as provided by the
    // parent container to construct B
    assert(nested_container.resolve<B>().value == 1);
    ////
}
