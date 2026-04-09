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
    A(int);      // Definition is not required as constructor is not called
    A(double) {} // Definition is required as constructor is called
};

////
int main() {
    using namespace dingo;
    ////
    container<> container;
    container.register_type<scope<external>, storage<double>>(1.1);

    // Register A with the explicitly selected A(double) constructor.
    // Manual disambiguation is required to avoid a compile-time assertion.
    container.register_type<scope<unique>, storage<A>,
                            factory<constructor<A(double)>>>();
    ////
}
