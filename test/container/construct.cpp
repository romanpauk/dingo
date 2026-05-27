//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#include "container/construct_common.h"

namespace dingo {

TYPED_TEST_SUITE(construct_test, container_types, );

TEST(construct_test, constructor_traits) {
    struct A {};
    struct B {
        B(std::shared_ptr<A>) {}
    };

    static_assert(construction_traits<std::variant<A, B>, A>::enabled);
    static_assert(construction_traits<const std::variant<A, B>, B>::enabled);
    static_assert(!construction_traits<std::variant<A, B>, int>::enabled);
    static_assert(std::is_same_v<
                  detail::alternative_type_alternatives_t<std::variant<A, B>>,
                  type_list<A, B>>);
    static_assert(!detail::type_list_has_duplicates<
                  detail::alternative_type_alternatives_t<std::variant<A, B>>>::
                      value);
    static_assert(
        detail::alternative_type_count<std::variant<A, B>, A>::value == 1);
    static_assert(
        detail::alternative_type_count<std::variant<A, B>, B>::value == 1);
    static_assert(
        detail::alternative_type_count<std::variant<A, B>, int>::value == 0);
    static_assert(
        is_alternative_type_interface_compatible_v<std::variant<A, B>, A>);
    static_assert(
        is_alternative_type_interface_compatible_v<std::variant<A, B>, B>);
    static_assert(
        !is_alternative_type_interface_compatible_v<std::variant<A, B>, int>);

    constructor_traits<A>::construct();
    constructor_traits<B>::construct(nullptr);
    delete constructor_traits<B*>::construct(nullptr);
    constructor_traits<std::unique_ptr<B>>::construct(nullptr);
    constructor_traits<std::shared_ptr<B>>::construct(nullptr);
    constructor_traits<std::optional<B>>::construct(nullptr);
}

TYPED_TEST(construct_test, plain) {
    using container_type = TypeParam;
    container_type container;
    container.template construct<int>();
    container.template construct<std::unique_ptr<int>>();
    container.template construct<std::shared_ptr<int>>();
    container.template construct<std::optional<int>>();
}

#if 0
// TODO: can ambiguous construction be detected?
TYPED_TEST(construct_test, ambiguous) {
    using container_type = TypeParam;
    struct A {
        A() {}
        A(int) {}
    };

    container_type container;
    container.template construct<A>();
}
#endif

TYPED_TEST(construct_test, aggregate1) {
    using container_type = TypeParam;
    struct A {
        int x;
    };
    struct B {
        B(A&&) {}
    };
    container_type container;
    container.template register_type<scope<unique>, storage<A>>();
    container.template register_type<scope<unique>, storage<int>>();
    container.template construct<B>();
}

TYPED_TEST(construct_test, aggregate2) {
    using container_type = TypeParam;
    struct A {
        int a = 1;
    };
    struct B {
        A a0;
    };
    struct C {
        A a0;
        A a1;
    };
    struct D {
        int a;
        int b;
    };

    container_type container;
    container.template register_type<scope<external>, storage<int>>(2);
    container.template register_type<scope<unique>, storage<A>>();
    auto b = container.template construct<B>();
    auto c = container.template construct<C>();
    auto d = container.template construct<D>();

    ASSERT_EQ(b.a0.a, 2);
    ASSERT_EQ(c.a0.a, 2);
    ASSERT_EQ(c.a1.a, 2);
    ASSERT_EQ(d.a, 2);
    ASSERT_EQ(d.b, 2);
}

TYPED_TEST(construct_test, resolve) {
    using container_type = TypeParam;
    struct A {};
    struct B {
        B(A&&) {}
    };
    struct C {
        C(B&) {}
    };

    container_type container;
    container.template register_type<scope<unique>, storage<A>>();
    container
        .template register_type<scope<shared>, storage<std::shared_ptr<B>>>();

    container.template construct<B>();
    container.template construct<std::unique_ptr<B>>();
    container.template construct<std::shared_ptr<B>>();
    container.template construct<std::optional<B>>();

    container.template construct<C>();
}

TYPED_TEST(construct_test, variant_selected_alternative) {
    using container_type = TypeParam;

    struct A {
        explicit A(int init) : value(init) {}
        int value;
    };
    struct B {
        explicit B(float init) : value(init) {}
        float value;
    };

    container_type container;
    container.template register_type<scope<external>, storage<int>>(7);
    container.template register_type<scope<external>, storage<float>>(3.5f);

    auto detected =
        container.template construct<std::variant<A, B>, constructor<A>>();
    ASSERT_TRUE(std::holds_alternative<A>(detected));
    EXPECT_EQ(std::get<A>(detected).value, 7);

    auto explicit_ctor =
        container
            .template construct<std::variant<A, B>, constructor<B(float)>>();
    ASSERT_TRUE(std::holds_alternative<B>(explicit_ctor));
    EXPECT_FLOAT_EQ(std::get<B>(explicit_ctor).value, 3.5f);
}

} // namespace dingo
