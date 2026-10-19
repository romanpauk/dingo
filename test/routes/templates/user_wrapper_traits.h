//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

// The wrapper and storage traits that make the user wrappers of the route
// probes work with the current implementation.

#include "probe_types.h"

#include <dingo/storage/type_storage_traits.h>
#include <dingo/type/type_conversion_traits.h>
#include <dingo/type/type_traits.h>

#include <type_traits>

namespace dingo {
namespace probe_detail {
template <typename T, template <typename...> class Template>
struct is_instance : std::false_type {};

template <template <typename...> class Template, typename... Args>
struct is_instance<Template<Args...>, Template> : std::true_type {};
} // namespace probe_detail

template <typename T> struct type_traits<user_shared<T>> {
  static constexpr bool enabled = true;
  static constexpr bool is_pointer_like = true;
  static constexpr bool is_value_borrowable = true;
  static constexpr bool is_owning_handle = true;

  template <typename> static constexpr bool is_handle_rebindable = true;

  using value_type = T;

  template <typename U> using rebind_t = user_shared<U>;

  template <typename Target>
  static constexpr bool is_rebindable =
      probe_detail::is_instance<std::remove_cv_t<Target>, user_shared>::value;

  static T *get(user_shared<T> &wrapper) { return wrapper.get(); }
  static const T *get(const user_shared<T> &wrapper) { return wrapper.get(); }
  static T &borrow(user_shared<T> &wrapper) { return *wrapper.get(); }
  static const T &borrow(const user_shared<T> &wrapper) {
    return *wrapper.get();
  }
  static bool empty(const user_shared<T> &wrapper) { return !wrapper.get(); }
  static void reset(user_shared<T> &wrapper) { wrapper.reset(); }

  // A wrapped wrapper is made through its own traits, as the std handles do;
  // a plain new T would leave it empty.
  template <typename... Args> static user_shared<T> make(Args &&...args) {
    if constexpr (type_traits<T>::enabled && !std::is_pointer_v<T>)
      return user_shared<T>(std::make_shared<T>(
          type_traits<T>::make(std::forward<Args>(args)...)));
    else if constexpr (std::is_constructible_v<T, Args...>)
      return user_shared<T>(std::make_shared<T>(std::forward<Args>(args)...));
    else
      return user_shared<T>(
          std::shared_ptr<T>(new T{std::forward<Args>(args)...}));
  }

  static user_shared<T> from_pointer(T *ptr) {
    return user_shared<T>(std::shared_ptr<T>(ptr));
  }

  template <typename TargetWrapper, typename Factory, typename Context>
  static TargetWrapper &resolve_type(Factory &factory, Context &context,
                                     type_descriptor, type_descriptor) {
    return factory.template resolve<TargetWrapper>(context);
  }
};

template <typename T> struct type_traits<user_unique<T>> {
  static constexpr bool enabled = true;
  static constexpr bool is_pointer_like = true;
  static constexpr bool is_value_borrowable = true;
  static constexpr bool is_owning_handle = true;

  template <typename> static constexpr bool is_handle_rebindable = true;

  using value_type = T;

  template <typename U> using rebind_t = user_unique<U>;

  template <typename Target>
  static constexpr bool is_rebindable = std::is_same_v<Target, rebind_t<T>>;

  static T *get(user_unique<T> &wrapper) { return wrapper.get(); }
  static const T *get(const user_unique<T> &wrapper) { return wrapper.get(); }
  static T &borrow(user_unique<T> &wrapper) { return *wrapper.get(); }
  static const T &borrow(const user_unique<T> &wrapper) {
    return *wrapper.get();
  }
  static bool empty(const user_unique<T> &wrapper) { return !wrapper.get(); }
  static void reset(user_unique<T> &wrapper) { wrapper.reset(); }

  template <typename... Args> static user_unique<T> make(Args &&...args) {
    if constexpr (type_traits<T>::enabled && !std::is_pointer_v<T>)
      return user_unique<T>(std::unique_ptr<T>(
          new T(type_traits<T>::make(std::forward<Args>(args)...))));
    else if constexpr (std::is_constructible_v<T, Args...>)
      return user_unique<T>(
          std::unique_ptr<T>(new T(std::forward<Args>(args)...)));
    else
      return user_unique<T>(
          std::unique_ptr<T>(new T{std::forward<Args>(args)...}));
  }

  static user_unique<T> from_pointer(T *ptr) {
    return user_unique<T>(std::unique_ptr<T>(ptr));
  }

  template <typename TargetWrapper, typename Factory, typename Context>
  static TargetWrapper &resolve_type(Factory &factory, Context &context,
                                     type_descriptor requested_type,
                                     type_descriptor registered_type) {
    if constexpr (std::is_same_v<TargetWrapper, rebind_t<T>>) {
      return factory.resolve(context);
    } else {
      throw detail::make_type_not_convertible_exception(
          requested_type, registered_type, context);
    }
  }
};

template <typename T> struct type_traits<user_optional<T>> {
  static constexpr bool enabled = true;
  static constexpr bool is_pointer_like = false;
  static constexpr bool is_value_borrowable = true;
  static constexpr bool is_owning_handle = false;

  template <typename> static constexpr bool is_handle_rebindable = false;

  using value_type = T;

  template <typename U> using rebind_t = user_optional<U>;

  template <typename Target>
  static constexpr bool is_rebindable = std::is_same_v<Target, rebind_t<T>>;

  static T *get(user_optional<T> &wrapper) { return wrapper.get(); }
  static const T *get(const user_optional<T> &wrapper) { return wrapper.get(); }
  static T &borrow(user_optional<T> &wrapper) { return *wrapper.get(); }
  static const T &borrow(const user_optional<T> &wrapper) {
    return *wrapper.get();
  }
  static bool empty(const user_optional<T> &wrapper) {
    return !wrapper.has_value();
  }
  static void reset(user_optional<T> &wrapper) { wrapper.reset(); }

  template <typename Value> static user_optional<T> wrap(Value &&value) {
    return user_optional<T>(std::in_place, std::forward<Value>(value));
  }

  static user_optional<T> make_empty() { return {}; }

  template <typename... Args> static user_optional<T> make(Args &&...args) {
    if constexpr (type_traits<T>::enabled && !std::is_pointer_v<T>)
      return user_optional<T>(
          std::in_place, type_traits<T>::make(std::forward<Args>(args)...));
    else if constexpr (std::is_constructible_v<T, Args...>)
      return user_optional<T>(std::in_place, std::forward<Args>(args)...);
    else
      return user_optional<T>(std::in_place, T{std::forward<Args>(args)...});
  }

  template <typename TargetWrapper, typename Factory, typename Context>
  static TargetWrapper &resolve_type(Factory &factory, Context &context,
                                     type_descriptor requested_type,
                                     type_descriptor registered_type) {
    if constexpr (std::is_same_v<TargetWrapper, rebind_t<T>>) {
      return factory.resolve(context);
    } else {
      throw detail::make_type_not_convertible_exception(
          requested_type, registered_type, context);
    }
  }
};

// What each scope publishes for a user wrapper is written the way the built-in
// cell of the matching handle form is: user_shared like std::shared_ptr,
// user_unique like std::unique_ptr, user_optional like std::optional. The
// stored handle and the handles it wraps are published through rebind_leaf, so
// a wrapped wrapper is reachable as it is for the std handles.
namespace probe_detail {
template <typename Handle, typename U> struct handle_cell {
private:
  using rebound = detail::wrapper_rebind_leaf_t<Handle, U>;
  using types = detail::wrapper_storage_types<rebound>;

public:
  using rebound_type = rebound;
  using value_types = type_list_unique_t<
      type_list_cat_t<type_list<U>, typename types::copyable_value_types>>;
  using lvalue_reference_types = typename types::lvalue_reference_types;
  using pointer_types = typename types::pointer_types;
};
} // namespace probe_detail

template <typename T, typename U>
struct storage_traits<unique, user_shared<T>, U> {
  static constexpr bool enabled = true;
  static constexpr bool is_stable = false;

  using value_types = type_list<>;
  using lvalue_reference_types = type_list<>;
  using rvalue_reference_types =
      type_list<detail::wrapper_rebind_leaf_t<user_shared<T>, U> &&>;
  using pointer_types = type_list<>;
};

template <typename T, typename U>
struct storage_traits<shared, user_shared<T>, U> {
  static constexpr bool enabled = true;
  static constexpr bool is_stable = true;

  using cell = probe_detail::handle_cell<user_shared<T>, U>;
  using value_types = typename cell::value_types;
  using lvalue_reference_types = typename cell::lvalue_reference_types;
  using rvalue_reference_types = type_list<>;
  using pointer_types = typename cell::pointer_types;
};

template <typename T, typename U>
struct storage_traits<external, user_shared<T>, U> {
  static constexpr bool enabled = true;
  static constexpr bool is_stable = true;

  using cell = probe_detail::handle_cell<user_shared<T>, U>;
  using value_types = typename cell::value_types;
  using lvalue_reference_types = typename cell::lvalue_reference_types;
  using rvalue_reference_types = type_list<>;
  using pointer_types = typename cell::pointer_types;
};

template <typename T, typename U>
struct storage_traits<unique, user_unique<T>, U> {
  static constexpr bool enabled = true;
  static constexpr bool is_stable = false;

  using value_types = type_list<>;
  using lvalue_reference_types = type_list<>;
  using rvalue_reference_types =
      type_list<detail::wrapper_rebind_leaf_t<user_unique<T>, U> &&,
                user_shared<detail::wrapper_rebind_leaf_t<T, U>> &&>;
  using pointer_types = type_list<>;
};

template <typename T, typename U>
struct storage_traits<shared, user_unique<T>, U> {
  static constexpr bool enabled = true;
  static constexpr bool is_stable = true;

  using cell = probe_detail::handle_cell<user_unique<T>, U>;
  using value_types = type_list<U>;
  using lvalue_reference_types = typename cell::lvalue_reference_types;
  using rvalue_reference_types = type_list<>;
  using pointer_types = typename cell::pointer_types;
};

template <typename T, typename U>
struct storage_traits<external, user_unique<T>, U> {
  static constexpr bool enabled = true;
  static constexpr bool is_stable = true;

  using cell = probe_detail::handle_cell<user_unique<T>, U>;
  using value_types = type_list<U>;
  using lvalue_reference_types = typename cell::lvalue_reference_types;
  using rvalue_reference_types = type_list<>;
  using pointer_types = typename cell::pointer_types;
};

template <typename T, typename U>
struct storage_traits<unique, user_optional<T>, U> {
  static constexpr bool enabled = true;
  static constexpr bool is_stable = false;

  using value_types = type_list<>;
  using lvalue_reference_types = type_list<>;
  using rvalue_reference_types = type_list<user_optional<T> &&>;
  using pointer_types = type_list<>;
};

template <typename T, typename U>
struct storage_traits<shared, user_optional<T>, U> {
  static constexpr bool enabled = true;
  static constexpr bool is_stable = true;

  using value_types = type_list<U>;
  using lvalue_reference_types =
      type_list<U &, exact_lookup<user_optional<T>> &>;
  using rvalue_reference_types = type_list<>;
  using pointer_types = type_list<U *, exact_lookup<user_optional<T>> *>;
};

template <typename T, typename U>
struct storage_traits<external, user_optional<T>, U> {
  static constexpr bool enabled = true;
  static constexpr bool is_stable = true;

  using value_types = type_list<U>;
  using lvalue_reference_types =
      type_list<U &, exact_lookup<user_optional<T>> &>;
  using rvalue_reference_types = type_list<>;
  using pointer_types = type_list<U *, exact_lookup<user_optional<T>> *>;
};

template <typename T, typename U>
struct type_conversion_traits<
    user_shared<U>, user_shared<T>,
    std::enable_if_t<std::is_convertible_v<T *, U *>>> {
  template <typename Source>
  using required_access =
      std::conditional_t<std::is_rvalue_reference_v<Source>, consume, borrow>;

  static user_shared<U> convert(const user_shared<T> &source) {
    return user_shared<U>(std::static_pointer_cast<U>(source.native()));
  }

  static user_shared<U> convert(user_shared<T> &&source) {
    return user_shared<U>(
        std::static_pointer_cast<U>(std::move(source).native()));
  }
};
} // namespace dingo
