//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

// Include required dingo headers
#include <dingo/container.h>
#include <dingo/storage/shared.h>
#include <dingo/storage/unique.h>

int main() {
    using namespace dingo;
    ////
    // User types do not need Dingo-specific base classes or macros.
    struct A {
        A() {}
    };
    struct B {
        B(A&, std::shared_ptr<A>) {}
    };
    struct C {
        C(B*, std::unique_ptr<B>&, A&) {}
    };

    container<> container;
    // A and B are shared, so every resolution reuses the same instances.
    container.register_type<scope<shared>, storage<std::shared_ptr<A>>>();
    container.register_type<scope<shared>, storage<std::unique_ptr<B>>>();

    // C is unique, so every resolve<C>() creates a fresh C.
    container.register_type<scope<unique>, storage<C>>();

    // Constructor arguments are resolved recursively, including ownership
    // conversions from the registered storage forms.
    C c = container.resolve<C>();

    struct D {
        A& a;
        B* b;
    };

    // Construct an unmanaged object using dependencies from the container.
    D d = container.construct<D>();

    // Or invoke a callable with resolved arguments.
    D e = container.invoke([&](A& a, B* b) { return D{a, b}; });
    ////
    (void)c;
    (void)d;
    (void)e;
}
