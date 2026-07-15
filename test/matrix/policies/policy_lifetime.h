//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include "matrix/policies/resolution.h"

#include <cstddef>

namespace dingo::matrix::resolution {

template <typename Check, typename Value, std::size_t Constructor,
          std::size_t CopyMinimum, std::size_t CopyMaximum,
          std::size_t MoveMinimum, std::size_t MoveMaximum>
struct lifetime {
  static void before() { Value::clear_stats(); }

  template <typename Container> static void check(Container &container) {
    Check::check(container);
  }

  static void after() {
    ASSERT_EQ(Value::constructor_count(), Constructor);
    if constexpr (CopyMinimum == CopyMaximum) {
      ASSERT_EQ(Value::copy_constructor_count(), CopyMinimum);
    } else {
      ASSERT_GE(Value::copy_constructor_count(), CopyMinimum);
      ASSERT_LE(Value::copy_constructor_count(), CopyMaximum);
    }
    if constexpr (MoveMinimum == MoveMaximum) {
      ASSERT_EQ(Value::move_constructor_count(), MoveMinimum);
    } else {
      ASSERT_GE(Value::move_constructor_count(), MoveMinimum);
      ASSERT_LE(Value::move_constructor_count(), MoveMaximum);
    }
    ASSERT_EQ(Value::destructor_count(), Value::total_instances());
  }
};

} // namespace dingo::matrix::resolution
