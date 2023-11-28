#include <dingo/container.h>
#include <dingo/storage/external.h>
//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

////
struct A {
    A(int); // Definition is not required as constructor is not called
    A(double, double) {} // Definition is required as constructor is called
};

////

int main() {
    using namespace dingo;
    ////
    container<> container;
    container.register_type<scope<external>, storage<double>>(1.1);

    // Constructor with a highest arity will be used (factory<> is deduced
    // automatically)
    container.register_type<scope<unique>,
                            storage<A> /*, factory<constructor<A>> */>();
    ////
}
