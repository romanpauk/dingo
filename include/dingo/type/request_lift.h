//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/base/complete_type.h>
#include <dingo/base/exceptions.h>
#include <dingo/type/type_traits.h>

#include <type_traits>
#include <utility>

namespace dingo {
namespace detail {

template <typename T, bool Complete = dingo::is_complete<T>::value>
struct liftable_value : std::false_type {};

template <typename T>
struct liftable_value<T, true>
    : std::bool_constant<!std::is_abstract_v<T> &&
                         (std::is_move_constructible_v<T> ||
                          std::is_copy_constructible_v<T>)> {};

template <typename T>
inline constexpr bool liftable_value_v = liftable_value<T>::value;

template <typename T, typename = void>
struct request_has_value_type : std::false_type {};

template <typename T>
struct request_has_value_type<T,
                              std::void_t<typename type_traits<T>::value_type>>
    : std::true_type {};

template <typename T>
inline constexpr bool request_has_value_type_v =
    request_has_value_type<T>::value;

template <typename Wrapper, typename Value, typename = void>
struct can_wrap_value : std::false_type {};

template <typename Wrapper, typename Value>
struct can_wrap_value<
    Wrapper, Value,
    std::void_t<decltype(type_traits<Wrapper>::wrap(std::declval<Value>()))>>
    : std::true_type {};

template <typename Wrapper, typename Value, typename = void>
struct can_make_value : std::false_type {};

template <typename Wrapper, typename Value>
struct can_make_value<
    Wrapper, Value,
    std::void_t<decltype(type_traits<Wrapper>::make(std::declval<Value>()))>>
    : std::true_type {};

template <typename Wrapper, typename Value,
          bool Enabled =
              liftable_value_v<typename type_traits<Wrapper>::value_type>,
          typename = void>
struct can_wrap_value_pointer : std::false_type {};

template <typename Wrapper, typename Value>
struct can_wrap_value_pointer<
    Wrapper, Value, true,
    std::void_t<typename type_traits<Wrapper>::value_type,
                decltype(type_traits<Wrapper>::from_pointer(
                    new typename type_traits<Wrapper>::value_type(
                        std::declval<Value>())))>> : std::true_type {};

template <typename T, typename = void> struct request_lift {
  static constexpr bool enabled = false;
};

template <typename T>
struct request_lift<
    T,
    std::enable_if_t<
        !std::is_reference_v<T> && type_traits<std::remove_cv_t<T>>::enabled &&
        request_has_value_type_v<std::remove_cv_t<T>> &&
        !type_traits<std::remove_cv_t<T>>::is_array &&
        !is_variant_type_v<std::remove_cv_t<T>> &&
        !std::is_pointer_v<
            typename type_traits<std::remove_cv_t<T>>::value_type> &&
        liftable_value_v<
            typename type_traits<std::remove_cv_t<T>>::value_type> &&
        (!type_traits<std::remove_cv_t<T>>::is_pointer_like ||
         type_traits<std::remove_cv_t<T>>::can_take) &&
        (can_wrap_value<
             std::remove_cv_t<T>,
             typename type_traits<std::remove_cv_t<T>>::value_type &&>::value ||
         can_wrap_value_pointer<
             std::remove_cv_t<T>,
             typename type_traits<std::remove_cv_t<T>>::value_type &&>::value ||
         can_make_value<std::remove_cv_t<T>,
                        typename type_traits<std::remove_cv_t<T>>::value_type
                            &&>::value)>> {
  using type = std::remove_cv_t<T>;
  using value_type = typename type_traits<type>::value_type;
  using traits = type_traits<type>;
  // Value wrappers may copy or move their value. A wrapper that takes
  // ownership must move from a consumed route, so a borrowed value can never
  // become an owner.
  using inner_type =
      std::conditional_t<traits::can_take, value_type &&, value_type>;
  static constexpr bool enabled = true;

  // Route operations: copy the contained value out, take ownership, wrap an
  // existing value, adopt a pointer, or construct a fresh wrapper.
  static constexpr bool copy_out = traits::can_copy_out;
  static constexpr bool take = traits::can_take;
  static constexpr bool wrap_value = can_wrap_value<type, value_type &&>::value;
  static constexpr bool adopt_pointer =
      can_wrap_value_pointer<type, value_type &&>::value;
  static constexpr bool construct_value =
      can_make_value<type, value_type &&>::value;

  static_assert(!std::is_same_v<type, value_type>,
                "request wrapper must not resolve to itself");

  template <typename Value> static type make(Value &&value) {
    if constexpr (can_wrap_value<type, Value &&>::value) {
      return type_traits<type>::wrap(std::forward<Value>(value));
    } else if constexpr (can_wrap_value_pointer<type, Value &&>::value) {
      return type_traits<type>::from_pointer(
          new value_type(std::forward<Value>(value)));
    } else {
      return type_traits<type>::make(std::forward<Value>(value));
    }
  }
};

template <typename T>
inline constexpr bool is_request_liftable_v = request_lift<T>::enabled;

// Tries the direct request first and falls back to the request lift only when
// the binding publishes no route for the wrapper. A conversion failure still
// propagates; only a missing route raises the distinct exception. If the lift
// itself cannot resolve its value, the original route error is the useful one.
template <typename T, typename R, typename Direct, typename Lift>
R resolve_request_or_lift(Direct &&direct, Lift &&lift) {
  if constexpr (!request_lift<std::remove_cv_t<T>>::enabled) {
    return std::forward<Direct>(direct)();
  } else {
    try {
      return std::forward<Direct>(direct)();
    } catch (const request_route_not_found_exception &route_error) {
      try {
        return std::forward<Lift>(lift)();
      } catch (...) {
        throw route_error;
      }
    }
  }
}

} // namespace detail
} // namespace dingo
