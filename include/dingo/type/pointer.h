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

#include <type_traits>

namespace dingo {
template <typename T> struct type_traits<T *> {
  static constexpr bool enabled = true;
  static constexpr bool is_pointer_like = true;
  static constexpr bool is_value_borrowable = true;
  static constexpr bool is_owning_handle = false;

  template <typename Target>
  static constexpr bool is_handle_rebindable =
      std::is_constructible_v<Target, T *>;

  using value_type = T;

  template <typename U> using rebind_t = U *;

  template <typename Target>
  static constexpr bool is_rebindable = std::is_same_v<Target, rebind_t<T>>;

  static T *get(T *ptr) { return ptr; }
  static T &borrow(T *ptr) { return *ptr; }
  static bool empty(T *ptr) { return ptr == nullptr; }
  static void reset(T *&ptr) { ptr = nullptr; }
  static T *from_pointer(T *ptr) { return ptr; }

  template <typename TargetType, typename Factory, typename Context>
  static TargetType resolve_type(Factory &factory, Context &context,
                                 type_descriptor requested_type,
                                 type_descriptor registered_type) {
    if constexpr (std::is_same_v<TargetType, rebind_t<T>>)
      return factory.resolve(context);
    else
      throw detail::make_type_not_convertible_exception(
          requested_type, registered_type, context);
  }
};

template <> struct type_traits<void *> {
  static constexpr bool enabled = true;
  static constexpr bool is_pointer_like = true;
  static constexpr bool is_value_borrowable = false;
  static constexpr bool is_owning_handle = false;

  template <typename Target>
  static constexpr bool is_handle_rebindable =
      std::is_constructible_v<Target, void *>;

  using value_type = void;

  template <typename U> using rebind_t = U *;

  template <typename Target>
  static constexpr bool is_rebindable = std::is_same_v<Target, rebind_t<void>>;

  static void *get(void *ptr) { return ptr; }
  static bool empty(void *ptr) { return ptr == nullptr; }
  static void reset(void *&ptr) { ptr = nullptr; }
  static void *from_pointer(void *ptr) { return ptr; }

  template <typename TargetType, typename Factory, typename Context>
  static TargetType resolve_type(Factory &factory, Context &context,
                                 type_descriptor requested_type,
                                 type_descriptor registered_type) {
    if constexpr (std::is_same_v<TargetType, rebind_t<void>>)
      return factory.resolve(context);
    else
      throw detail::make_type_not_convertible_exception(
          requested_type, registered_type, context);
  }
};

template <> struct type_traits<const void *> {
  static constexpr bool enabled = true;
  static constexpr bool is_pointer_like = true;
  static constexpr bool is_value_borrowable = false;
  static constexpr bool is_owning_handle = false;

  template <typename Target>
  static constexpr bool is_handle_rebindable =
      std::is_constructible_v<Target, const void *>;

  using value_type = const void;

  template <typename U> using rebind_t = U *;

  template <typename Target>
  static constexpr bool is_rebindable =
      std::is_same_v<Target, rebind_t<const void>>;

  static const void *get(const void *ptr) { return ptr; }
  static bool empty(const void *ptr) { return ptr == nullptr; }
  static void reset(const void *&ptr) { ptr = nullptr; }
  static const void *from_pointer(const void *ptr) { return ptr; }

  template <typename TargetType, typename Factory, typename Context>
  static TargetType resolve_type(Factory &factory, Context &context,
                                 type_descriptor requested_type,
                                 type_descriptor registered_type) {
    if constexpr (std::is_same_v<TargetType, rebind_t<const void>>)
      return factory.resolve(context);
    else
      throw detail::make_type_not_convertible_exception(
          requested_type, registered_type, context);
  }
};
} // namespace dingo
