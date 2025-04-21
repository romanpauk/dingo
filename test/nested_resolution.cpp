//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#include <dingo/container.h>
#include <dingo/storage/external.h>
#include <dingo/storage/shared.h>
#include <dingo/storage/unique.h>

#include <gtest/gtest.h>

#include "assert.h"
#include "class.h"
#include "containers.h"

#if !defined(_MSC_VER) || DINGO_CXX_VERSION > 17

namespace dingo {
template <typename T> struct nested_resolution_test : public testing::Test {};

TYPED_TEST_SUITE(nested_resolution_test, container_types);

TYPED_TEST(nested_resolution_test, nested_types) {
    using container_type = TypeParam;

    struct Shared {
        int value;
    };
    struct SharedPtr {
        int value;
    };
    struct Unique {
        int value;
    };
    struct UniquePtr {
        int value;
    };
    struct External {
        int value;
    };

    struct Outer {
        struct Inner1 {
            int value;
            Shared* shared_raw_ptr;
            std::shared_ptr<SharedPtr> shared_ptr;
            std::unique_ptr<UniquePtr> unique_ptr;
            Unique unique;
            External external_value;
            External& external_ref;
            External* external_ptr;
        } inner1;

        struct Inner2 {
            Inner1 inner1;
        } inner2;

        struct Inner3 {
            Inner2 inner2;
            Inner1 inner1;
        } inner3;

        int value;
    };

    External ex{1};

    container_type container;

    container.template register_type<scope<unique>, storage<Unique>>();
    container.template register_type<scope<unique>, storage<std::unique_ptr<UniquePtr>>>();
    container.template register_type<scope<shared>, storage<Shared>>();
    container.template register_type<scope<shared>, storage<std::shared_ptr<SharedPtr>>>();
    container.template register_type<scope<external>, storage<External&>>(ex);
    container.template register_type<scope<external>, storage<int>>(11);
    container.template register_type<scope<unique>, storage<Outer>>();

    auto inner = container.template construct<typename Outer::Inner1>();
    ASSERT_EQ(inner.value, 11);

    auto outer = container.template resolve<Outer>();
    ASSERT_EQ(outer.value, 11);
    ASSERT_EQ(outer.inner1.value, 11);

    static_assert(constructor_detection<Outer>::arity == 4);
    static_assert(constructor_detection<Unique>::arity == 1);
    static_assert(constructor_detection<typename Outer::Inner1>::arity == 8);

    auto assert_inner = [&](auto& in) {
        ASSERT_EQ(in.value, 11);
        ASSERT_EQ(in.unique.value, 11);
        ASSERT_EQ(in.shared_raw_ptr->value, 11);
        ASSERT_EQ(in.shared_ptr->value, 11);
        ASSERT_EQ(in.unique_ptr->value, 11);
        ASSERT_EQ(in.external_value.value, 1);
        ASSERT_EQ(in.external_ref.value, 1);
        ASSERT_EQ(in.external_ptr->value, 1);
    };

    assert_inner(outer.inner1);
    assert_inner(outer.inner2.inner1);
    assert_inner(outer.inner3.inner1);
    assert_inner(outer.inner3.inner2.inner1);
}

TYPED_TEST(nested_resolution_test, value_resolution) {
    struct SharedPtr {
        int value;
    };

    struct UniquePtr {
        int value;
    };

    struct Inner1 {
        std::unique_ptr<UniquePtr> unique_ptr;
        std::shared_ptr<SharedPtr> shared_ptr;
    };

    struct Inner2 {
        Inner1 inner1;
    };

    struct Outer {
        Inner1 inner1;
        Inner2 inner2;
    };

    using container_type = TypeParam;
    container_type container;
    container.template register_type<scope<external>, storage<int>>(11);
    container.template register_type<scope<unique>, storage<std::unique_ptr<UniquePtr>>>();
    container.template register_type<scope<shared>, storage<std::shared_ptr<SharedPtr>>>();

    static_assert(dingo::constructor_detection<Inner1, dingo::detail::value>::arity == 2);
    static_assert(dingo::constructor_detection<Inner1, dingo::detail::reference>::arity == 2);
    static_assert(dingo::constructor_detection<Outer, dingo::detail::value>::arity == 2);
    // TODO:
    //static_assert(dingo::constructor_detection<Outer, dingo::detail::reference>::arity == 2);

    auto inner = container.template construct<Inner1, dingo::constructor_detection<Inner1, dingo::detail::value> >();
    ASSERT_EQ(inner.unique_ptr->value, 11);
    ASSERT_EQ(inner.shared_ptr->value, 11);

    auto outer = container.template construct<Outer, dingo::constructor_detection<Outer, dingo::detail::value> >();
    ASSERT_EQ(outer.inner1.unique_ptr->value, 11);
    ASSERT_EQ(outer.inner1.shared_ptr->value, 11);
}

} // namespace dingo

#endif