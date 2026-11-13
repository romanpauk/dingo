//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/base/config.h>

#include <cstdint>
#include <type_traits>

namespace dingo {
namespace detail {
// A set of the values of an enum, as one bit for each; the values are small
// and count from zero.
template <typename Enum> class flags {
  static_assert(std::is_enum_v<Enum>);

public:
  constexpr flags() = default;

  // The set of the values given.
  template <typename... Values> static constexpr flags of(Values... values) {
    flags set;
    (set.set(values), ...);
    return set;
  }

  // Adds a value, where it holds.
  constexpr flags &set(Enum value, bool holds = true) {
    if (holds) {
      bits_ |= bit(value);
    }
    return *this;
  }

  constexpr bool has(Enum value) const { return (bits_ & bit(value)) != 0; }

  friend constexpr flags operator|(flags lhs, flags rhs) {
    flags set;
    set.bits_ = lhs.bits_ | rhs.bits_;
    return set;
  }

private:
  static constexpr std::uint32_t bit(Enum value) {
    return std::uint32_t(1) << static_cast<unsigned>(value);
  }

  std::uint32_t bits_ = 0;
};
} // namespace detail
} // namespace dingo
