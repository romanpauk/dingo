//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/base/exceptions.h>
#include <dingo/base/type_descriptor.h>
#include <dingo/type/array_construction.h>
#include <dingo/type/primary.h>

#include <memory>
#include <type_traits>
#include <utility>

namespace dingo {
template <typename Array, typename Deleter>
struct type_traits<std::unique_ptr<Array, Deleter>,
                   std::enable_if_t<std::is_array_v<Array> &&
                                    (std::extent_v<Array, 0> == 0)>> {
  static constexpr bool enabled = true;
  static constexpr bool is_pointer_like = true;
  static constexpr bool is_value_borrowable = false;
  static constexpr bool is_owning_handle = true;

  template <typename Target>
  static constexpr bool is_handle_rebindable =
      std::is_constructible_v<Target, std::unique_ptr<Array, Deleter>>;

  using value_type = std::remove_extent_t<Array>;

  template <typename U>
  using rebind_t = std::unique_ptr<U[], std::default_delete<U[]>>;

  template <typename Target>
  static constexpr bool is_rebindable =
      std::is_same_v<Target, rebind_t<value_type>>;

  static value_type *get(std::unique_ptr<Array, Deleter> &wrapper) {
    return wrapper.get();
  }

  static const value_type *get(const std::unique_ptr<Array, Deleter> &wrapper) {
    return wrapper.get();
  }

  static bool empty(const std::unique_ptr<Array, Deleter> &wrapper) {
    return wrapper.get() == nullptr;
  }

  static void reset(std::unique_ptr<Array, Deleter> &wrapper) {
    wrapper.reset();
  }

  template <typename... Args>
  static std::unique_ptr<Array, Deleter> make(Args &&...args) {
    return std::unique_ptr<Array, Deleter>(
        detail::make_dynamic_array<value_type>(std::forward<Args>(args)...));
  }

  static std::unique_ptr<Array, Deleter> from_pointer(value_type *ptr) {
    return std::unique_ptr<Array, Deleter>(ptr);
  }

  template <typename TargetType, typename Factory, typename Context>
  static TargetType &resolve_type(Factory &factory, Context &context,
                                  type_descriptor requested_type,
                                  type_descriptor registered_type) {
    if constexpr (std::is_same_v<TargetType, rebind_t<value_type>>)
      return factory.resolve(context);
    else
      throw detail::make_type_not_convertible_exception(
          requested_type, registered_type, context);
  }
};

template <typename T, typename Deleter>
struct type_traits<std::unique_ptr<T, Deleter>,
                   std::enable_if_t<!std::is_array_v<T>>> {
  static constexpr bool enabled = true;
  static constexpr bool is_pointer_like = true;
  static constexpr bool is_value_borrowable = true;
  static constexpr bool is_owning_handle = true;

  template <typename Target>
  static constexpr bool is_handle_rebindable =
      std::is_constructible_v<Target, std::unique_ptr<T, Deleter>>;

  using value_type = T;

  template <typename U>
  using rebind_t = std::unique_ptr<U, std::default_delete<U>>;

  template <typename Target>
  static constexpr bool is_rebindable = std::is_same_v<Target, rebind_t<T>>;

  static T *get(std::unique_ptr<T, Deleter> &wrapper) { return wrapper.get(); }

  static const T *get(const std::unique_ptr<T, Deleter> &wrapper) {
    return wrapper.get();
  }

  static T &borrow(std::unique_ptr<T, Deleter> &wrapper) { return *wrapper; }

  static const T &borrow(const std::unique_ptr<T, Deleter> &wrapper) {
    return *wrapper;
  }

  static bool empty(const std::unique_ptr<T, Deleter> &wrapper) {
    return wrapper.get() == nullptr;
  }

  static void reset(std::unique_ptr<T, Deleter> &wrapper) { wrapper.reset(); }

  template <typename... Args>
  static std::unique_ptr<T, Deleter> make(Args &&...args) {
    if constexpr (type_traits<T>::enabled && !std::is_pointer_v<T>)
      return std::unique_ptr<T, Deleter>(
          new T(detail::make_nested<T>(std::forward<Args>(args)...)));
    // Work around direct-initialization in smart-pointer-backed factories.
    else if constexpr (std::is_constructible_v<T, Args...>)
      return std::unique_ptr<T, Deleter>(new T(std::forward<Args>(args)...));
    else
      return std::unique_ptr<T, Deleter>(new T{std::forward<Args>(args)...});
  }

  static std::unique_ptr<T, Deleter> from_pointer(T *ptr) {
    return std::unique_ptr<T, Deleter>(ptr);
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

template <typename Array>
struct type_traits<std::shared_ptr<Array>,
                   std::enable_if_t<std::is_array_v<Array> &&
                                    (std::extent_v<Array, 0> == 0)>> {
  static constexpr bool enabled = true;
  static constexpr bool is_pointer_like = true;
  static constexpr bool is_value_borrowable = false;
  static constexpr bool is_owning_handle = true;

  template <typename Target>
  static constexpr bool is_handle_rebindable =
      std::is_constructible_v<Target, std::shared_ptr<Array>>;

  using value_type = std::remove_extent_t<Array>;

  template <typename U> using rebind_t = std::shared_ptr<U[]>;

  template <typename Target>
  static constexpr bool is_rebindable =
      std::is_constructible_v<std::remove_cv_t<Target>, std::shared_ptr<Array>>;

  static value_type *get(std::shared_ptr<Array> &wrapper) {
    return wrapper.get();
  }

  static const value_type *get(const std::shared_ptr<Array> &wrapper) {
    return wrapper.get();
  }

  static bool empty(const std::shared_ptr<Array> &wrapper) {
    return wrapper.get() == nullptr;
  }

  static void reset(std::shared_ptr<Array> &wrapper) { wrapper.reset(); }

  template <typename... Args>
  static std::shared_ptr<Array> make(Args &&...args) {
    return std::shared_ptr<Array>(
        detail::make_dynamic_array<value_type>(std::forward<Args>(args)...));
  }

  static std::shared_ptr<Array> from_pointer(value_type *ptr) {
    return std::shared_ptr<Array>(ptr);
  }

  template <typename TargetType, typename Factory, typename Context>
  static TargetType &resolve_type(Factory &factory, Context &context,
                                  type_descriptor, type_descriptor) {
    return factory.template resolve<TargetType>(context);
  }
};

template <typename T>
struct type_traits<std::shared_ptr<T>, std::enable_if_t<!std::is_array_v<T>>> {
  static constexpr bool enabled = true;
  static constexpr bool is_pointer_like = true;
  static constexpr bool is_value_borrowable = true;
  static constexpr bool is_owning_handle = true;

  template <typename Target>
  static constexpr bool is_handle_rebindable =
      std::is_constructible_v<Target, std::shared_ptr<T>>;

  using value_type = T;

  template <typename U> using rebind_t = std::shared_ptr<U>;

  template <typename Target>
  static constexpr bool is_rebindable =
      std::is_constructible_v<std::remove_cv_t<Target>, std::shared_ptr<T>>;

  static T *get(std::shared_ptr<T> &wrapper) { return wrapper.get(); }

  static const T *get(const std::shared_ptr<T> &wrapper) {
    return wrapper.get();
  }

  static T &borrow(std::shared_ptr<T> &wrapper) { return *wrapper; }

  static const T &borrow(const std::shared_ptr<T> &wrapper) { return *wrapper; }

  static bool empty(const std::shared_ptr<T> &wrapper) {
    return wrapper.get() == nullptr;
  }

  static void reset(std::shared_ptr<T> &wrapper) { wrapper.reset(); }

  template <typename... Args> static std::shared_ptr<T> make(Args &&...args) {
    if constexpr (type_traits<T>::enabled && !std::is_pointer_v<T>)
      return std::make_shared<T>(
          detail::make_nested<T>(std::forward<Args>(args)...));
    // Work around direct-initialization in std::make_shared().
    else if constexpr (std::is_constructible_v<T, Args...>)
      return std::make_shared<T>(std::forward<Args>(args)...);
    else
      return std::shared_ptr<T>(new T{std::forward<Args>(args)...});
  }

  static std::shared_ptr<T> from_pointer(T *ptr) {
    return std::shared_ptr<T>(ptr);
  }

  template <typename TargetType, typename Factory, typename Context>
  static TargetType &resolve_type(Factory &factory, Context &context,
                                  type_descriptor, type_descriptor) {
    return factory.template resolve<TargetType>(context);
  }
};
} // namespace dingo
