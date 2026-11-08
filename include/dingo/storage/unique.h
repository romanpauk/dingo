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
#include <dingo/type/shape.h>

#include <type_traits>

namespace dingo {
struct unique {};

namespace detail {
template <typename Type, typename StoredType, typename Factory,
          typename Conversions>
class unique_storage;

// Whether a wrapper holds another wrapper.
template <typename Type, typename = void>
inline constexpr bool holds_wrapper_v = false;

template <typename Type>
inline constexpr bool
    holds_wrapper_v<Type, std::void_t<typename type_traits<Type>::value_type>> =
        type_traits<Type>::enabled && !std::is_pointer_v<Type> &&
        type_traits<typename type_traits<Type>::value_type>::enabled;
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
      return storage_exposure::lvalue | storage_exposure::rvalue |
             storage_exposure::pointer;
    case storage_form::pointer:
    case storage_form::array:
    case storage_form::bounded_array:
      return storage_exposure::rvalue | storage_exposure::pointer;
    case storage_form::shared_handle:
      return storage_exposure::value | storage_exposure::lvalue |
             storage_exposure::rvalue | storage_exposure::pointer;
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

  // A new value is additionally moved into an owning handle of its own: a
  // plain one or an alternative, or the element of an optional. The element of
  // an optional or a unique_ptr is moved out of it; that of a shared_ptr is
  // copied. The leaf of one that holds another wrapper is moved out of both,
  // and into an owning handle. Any other wrapper of what is moved out is
  // composed of it.
  template <storage_form Form, typename Type, typename U>
  struct requests : detail::scope_rule_requests<unique, Type, U> {
  private:
    using rule = detail::scope_rule_requests<unique, Type, U>;
    using handles = type_list<std::unique_ptr<U> &&, std::shared_ptr<U> &&>;
    static constexpr bool nested = detail::holds_wrapper_v<Type>;
    using wrapped = std::conditional_t<
        Form == storage_form::plain || Form == storage_form::alternative,
        handles,
        std::conditional_t<
            Form == storage_form::optional,
            type_list_cat_t<type_list<U &&>, handles>,
            std::conditional_t<
                Form == storage_form::unique_handle,
                type_list_cat_t<
                    type_list<U &&>,
                    std::conditional_t<nested, handles, type_list<>>>,
                type_list<>>>>;

  public:
    using rvalue_reference_types =
        type_list_cat_t<typename rule::rvalue_reference_types, wrapped>;
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

  template <typename Context, typename Container, typename Scope>
  decltype(auto) resolve(Scope scope, Context &context, Container &container) {
    return Factory::template construct<Type>(scope, context, container);
  }
};
} // namespace detail
} // namespace dingo
