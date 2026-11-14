//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

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

struct copy_only {
  explicit copy_only(int initial_value = 31) : value(initial_value) {}

  copy_only(const copy_only &other) : value(other.value) { ++copies; }
  copy_only(copy_only &&) = delete;

  inline static int copies = 0;
  int value;
};
} // namespace dingo::resolution_test
