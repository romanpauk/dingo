//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <type_traits>
#include <utility>

namespace dingo {
template <typename T, typename = void> struct type_traits {
  static constexpr bool enabled = false;
  static constexpr bool is_pointer_like = false;
  static constexpr bool is_value_borrowable = false;
  static constexpr bool is_owning_handle = false;

  template <typename> static constexpr bool is_handle_rebindable = false;

  template <typename> static constexpr bool is_rebindable = false;
};

template <typename T> struct type_traits<const T> : type_traits<T> {};

template <typename T>
inline constexpr bool is_pointer_like_type_v =
    type_traits<T>::enabled && type_traits<T>::is_pointer_like &&
    !std::is_pointer_v<T>;

namespace detail {
template <typename T, typename... Args> T make_nested(Args &&...args) {
  return type_traits<T>::make(std::forward<Args>(args)...);
}
} // namespace detail
} // namespace dingo
