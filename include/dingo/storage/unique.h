//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/base/config.h>

#include <dingo/factory/constructor.h>
#include <dingo/factory/factory_traits.h>
#include <dingo/storage/scope_traits.h>
#include <dingo/storage/storage.h>
#include <dingo/storage/type_storage_traits.h>
#include <dingo/type/normalized_type.h>

#include <optional>
#include <type_traits>

namespace dingo {
struct unique {};

namespace detail {
template <typename Type, typename StoredType, typename Factory,
          typename Conversions>
class unique_storage;
} // namespace detail

template <> struct scope_traits<unique> {
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

  // A plain value is additionally consumable as an optional.
  template <storage_form Form, typename Type, typename U>
  struct requests : detail::scope_rule_requests<unique, Type, U> {
    using rvalue_reference_types = std::conditional_t<
        Form == storage_form::plain,
        type_list_cat_t<typename detail::scope_rule_requests<
                            unique, Type, U>::rvalue_reference_types,
                        type_list<std::optional<U> &&>>,
        typename detail::scope_rule_requests<unique, Type,
                                             U>::rvalue_reference_types>;
  };

  template <typename Type, typename StoredType, typename Factory,
            typename Conversions>
  using storage =
      detail::unique_storage<Type, StoredType, Factory, Conversions>;
};

namespace detail {
template <typename Type, typename StoredType, typename Factory,
          typename Conversions>
class unique_storage : Factory {
public:
  template <typename... Args>
  unique_storage(Args &&...args) : Factory(std::forward<Args>(args)...) {}

  using conversions = Conversions;
  using factory_type = Factory;
  using type = Type;
  using stored_type = StoredType;
  // A bounded array is resolved as a pointer to its first element.
  using resolved_type = std::conditional_t<(std::extent_v<Type> > 0),
                                           std::remove_extent_t<Type> *, Type>;
  using tag_type = unique;

  template <typename Context, typename Container>
  decltype(auto) resolve(construction_scope scope, Context &context,
                         Container &container) {
    return Factory::template construct<Type>(scope, context, container);
  }
};
} // namespace detail
} // namespace dingo
