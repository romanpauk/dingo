//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include "matrix/policies/policy_core.h"

#include <dingo/core/exceptions.h>

#include <type_traits>

namespace dingo::matrix::resolution {

template <typename Type, int Expected> struct local_value_ref {
  template <typename Container> static void check(Container &container) {
    auto &resolved = container.template resolve<Type &>();
    ASSERT_EQ(resolved.value, Expected);
    ASSERT_EQ(container.template construct<Type>().value, Expected);
  }
};

template <typename Type> struct local_collection_ref {
  template <typename Container> static void check(Container &container) {
    auto &resolved = container.template resolve<Type &>();
    ASSERT_EQ(resolved.count, 2u);
    ASSERT_EQ(resolved.sum, 1);
    auto constructed = container.template construct<Type>();
    ASSERT_EQ(constructed.count, 2u);
    ASSERT_EQ(constructed.sum, 1);
  }
};

template <typename Type> struct ambiguous {
  template <typename Container> static void check(Container &container) {
    ASSERT_THROW(container.template resolve<Type &>(),
                 dingo::type_ambiguous_exception);
  }
};

template <typename Value, template <typename> typename Stats>
struct custom_allocator {
  template <typename Container> static void check(Container &container) {
    ASSERT_GT(Stats<void>::allocated(), 0u);
    auto &resolved = container.template resolve<Value &>();
    ASSERT_TRUE(is_constructed_value(resolved));
  }
};

template <typename Value, typename ExpectedRtti> struct custom_rtti {
  template <typename Container> static void check(Container &container) {
    using rtti_type = typename Container::rtti_type;
    static_assert(std::is_same_v<rtti_type, ExpectedRtti>);
    auto &resolved = container.template resolve<Value &>();
    ASSERT_TRUE(is_constructed_value(resolved));
  }
};

} // namespace dingo::matrix::resolution
