//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/base/exceptions.h>
#include <dingo/base/type_descriptor.h>
#include <dingo/type/primary.h>

#include <memory>
#include <optional>
#include <type_traits>
#include <utility>

namespace dingo {
template <typename T> struct type_traits<std::optional<T>> {
  static constexpr bool enabled = true;
  static constexpr bool is_pointer_like = false;
  static constexpr bool is_value_borrowable = true;
  static constexpr bool is_owning_handle = false;

  template <typename> static constexpr bool is_handle_rebindable = false;

  using value_type = T;

  template <typename U> using rebind_t = std::optional<U>;

  template <typename Target>
  static constexpr bool is_rebindable = std::is_same_v<Target, rebind_t<T>>;

  static T *get(std::optional<T> &wrapper) {
    return wrapper ? std::addressof(wrapper.value()) : nullptr;
  }

  static const T *get(const std::optional<T> &wrapper) {
    return wrapper ? std::addressof(wrapper.value()) : nullptr;
  }

  static T &borrow(std::optional<T> &wrapper) { return wrapper.value(); }

  static const T &borrow(const std::optional<T> &wrapper) {
    return wrapper.value();
  }

  static bool empty(const std::optional<T> &wrapper) {
    return !wrapper.has_value();
  }

  static void reset(std::optional<T> &wrapper) { wrapper.reset(); }

  template <typename Value> static std::optional<T> wrap(Value &&value) {
    return std::optional<T>(std::in_place, std::forward<Value>(value));
  }

  static std::optional<T> make_empty() { return std::nullopt; }

  template <typename... Args> static std::optional<T> make(Args &&...args) {
    if constexpr (type_traits<T>::enabled && !std::is_pointer_v<T>)
      return std::optional<T>{
          std::in_place, detail::make_nested<T>(std::forward<Args>(args)...)};
    else if constexpr (std::is_constructible_v<T, Args...>)
      return std::optional<T>{std::in_place, std::forward<Args>(args)...};
    else
      return std::optional<T>{std::in_place, T{std::forward<Args>(args)...}};
  }

  template <typename TargetType, typename Factory, typename Context>
  static TargetType &resolve_type(Factory &factory, Context &context,
                                  type_descriptor requested_type,
                                  type_descriptor registered_type) {
    if constexpr (std::is_same_v<TargetType, rebind_t<T>>)
      return factory.resolve(context);
    else
      throw detail::make_type_not_convertible_exception(
          requested_type, registered_type, context);
  }
};
} // namespace dingo
