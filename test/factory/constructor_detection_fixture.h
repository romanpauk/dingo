//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/factory/constructor_detection.h>
#include <dingo/registration/constructor.h>
#include <dingo/type/auto_constructible.h>

#include <type_traits>

namespace dingo::fixture {

struct dependency_move_only {
  dependency_move_only() = default;
  dependency_move_only(const dependency_move_only &) = delete;
  dependency_move_only(dependency_move_only &&) = default;

  int marker() const { return 3; }
};

struct constructor_two_values {
  constructor_two_values(int init_integer, float init_real)
      : integer(init_integer), real(init_real) {}

  int integer;
  float real;
};

struct constructor_defaulted {};

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

} // namespace dingo::fixture

namespace dingo {

template <>
struct constructor_detection_traits<fixture::constructor_trait_limited> {
  static constexpr size_t max_arity = 2;
};

template <>
struct is_auto_constructible<fixture::constructor_public_selected_dependency>
    : std::true_type {};

} // namespace dingo
