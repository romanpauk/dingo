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

// The copies and moves a row expects, the range the baseline containers make.
// The next implementation constructs a stored object in place, every layer
// around its leaf, as the behavior change
// stored_objects_are_constructed_in_place of the spec declares, and moves a
// consumed object only as far as the request needs, as
// consuming_moves_minimally declares: every container serves the object its
// binding constructed, so it makes the fewest copies of the range and moves
// nothing, where the baseline containers move a stored optional or a consumed
// value one to three times.
template <std::size_t CopyMinimum, std::size_t CopyMaximum,
          std::size_t MoveMinimum, std::size_t MoveMaximum>
struct lifetime_bounds {
#if !defined(DINGO_MATRIX_BASELINE)
  static constexpr std::size_t copy_minimum = CopyMinimum;
  static constexpr std::size_t copy_maximum = CopyMinimum;
  static constexpr std::size_t move_minimum = 0;
  static constexpr std::size_t move_maximum = 0;
#else
  static constexpr std::size_t copy_minimum = CopyMinimum;
  static constexpr std::size_t copy_maximum = CopyMaximum;
  static constexpr std::size_t move_minimum = MoveMinimum;
  static constexpr std::size_t move_maximum = MoveMaximum;
#endif
};

template <typename Check, typename Value, std::size_t Constructor,
          std::size_t CopyMinimum, std::size_t CopyMaximum,
          std::size_t MoveMinimum, std::size_t MoveMaximum>
struct lifetime {
  using bounds =
      lifetime_bounds<CopyMinimum, CopyMaximum, MoveMinimum, MoveMaximum>;

  static void before() { Value::clear_stats(); }

  template <typename Container> static void check(Container &container) {
    Check::check(container);
  }

  static void after() {
    ASSERT_EQ(Value::constructor_count(), Constructor);
    if constexpr (bounds::copy_minimum == bounds::copy_maximum) {
      ASSERT_EQ(Value::copy_constructor_count(), bounds::copy_minimum);
    } else {
      ASSERT_GE(Value::copy_constructor_count(), bounds::copy_minimum);
      ASSERT_LE(Value::copy_constructor_count(), bounds::copy_maximum);
    }
    if constexpr (bounds::move_minimum == bounds::move_maximum) {
      ASSERT_EQ(Value::move_constructor_count(), bounds::move_minimum);
    } else {
      ASSERT_GE(Value::move_constructor_count(), bounds::move_minimum);
      ASSERT_LE(Value::move_constructor_count(), bounds::move_maximum);
    }
    ASSERT_EQ(Value::destructor_count(), Value::total_instances());
  }
};

} // namespace dingo::matrix::resolution
