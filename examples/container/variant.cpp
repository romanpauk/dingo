//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#include <cassert>
#include <variant>

#include <dingo/container.h>
#include <dingo/factory/constructor.h>
#include <dingo/storage/external.h>
#include <dingo/storage/shared.h>
#include <dingo/storage/unique.h>

////
struct A {
    explicit A(int init) : value(init) {}
    int value;
};

struct B {
    explicit B(float init) : value(init) {}
    float value;
};

////
int main() {
    using namespace dingo;
    ////
    container<> construct_container;
    construct_container.register_type<scope<external>, storage<int>>(7);
    construct_container.register_type<scope<external>, storage<float>>(3.5f);

    // Construct a variant by selecting which alternative should be built.
    [[maybe_unused]] auto detected =
        construct_container.construct<std::variant<A, B>, constructor<A>>();
    assert(std::holds_alternative<A>(detected));

    [[maybe_unused]] auto explicit_ctor =
        construct_container.construct<std::variant<A, B>,
                                      constructor<B(float)>>();
    assert(std::holds_alternative<B>(explicit_ctor));

    container<> unique_container;
    unique_container.register_type<scope<unique>, storage<int>>();
    unique_container.register_type<
        scope<unique>, storage<std::variant<A, B>>,
        factory<constructor<A>>>();

    // Resolve either the whole variant or its currently held alternative.
    [[maybe_unused]] auto value =
        unique_container.resolve<std::variant<A, B>>();
    assert(std::holds_alternative<A>(value));

    [[maybe_unused]] auto selected = unique_container.resolve<A>();

    try {
        unique_container.resolve<B>();
        assert(false);
    } catch (const type_not_convertible_exception&) {
    }

    std::variant<A, B> existing(std::in_place_type<A>, 9);
    container<> external_container;
    external_container
        .register_type<scope<external>, storage<std::variant<A, B>&>>(existing);

    [[maybe_unused]] auto& ref =
        external_container.resolve<std::variant<A, B>&>();
    assert(&ref == &existing);
    assert(std::holds_alternative<A>(ref));

    [[maybe_unused]] auto& held = external_container.resolve<A&>();
    assert(&held == &std::get<A>(existing));
    ////
}
