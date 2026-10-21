//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include "matrix/common/refusal.h"
#include "matrix/policies/policy_core.h"

#include <dingo/container.h>

#include <cstddef>
#include <type_traits>

namespace dingo::matrix::resolution {

template <typename Type, int Expected> struct local_value_ref {
  template <typename Container> static void check(Container &container) {
    auto &resolved = container.template resolve<Type &>();
    ASSERT_EQ(resolved.value, Expected);
    ASSERT_EQ(container.template construct<Type>().value, Expected);
  }
};

// A collection a binding with local bindings depends on, which the container
// and the local bindings both provide: the container binds element 1, the
// local bindings element 0. The next implementation makes the local bindings
// shadow the container's for a collection, as for one object, as the behavior
// change local_bindings_shadow_collections of the spec declares, so the
// collection holds element 0 alone; the baseline gathers both.
struct local_collection_expectation {
#if defined(DINGO_IMPLEMENTATION_NEXT)
  static constexpr std::size_t count = 1;
  static constexpr int sum = 0;
#else
  static constexpr std::size_t count = 2;
  static constexpr int sum = 1;
#endif
};

template <typename Type> struct local_collection_ref {
  template <typename Container> static void check(Container &container) {
    using expected = local_collection_expectation;
    auto &resolved = container.template resolve<Type &>();
    ASSERT_EQ(resolved.count, expected::count);
    ASSERT_EQ(resolved.sum, expected::sum);
    auto constructed = container.template construct<Type>();
    ASSERT_EQ(constructed.count, expected::count);
    ASSERT_EQ(constructed.sum, expected::sum);
  }
};

template <typename Type> struct ambiguous {
  template <typename Container> static void check(Container &container) {
    if constexpr (!ambiguity_refused_when_compiled_v<Container, Type &>) {
      ASSERT_THROW(container.template resolve<Type &>(),
                   dingo::type_ambiguous_exception);
    }
  }
};

template <typename Value, template <typename> typename Stats>
struct custom_allocator {
  template <typename Container> static void check(Container &container) {
    // How many bytes the container allocates is the implementation's; that it
    // allocates by the allocator is compared.
    ASSERT_TRUE(Stats<void>::allocated() > 0u);
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
