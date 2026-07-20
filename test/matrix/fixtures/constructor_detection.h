//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include "matrix/fixtures/dependency_shapes.h"

#include <dingo/core/auto_constructible.h>
#include <dingo/factory/constructor.h>
#include <dingo/factory/detail/constructor_detection_msvc.hpp>
#include <dingo/registration/constructor.h>

#include <array>
#include <initializer_list>
#include <memory>
#include <optional>
#include <type_traits>
#include <variant>

namespace dingo::matrix {

using constructor_config = dependency_regular;

template <typename Dependency> struct constructor_dependency {
  explicit constructor_dependency(Dependency) {}
};

using constructor_copy_only_config = dependency_copy_only;
using constructor_move_only_config = dependency_move_only;

struct constructor_two_values {
  constructor_two_values(int init_integer, float init_real)
      : integer(init_integer), real(init_real) {}

  int integer;
  float real;
};

template <typename T> struct constructor_test_wrapper {
  explicit constructor_test_wrapper(T) {}
};

template <typename T> struct constructor_forwarding_wrapper {
  template <typename U,
            typename = std::enable_if_t<std::is_constructible_v<T, U &&>>>
  constructor_forwarding_wrapper(U &&) {}
};

using constructor_nested_forwarding_dependency = constructor_forwarding_wrapper<
    constructor_forwarding_wrapper<constructor_config>>;

struct constructor_nested_forwarding_wrapper {
  explicit constructor_nested_forwarding_wrapper(
      constructor_nested_forwarding_dependency) {}
};

template <typename T> struct constructor_unconstrained_forwarding_wrapper {
  template <typename U> constructor_unconstrained_forwarding_wrapper(U &&) {}
};

using constructor_unconstrained_forwarding_dependency =
    constructor_unconstrained_forwarding_wrapper<constructor_config>;

struct constructor_unconstrained_forwarding_wrapper_consumer {
  explicit constructor_unconstrained_forwarding_wrapper_consumer(
      constructor_unconstrained_forwarding_dependency) {}
};

struct constructor_mixed_wrappers {
  constructor_mixed_wrappers(
      int, constructor_test_wrapper<constructor_config>,
      std::optional<constructor_config>,
      std::variant<constructor_config, constructor_move_only_config>) {}
};

struct constructor_copy_only_value {
  explicit constructor_copy_only_value(constructor_copy_only_config) {}
};

struct constructor_move_only_value {
  explicit constructor_move_only_value(constructor_move_only_config) {}
};

struct constructor_selector {};

struct constructor_incomplete_config;

struct constructor_incomplete_reference {
  explicit constructor_incomplete_reference(constructor_incomplete_config &) {}
};

struct constructor_selected_incomplete_reference {
  explicit constructor_selected_incomplete_reference(
      detail::selected<constructor_incomplete_config &,
                       detail::type_selector<constructor_selector>>) {}
};

struct constructor_aggregate {
  int number;
  constructor_config &config;
};

struct constructor_defaulted {};

struct constructor_generic {
  template <typename... Args> constructor_generic(Args &&...) {}
};

struct constructor_ambiguous {
  explicit constructor_ambiguous(int) {}
  explicit constructor_ambiguous(float) {}
};

struct constructor_explicit {
  explicit constructor_explicit(int) {}
};

struct constructor_initializer_list {
  constructor_initializer_list(std::initializer_list<int>) {}
};

struct constructor_same_arity_overload {
  constructor_same_arity_overload(int, float) {}
  constructor_same_arity_overload(float, int) {}
};

template <typename T, typename Sequence> struct constructor_repeated_arguments;

template <typename T, size_t... Is>
struct constructor_repeated_arguments<T, std::index_sequence<Is...>> {
  using type = type_list<detail::repeated_type<T, Is>...>;
};

using constructor_repeated_int_arguments =
    typename constructor_repeated_arguments<
        int, std::make_index_sequence<DINGO_CONSTRUCTOR_DETECTION_ARGS>>::type;

struct constructor_trait_limited {
  constructor_trait_limited(int, float) {}
  constructor_trait_limited(int, float, double) {}
};

struct constructor_typedef_selected {
  DINGO_CONSTRUCTOR(constructor_typedef_selected(double, const char *)) {}
};

struct constructor_public_selected_dependency {
  DINGO_CONSTRUCTOR(constructor_public_selected_dependency()) : value(1) {}
  explicit constructor_public_selected_dependency(int) : value(2) {}

  int value;
};

struct constructor_public_selected_consumer {
  explicit constructor_public_selected_consumer(
      constructor_public_selected_dependency dependency)
      : value(dependency.value) {}

  int value;
};

#if !defined(_MSC_VER)
struct constructor_detection_portable_backend {
  template <typename T, typename DetectionMode>
  using detection =
      detail::constructor_detection<T, DetectionMode,
                                    detail::list_initialization,
                                    DINGO_CONSTRUCTOR_DETECTION_ARGS>;
};
#endif

struct constructor_detection_msvc_backend {
  template <typename T, typename DetectionMode>
  using detection =
      detail::constructor_detection_msvc<T, DetectionMode,
                                         detail::list_initialization,
                                         DINGO_CONSTRUCTOR_DETECTION_ARGS>;
};

struct constructor_detection_context {
  template <typename T>
  T resolve(construction_scope, constructor_detection_context &) {
    static_assert(std::is_same_v<T, int> || std::is_same_v<T, float>);
    if constexpr (std::is_same_v<T, int>) {
      ++integer_resolutions;
      return 42;
    } else {
      ++float_resolutions;
      return 2.5f;
    }
  }

  int integer_resolutions = 0;
  int float_resolutions = 0;
};

struct constructor_copy_only_context {
  template <typename T>
  T resolve(construction_scope, constructor_copy_only_context &) {
    static_assert(std::is_same_v<T, constructor_copy_only_config>);
    return {};
  }
};

} // namespace dingo::matrix

namespace dingo {

template <>
struct constructor_detection_traits<matrix::constructor_trait_limited> {
  static constexpr size_t max_arity = 2;
};

template <>
struct is_auto_constructible<matrix::constructor_public_selected_dependency>
    : std::true_type {};

} // namespace dingo
