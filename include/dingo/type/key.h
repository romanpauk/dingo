//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/base/config.h>
#include <dingo/base/meta.h>
#include <dingo/base/none.h>
#include <dingo/base/selected.h>

#include <type_traits>
#include <utility>

namespace dingo {
template <typename T, auto... Values> struct key_type;
template <typename T, typename Interface = void> struct key_value;

namespace detail {
// The value V of a key_type<T, V> makes the key: V itself, or T{V}.
template <typename T, auto Value>
inline constexpr bool is_fixed_key_value_constructible_v =
    std::is_same_v<std::remove_cv_t<decltype(Value)>, T> ||
    is_brace_constructible_v<T, Value>;

template <typename T, auto Value> constexpr T make_fixed_key_value() {
  static_assert(is_fixed_key_value_constructible_v<T, Value>,
                "dingo::key_type<T, V> requires V to be usable as T{V}");
  if constexpr (std::is_same_v<std::remove_cv_t<decltype(Value)>, T>) {
    return Value;
  } else {
    return T{Value};
  }
}
} // namespace detail

template <typename T, auto... Values> struct key_type {
  static_assert(sizeof...(Values) <= 1,
                "dingo::key_type<T, V> accepts at most one value");
  static_assert((detail::is_fixed_key_value_constructible_v<T, Values> && ...),
                "dingo::key_type<T, V> requires V to be usable as T{V}");

  using type = T;
  template <typename U> using rebind_t = key_type<U>;

  static constexpr bool has_value = sizeof...(Values) == 1;
};

template <typename T, typename Interface> struct key_value {
  using type = T;
  using interface_type = Interface;

  template <typename U> using rebind_t = key_value<U, Interface>;

  template <typename U,
            typename = std::enable_if_t<std::is_constructible_v<T, U &&>>>
  explicit key_value(U &&runtime_value)
      : runtime_value_(std::forward<U>(runtime_value)) {}

  const T &value() const { return runtime_value_; }

private:
  T runtime_value_;
};

template <typename T> key_value(T &&) -> key_value<std::decay_t<T>>;

namespace detail {
template <typename T> struct is_key_type : std::false_type {};
template <typename T, auto... Values>
struct is_key_type<key_type<T, Values...>> : std::true_type {};
template <typename T>
inline constexpr bool is_key_type_v = is_key_type<std::decay_t<T>>::value;

template <typename T> struct is_key_value : std::false_type {};

template <typename T, auto Value>
struct is_key_value<key_type<T, Value>>
    : std::bool_constant<!std::is_void_v<T>> {};

template <typename T>
inline constexpr bool is_key_value_v = is_key_value<std::decay_t<T>>::value;

template <typename T, auto Value>
struct is_value_selector<key_type<T, Value>> : std::true_type {
  using type = T;
  static T make() { return make_fixed_key_value<T, Value>(); }
};

template <typename T>
struct is_runtime_registration_key_arg : std::false_type {};

template <typename T, typename Interface>
struct is_runtime_registration_key_arg<::dingo::key_value<T, Interface>>
    : std::bool_constant<!std::is_void_v<T>> {};

template <typename T>
inline constexpr bool is_runtime_registration_key_arg_v =
    is_runtime_registration_key_arg<std::decay_t<T>>::value;

template <typename... Args>
inline constexpr bool are_runtime_registration_key_args_v =
    (is_runtime_registration_key_arg_v<Args> && ...);
} // namespace detail

} // namespace dingo
