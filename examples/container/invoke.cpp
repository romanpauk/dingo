//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#include <dingo/container.h>
#include <dingo/storage/external.h>
#include <dingo/storage/shared.h>

#include <functional>

// struct A that will be registered with the container
struct A {};

////
// struct B that will be constructed using container
struct B {
    A& a;
    static B factory(A& a) { return B{a}; }
};

struct overloaded_factory {
    B operator()(A& a) const { return B{a}; }
    int operator()(int value) const { return value * 2; }
};
////
int main() {
    using namespace dingo;
    container<> container;
    // Register struct A with shared scope
    container.register_type<scope<shared>, storage<A>>();
    container.register_type<scope<external>, storage<int>>(2);
    ////
    // Construct instance of B, injecting shared instance of A
    /*B b1 =*/container.invoke([&](A& a) { return B{a}; });
    /*B b2 =*/container.invoke(
        std::function<B(A&)>([](auto& a) { return B{a}; }));
    /*B b3 =*/container.invoke(B::factory);
    // Use an explicit signature to disambiguate overloaded call operators.
    /*B b4 =*/container.invoke<B(A&)>(overloaded_factory{});
    ////
}
