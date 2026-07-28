//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/type/type_conversion_traits.h>

#include <utility>

namespace dingo::resolution_test {
struct move_target {
  explicit move_target(int initial_value = 23) : value(initial_value) {}

  move_target(const move_target &) = delete;
  move_target &operator=(const move_target &) = delete;

  move_target(move_target &&other) noexcept
      : value(std::exchange(other.value, -1)) {}
  move_target &operator=(move_target &&) = delete;

  int value;
};

struct secondary {
  virtual ~secondary() = default;
};

struct move_source : move_target, secondary {
  move_source() = default;
};

struct copy_target {
  explicit copy_target(int initial_value = 23) : value(initial_value) {}

  int value;
};

struct copy_source : copy_target, secondary {
  copy_source() = default;
};

struct copy_only_conversion_result {
  int value;
};

struct copy_only_conversion_target {
  explicit copy_only_conversion_target(copy_only_conversion_result result)
      : value(result.value) {}

  copy_only_conversion_target(const copy_only_conversion_target &) = default;
  copy_only_conversion_target(copy_only_conversion_target &&) = delete;

  int value;
};

struct copy_only_conversion_source : copy_only_conversion_target {
  copy_only_conversion_source()
      : copy_only_conversion_target(copy_only_conversion_result{41}) {}
};

struct copy_only {
  explicit copy_only(int initial_value = 31) : value(initial_value) {}

  copy_only(const copy_only &other) : value(other.value) { ++copies; }
  copy_only(copy_only &&) = delete;

  inline static int copies = 0;
  int value;
};
} // namespace dingo::resolution_test

namespace dingo {
template <>
struct type_conversion_traits<resolution_test::move_target,
                              resolution_test::move_source> {
  template <typename> using required_access = borrow;

  static resolution_test::move_target
  convert(const resolution_test::move_source &source) {
    return resolution_test::move_target(source.value);
  }
};

template <>
struct type_conversion_traits<resolution_test::copy_target,
                              resolution_test::copy_source> {
  template <typename> using required_access = borrow;

  static resolution_test::copy_target
  convert(const resolution_test::copy_source &) {
    return resolution_test::copy_target(47);
  }
};

template <>
struct type_conversion_traits<resolution_test::copy_only_conversion_target,
                              resolution_test::copy_only_conversion_source> {
  template <typename> using required_access = borrow;

  static resolution_test::copy_only_conversion_result
  convert(const resolution_test::copy_only_conversion_source &) {
    return {53};
  }
};
} // namespace dingo
