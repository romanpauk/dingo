//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/base/config.h>
#include <dingo/conversion/source.h>

#include <dingo/factory/constructor.h>
#include <dingo/factory/factory_traits.h>
#include <dingo/storage/scope_traits.h>
#include <dingo/storage/storage.h>
#include <dingo/storage/type_storage_traits.h>
#include <dingo/type/shape.h>

#include <type_traits>

namespace dingo {
struct unique {};

namespace detail {
template <typename Type, typename StoredType, typename Factory>
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
    case storage_form::pointer:
    case storage_form::array:
    case storage_form::bounded_array:
      return storage_exposure::rvalue | storage_exposure::pointer;
    case storage_form::shared_handle:
      return storage_exposure::value | storage_exposure::lvalue |
             storage_exposure::rvalue | storage_exposure::pointer;
    case storage_form::plain:
    case storage_form::alternative:
    case storage_form::unique_handle:
    case storage_form::unique_array_handle:
    case storage_form::shared_array_handle:
    case storage_form::optional:
      return storage_exposure::lvalue | storage_exposure::rvalue |
             storage_exposure::pointer;
    case storage_form::reference:
    case storage_form::unknown:
      break;
    }
    return storage_exposure::none;
  }

  template <typename Type, typename StoredType, typename Factory>
  using storage = detail::unique_storage<Type, StoredType, Factory>;
};

namespace detail {
template <typename Type, typename StoredType, typename Factory>
class unique_storage : Factory {
public:
  template <typename... Args>
  unique_storage(Args &&...args) : Factory(std::forward<Args>(args)...) {}

  using factory_type = Factory;
  using type = Type;
  using stored_type = StoredType;
  // A bounded array is resolved as a pointer to its first element.
  using resolved_type = std::conditional_t<(std::extent_v<Type> > 0),
                                           std::remove_extent_t<Type> *, Type>;
  using tag_type = unique;

  // A bounded array is made as the pointer to its first element, which the
  // request owns.
  template <typename Context, typename Container, typename Scope>
  decltype(auto) resolve(Scope scope, Context &context, Container &container) {
    if constexpr (std::extent_v<Type> > 0) {
      return array_owner<std::remove_extent_t<Type>>(
          Factory::template construct<Type>(scope, context, container));
    } else {
      return Factory::template construct<Type>(scope, context, container);
    }
  }
};
} // namespace detail
} // namespace dingo
