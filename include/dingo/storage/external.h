//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/base/config.h>

#include <dingo/factory/constructor.h>
#include <dingo/resolution/resolution_operation.h>
#include <dingo/storage/scope_traits.h>
#include <dingo/storage/storage.h>
#include <dingo/storage/type_storage_traits.h>
#include <dingo/type/shape.h>

namespace dingo {
struct external {};

namespace detail {
template <typename Type, typename StoredType, typename Factory,
          typename Conversions>
class external_storage;
} // namespace detail

template <> struct scope_traits<external> {
  static constexpr bool enabled = true;
  static constexpr bool is_stable = true;
  static constexpr bool consumes = false;
  static constexpr materialization_guard guard = materialization_guard::none;
  static constexpr bool retains_unresolved_source = false;

  static constexpr storage_exposure exposed(storage_form) {
    return storage_exposure::value | storage_exposure::lvalue |
           storage_exposure::pointer;
  }

  template <typename Type, typename StoredType, typename Factory,
            typename Conversions>
  using storage =
      detail::external_storage<Type, StoredType, Factory, Conversions>;
};

namespace detail {
// A wrapper given by value is held as its stored type.
template <typename Type>
inline constexpr bool external_converts_v =
    !std::is_reference_v<Type> && !std::is_pointer_v<Type> &&
    !std::is_array_v<Type> && type_traits<Type>::enabled;

// What an external storage holds: a reference or a pointer to the instance it
// was given, or the given value, converted to the stored type of a wrapper.
template <typename Type, typename StoredType>
using external_held_t =
    std::conditional_t<external_converts_v<Type>, StoredType,
                       std::conditional_t<std::is_array_v<Type>, Type &, Type>>;

template <typename Type, typename StoredType>
class storage_instance<external, Type, StoredType, void> {
  using held_type = external_held_t<Type, StoredType>;
  static constexpr bool converts = external_converts_v<Type>;

  template <typename T> static held_type hold(T &&instance) {
    if constexpr (converts) {
      return detail::convert_type<StoredType, consume>(
          std::forward<T>(instance));
    } else {
      return std::forward<T>(instance);
    }
  }

public:
  template <typename T>
  storage_instance(T &&instance) : instance_(hold(std::forward<T>(instance))) {}

  // An array is published by its first element, a pointer by value and
  // anything else by reference.
  decltype(auto) get() {
    if constexpr (std::is_array_v<Type>) {
      return &instance_[0];
    } else if constexpr (std::is_pointer_v<Type>) {
      return instance_;
    } else {
      return (instance_);
    }
  }

private:
  held_type instance_;
};

template <typename Type, typename StoredType, typename Factory,
          typename Conversions>
class external_storage {
  storage_instance<external, Type, StoredType, void> instance_;

public:
  using conversions = Conversions;
  using type = Type;
  using stored_type = StoredType;
  using resolved_type =
      decltype(std::declval<
                   storage_instance<external, Type, StoredType, void> &>()
                   .get());
  using tag_type = external;

  template <typename T>
  external_storage(T &&instance) : instance_(std::forward<T>(instance)) {}

  template <typename Context, typename Container, typename Scope>
  decltype(auto) resolve(Scope, Context &, Container &) {
    return instance_.get();
  }
  constexpr bool is_resolved() const { return true; }

  void reset() {}
};
} // namespace detail
} // namespace dingo
