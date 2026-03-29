//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#include <dingo/container.h>
#include <dingo/storage/shared.h>

////
// struct A that will be registered with the container
struct A {};

// struct B that will be constructed using container
struct B {
    A& a;
};

////
int main() {
    using namespace dingo;
    ////
    container<> container;
    // Register struct A with shared scope
    container.register_type<scope<shared>, storage<A>>();
    // Construct instance of B, injecting shared instance of A
    /*B b =*/container.construct<B>();
    ////
}
