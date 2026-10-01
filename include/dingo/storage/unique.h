//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/core/config.h>

#include <dingo/core/factory_traits.h>
#include <dingo/factory/constructor.h>
#include <dingo/storage/storage.h>
#include <dingo/storage/storage_scope_policy.h>
#include <dingo/storage/type_storage_traits.h>
#include <dingo/type/normalized_type.h>

namespace dingo {
struct unique {};

template <> struct storage_scope_policy<unique> {
  static constexpr bool enabled = true;
  static constexpr bool is_stable = false;
  static constexpr bool consumes = true;
  static constexpr materialization_guard guard = materialization_guard::always;
  static constexpr bool retains_unresolved_source = false;

  static constexpr storage_exposure exposed(storage_form form) {
    switch (form) {
    case storage_form::plain:
      return storage_exposure::lvalue | storage_exposure::rvalue;
    case storage_form::pointer:
    case storage_form::array:
    case storage_form::bounded_array:
      return storage_exposure::rvalue | storage_exposure::pointer;
    case storage_form::alternative:
    case storage_form::unique_handle:
    case storage_form::unique_array_handle:
    case storage_form::shared_handle:
    case storage_form::shared_array_handle:
    case storage_form::optional:
      return storage_exposure::rvalue;
    case storage_form::unknown:
      break;
    }
    return storage_exposure::none;
  }
};

// A plain value is additionally consumable as an optional. This is the only
// resolution_traits specialization; it applies to the registered type itself,
// not to a reference to it.
template <typename Type, typename U>
struct resolution_traits<
    unique, Type, U,
    std::enable_if_t<detail::storage_form_traits<Type>::form ==
                     storage_form::plain>> {
  using value_types = type_list<>;
  using lvalue_reference_types = type_list<>;
  using rvalue_reference_types = type_list<std::optional<U> &&>;
  using pointer_types = type_list<>;
};

namespace detail {
template <typename Type, typename U>
struct conversions<unique, Type, U> : type_storage_traits<unique, Type, U> {};

template <typename Type, typename StoredType, typename Factory,
          typename Conversions>
class storage<unique, Type, StoredType, Factory, Conversions> : Factory {
public:
  template <typename... Args>
  storage(Args &&...args) : Factory(std::forward<Args>(args)...) {}

  using conversions = Conversions;
  using factory_type = Factory;
  using type = Type;
  using stored_type = StoredType;
  using resolved_type = Type;
  using tag_type = unique;

  template <typename Context, typename Container>
  decltype(auto) resolve(construction_scope scope, Context &context,
                         Container &container) {
    return Factory::template construct<Type>(scope, context, container);
  }
};

template <typename Type, size_t N, typename StoredType, typename Factory,
          typename Conversions>
class storage<unique, Type[N], StoredType, Factory, Conversions> : Factory {
public:
  template <typename... Args>
  storage(Args &&...args) : Factory(std::forward<Args>(args)...) {}

  using conversions = Conversions;
  using factory_type = Factory;
  using type = Type[N];
  using stored_type = StoredType;
  using resolved_type = Type *;
  using tag_type = unique;

  template <typename Context, typename Container>
  decltype(auto) resolve(construction_scope scope, Context &context,
                         Container &container) {
    return Factory::template construct<Type[N]>(scope, context, container);
  }
};
} // namespace detail
} // namespace dingo
