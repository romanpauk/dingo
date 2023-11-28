//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#include <dingo/container.h>
#include <dingo/factory/constructor.h>
#include <dingo/storage/external.h>

#include <gtest/gtest.h>

#include "assert.h"
#include "class.h"
#include "containers.h"

namespace dingo {
template <typename T> struct nesting_test : public testing::Test {};

TYPED_TEST_SUITE(nesting_test, container_types);

TYPED_TEST(nesting_test, type) {
    using container_type = TypeParam;

    struct A {
        int value;
    };

    struct B {
        int value;
    };

    container_type container;
    container.template register_type<scope<external>, storage<int>>(42);
    container.template register_type<scope<unique>, storage<A>>();

    container.template register_type<scope<unique>, storage<B>>()
        .template register_type<scope<external>, storage<int>>(4);

    auto&& a = container.template resolve<A>();
    ASSERT_EQ(a.value, 42);
    auto&& b = container.template resolve<B>();
    ASSERT_EQ(b.value, 4);

    // Override B
    typename container_type::template child_container_type<void> container2(
        &container);
    container2.template register_type<scope<unique>, storage<B>>();
    auto&& b2 = container2.template resolve<B>();
    ASSERT_EQ(b2.value, 42);
}

TYPED_TEST(nesting_test, child_container) {
    using container_type = TypeParam;

    struct A {
        int value;
    };

    struct B {
        A a;
        int value;
    };

    container_type container;
    container.template register_type<scope<external>, storage<int>>(42);
    container.template register_type<scope<unique>, storage<A>>();

    // Static containers require different container types
    typename container_type::template child_container_type<void> container2(
        &container);
    container2.template register_type<scope<unique>, storage<B>>();
    container2.template register_type<scope<external>, storage<int>>(4);

    auto&& a = container.template resolve<A>();
    ASSERT_EQ(a.value, 42);
    auto&& b = container2.template resolve<B>();
    ASSERT_EQ(b.value, 4);
    ASSERT_EQ(b.a.value, 42);
}

TEST(nesting_test, child_container_dynamic) {
    using container_type = container<>;

    struct A {
        int value;
    };

    struct B {
        A a;
        int value;
    };

    container_type container;
    container.template register_type<scope<external>, storage<int>>(42);
    container.template register_type<scope<unique>, storage<A>>();

    // Dynamic containers can use same container type
    container_type container2(&container);
    container2.template register_type<scope<unique>, storage<B>>();
    container2.template register_type<scope<external>, storage<int>>(4);

    auto&& a = container.template resolve<A>();
    ASSERT_EQ(a.value, 42);
    auto&& b = container2.template resolve<B>();
    ASSERT_EQ(b.value, 4);
    ASSERT_EQ(b.a.value, 42);
}

} // namespace dingo