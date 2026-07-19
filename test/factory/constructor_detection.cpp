//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#include <dingo/container.h>
#include <dingo/factory/constructor_detection.h>

#include <gtest/gtest.h>

#include <type_traits>
#include <utility>

#include "matrix/fixtures/constructor_detection.h"

namespace dingo {

#if !defined(_MSC_VER)
struct unresolved_argument_category {};
#endif

TEST(constructor_detection_test, public_constructor_argument_metadata) {
  struct explicit_constructor {
    explicit_constructor(int, float) {}
  };

  static_assert(
      std::is_same_v<
          typename constructor<explicit_constructor(int, float)>::arguments,
          type_list<int, float>>);
  static_assert(std::is_same_v<typename constructor_detection<
                                   matrix::constructor_two_values,
                                   detail::constructor_signature>::arguments,
                               type_list<int, float>>);
  static_assert(
      std::is_same_v<typename constructor_detection<
                         matrix::constructor_typedef_selected>::arguments,
                     type_list<double, const char *>>);
  static_assert(std::is_same_v<typename constructor_detection<
                                   matrix::constructor_defaulted>::arguments,
                               type_list<>>);
}

#if !defined(_MSC_VER)
TEST(constructor_detection_test,
     unresolved_signature_arguments_fall_back_to_void) {
  using arguments = typename detail::constructor_signature_arguments<
      matrix::constructor_two_values, unresolved_argument_category,
      std::make_index_sequence<2>>::type;
  static_assert(std::is_void_v<arguments>);
}
#endif

TEST(constructor_detection_test, traits_limit_only_the_public_search) {
  using target = matrix::constructor_trait_limited;
  static_assert(constructor_detection<target>::arity == 2);
  static_assert(constructor_detection<target>::kind ==
                detail::constructor_kind::concrete);

#if defined(_MSC_VER)
  static_assert(
      detail::constructor_detection_msvc<target, detail::constructor_shape,
                                         detail::list_initialization,
                                         3>::arity == 3);
#else
  static_assert(
      detail::constructor_detection<target, detail::constructor_shape,
                                    detail::list_initialization, 3>::arity ==
      3);
#endif
}

TEST(constructor_detection_test, public_detection_defaults_to_shape) {
  static_assert(
      std::is_same_v<constructor_detection<matrix::constructor_two_values>,
                     constructor_detection<matrix::constructor_two_values,
                                           detail::constructor_shape>>);
}

TEST(constructor_detection_test, auto_construction_uses_public_selection) {
  container<> container;
  EXPECT_EQ(
      container.construct<matrix::constructor_public_selected_consumer>().value,
      1);
}

} // namespace dingo
