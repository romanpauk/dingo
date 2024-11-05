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

namespace dingo {
template <typename T> struct nested_resolution_test : public testing::Test {};

TYPED_TEST_SUITE(nested_resolution_test, container_types);

TYPED_TEST(nested_resolution_test, nested_types) {
    using container_type = TypeParam;

    struct Shared {
        int value;
    };
    struct SharedPtr {};

    struct Unique {
        int value;
    };
    struct UniquePtr {};

    struct External {
        int value;
    };

    struct Outer {
        struct Inner1 {
            Shared* shared_raw_ptr;
            // TODO:
            //std::shared_ptr<SharedPtr> shared_ptr;
            Unique unique;
            // TODO:
            //std::unique_ptr<UniquePtr> unique_ptr;
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
    };

    External ex{1};

    container_type container;

    container.template register_type<scope<unique>, storage<Unique>>();
    container.template register_type<scope<unique>, storage<UniquePtr>>();
    container.template register_type<scope<shared>, storage<Shared>>();
    container.template register_type<scope<shared>, storage<SharedPtr>>();
    container.template register_type<scope<external>, storage<External&>>(ex);
    container.template register_type<scope<external>, storage<int>>(11);
    container.template register_type<scope<unique>, storage<Outer>>();

    auto outer = container.template resolve<Outer>();

    auto assert_inner = [&](auto& inner) {
        ASSERT_EQ(inner.unique.value, 11);
        ASSERT_EQ(inner.shared_raw_ptr->value, 11);
        ASSERT_EQ(inner.external_value.value, 1);
        ASSERT_EQ(inner.external_ref.value, 1);
        ASSERT_EQ(inner.external_ptr->value, 1);
    };

    assert_inner(outer.inner1);
    assert_inner(outer.inner2.inner1);
    assert_inner(outer.inner3.inner1);
    assert_inner(outer.inner3.inner2.inner1);
}

} // namespace dingo
