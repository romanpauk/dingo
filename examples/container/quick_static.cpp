//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#include <dingo/container.h>
#include <dingo/storage/shared.h>
#include <dingo/storage/unique.h>

int main() {
    using namespace dingo;
    ////
    struct A {
        A() {}
    };
    struct B {
        B(A&, std::shared_ptr<A>) {}
    };
    struct C {
        C(B*, std::unique_ptr<B>&, A&) {}
    };

    // Compile-time bindings declare the same storage and scope as runtime
    // registration.
    using app_bindings =
        bindings<dingo::bind<scope<shared>, storage<std::shared_ptr<A>>>,
                 dingo::bind<scope<shared>, storage<std::unique_ptr<B>>>,
                 dingo::bind<scope<unique>, storage<C>>>;

    container<app_bindings> container;
    // Resolution still uses the same API.
    C c = container.resolve<C>();

    struct D {
        A& a;
        B* b;
    };

    // construct() and invoke() work with compile-time registration, too.
    D d = container.construct<D>();
    D e = container.invoke([&](A& a, B* b) { return D{a, b}; });
    ////
    (void)c;
    (void)d;
    (void)e;
}
