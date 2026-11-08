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

#include <optional>
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

// Whether a wrapper gives its leaf away when it is consumed: none of the
// wrappers down to it shares it.
template <typename Type> constexpr bool gives_leaf_away() {
  if constexpr (!type_traits<Type>::enabled || std::is_pointer_v<Type>) {
    return true;
  } else if constexpr (std::is_base_of_v<semantics::shared_owner,
                                         type_traits<Type>>) {
    return false;
  } else {
    return gives_leaf_away<typename type_traits<Type>::value_type>();
  }
}

template <typename Type>
inline constexpr bool gives_leaf_away_v = gives_leaf_away<Type>();
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

  // A new value is additionally moved into a wrapper of its own: a plain one
  // or an alternative into an optional or an owning handle. The element of an
  // optional or a unique_ptr is moved out of it, and into a wrapper of its
  // own; that of a shared_ptr is copied. The leaf of one that holds another
  // wrapper is moved out of both, into any wrapper of its own, unless a
  // shared_ptr shares it.
  template <storage_form Form, typename Type, typename U>
  struct requests : detail::scope_rule_requests<unique, Type, U> {
  private:
    using rule = detail::scope_rule_requests<unique, Type, U>;
    using handles = type_list<std::unique_ptr<U> &&, std::shared_ptr<U> &&>;
    static constexpr bool nested = detail::holds_wrapper_v<Type>;
    // The leaf moved out of a wrapper, alone or into an optional of its own.
    // The leaf a shared_ptr in it shares is copied out of it rather than
    // moved, and into no optional.
    using moved = std::conditional_t<detail::gives_leaf_away_v<Type>,
                                     type_list<U &&, std::optional<U> &&>,
                                     type_list<U &&>>;
    using wrapped = std::conditional_t<
        Form == storage_form::plain || Form == storage_form::alternative,
        type_list_cat_t<type_list<std::optional<U> &&>, handles>,
        std::conditional_t<
            Form == storage_form::optional,
            type_list_cat_t<std::conditional_t<nested, moved, type_list<U &&>>,
                            handles>,
            std::conditional_t<
                Form == storage_form::unique_handle,
                type_list_cat_t<
                    moved, std::conditional_t<nested, handles, type_list<>>>,
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
