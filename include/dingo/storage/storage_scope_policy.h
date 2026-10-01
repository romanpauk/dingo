//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/core/config.h>

#include <dingo/core/construction_scope.h>
#include <dingo/resolution/recursion_guard.h>
#include <dingo/storage/materialized_source.h>
#include <dingo/storage/type_storage_traits.h>
#include <dingo/type/rebind_type.h>
#include <dingo/type/type_list.h>
#include <dingo/type/type_traits.h>

#include <cstddef>
#include <memory>
#include <new>
#include <optional>
#include <type_traits>
#include <utility>

// A storage scope is described once, by a storage_scope_policy specialization.
// From the policy this header derives both the request lists storage_traits
// exposes (policy x form) and the storage_materialization_traits behavior.
//
// Request lists are produced in three steps:
//   1. storage_form_traits<Type> classifies the registered type into a
//      storage_form and names the lists the form offers: borrowed lists
//      (value, lvalue reference, pointer) and consumed lists (rvalue
//      reference).
//   2. storage_scope_policy<Scope>::exposed(form) selects which of those lists
//      the scope publishes.
//   3. detail::storage_cell<Scope, form, Type, U> turns the selection into the
//      four lists. A cell that deviates from the rule specializes
//      detail::storage_cell; those specializations are the complete list of
//      per-cell deviations.

namespace dingo {

// The registered shapes storage exposure distinguishes.
enum class storage_form {
  unknown,
  plain,
  alternative,
  pointer,
  array,
  bounded_array,
  unique_handle,
  unique_array_handle,
  shared_handle,
  shared_array_handle,
  optional
};

// The request lists of a form a scope publishes.
enum class storage_exposure : unsigned {
  none = 0,
  value = 1,
  lvalue = 2,
  rvalue = 4,
  pointer = 8
};

constexpr storage_exposure operator|(storage_exposure lhs,
                                     storage_exposure rhs) {
  return static_cast<storage_exposure>(static_cast<unsigned>(lhs) |
                                       static_cast<unsigned>(rhs));
}

constexpr storage_exposure operator&(storage_exposure lhs,
                                     storage_exposure rhs) {
  return static_cast<storage_exposure>(static_cast<unsigned>(lhs) &
                                       static_cast<unsigned>(rhs));
}

// When a recursive materialization of a storage is detected.
enum class materialization_guard {
  // never
  none,
  // while the storage has not been resolved yet
  while_unresolved,
  // on every materialization
  always
};

// Describes a storage scope.
//
//   enabled                    the tag is a storage scope
//   is_stable                  references and pointers handed out remain valid
//   consumes                   resolution consumes a produced value (an
//                              rvalue source) instead of borrowing the stored
//                              instance
//   guard                      recursion guard kind
//   retains_unresolved_source  an unresolved storage may retain source state
//                              across a resolution
//   exposed(form)              request lists published for a form; none
//                              disables the form
template <typename StorageTag> struct storage_scope_policy {
  static constexpr bool enabled = false;
  static constexpr bool is_stable = false;
  static constexpr bool consumes = false;
  static constexpr materialization_guard guard = materialization_guard::none;
  static constexpr bool retains_unresolved_source = false;

  static constexpr storage_exposure exposed(storage_form) {
    return storage_exposure::none;
  }
};

namespace detail {
constexpr bool exposes_any(storage_exposure exposed, storage_exposure lists) {
  return (exposed & lists) != storage_exposure::none;
}

template <typename Type, typename = void> struct storage_form_traits {
  static constexpr storage_form form = storage_form::unknown;
};

// storage_form_traits<Type>::borrowed<U> lists what a borrowed storage can
// hand out for the leaf U: copies, references and pointers. consumed<U> lists
// what a consumed value can be converted to. They are separate so a scope only
// instantiates the lists it publishes.

template <typename Type>
struct storage_form_traits<Type, std::enable_if_t<!type_traits<Type>::enabled &&
                                                  !std::is_reference_v<Type> &&
                                                  !std::is_array_v<Type>>> {
  static constexpr storage_form form = is_alternative_type_v<Type>
                                           ? storage_form::alternative
                                           : storage_form::plain;

  template <typename U> struct borrowed {
    using value_types = type_list<U>;
    using lvalue_reference_types = type_list<U &>;
    using pointer_types = type_list<U *>;
  };

  template <typename U> struct consumed {
    using rvalue_reference_types = type_list<U &&>;
  };
};

template <typename T> struct storage_form_traits<T *> {
  static constexpr storage_form form = storage_form::pointer;

  template <typename U> struct borrowed {
    using value_types = type_list<U>;
    using lvalue_reference_types = type_list<U &>;
    using pointer_types = type_list<U *>;
  };

  template <typename U> struct consumed {
    using rvalue_reference_types =
        type_list<std::unique_ptr<U> &&, std::shared_ptr<U> &&>;
  };
};

// An array is consumed as an owning handle to its rows.
template <typename T, typename U> struct array_row_handle_promotions {
  using type = type_list<wrapper_rebind_leaf_t<std::unique_ptr<T[]>, U> &&,
                         wrapper_rebind_leaf_t<std::shared_ptr<T[]>, U> &&>;
};

template <typename T> struct storage_form_traits<T[]> {
  static constexpr storage_form form = storage_form::array;

  template <typename U> struct borrowed {
    using value_types = type_list<>;
    using lvalue_reference_types = type_list<>;
    using pointer_types = type_list<typename wrapper_rebind_leaf<T, U>::type *>;
  };

  template <typename U> struct consumed {
    using rvalue_reference_types =
        typename array_row_handle_promotions<T, U>::type;
  };
};

template <typename T, size_t N> struct storage_form_traits<T[N]> {
  static constexpr storage_form form = storage_form::bounded_array;

  template <typename U> struct borrowed {
  private:
    using row_type = typename wrapper_rebind_leaf<T, U>::type;
    using exact_type = typename wrapper_rebind_leaf<T[N], U>::type;

  public:
    using value_types = type_list<>;
    using lvalue_reference_types = type_list<exact_lookup<exact_type> &>;
    using pointer_types = type_list<row_type *, exact_lookup<exact_type> *>;
  };

  template <typename U> struct consumed {
    using rvalue_reference_types =
        typename array_row_handle_promotions<T, U>::type;
  };
};

template <typename Array, typename Deleter>
struct storage_form_traits<std::unique_ptr<Array, Deleter>,
                           std::enable_if_t<std::is_array_v<Array> &&
                                            (std::extent_v<Array, 0> == 0)>> {
  static constexpr storage_form form = storage_form::unique_array_handle;

  template <typename U> struct borrowed {
  private:
    using handle_type =
        wrapper_rebind_leaf_t<std::unique_ptr<Array, Deleter>, U>;

  public:
    using value_types = type_list<>;
    using lvalue_reference_types = type_list<handle_type &>;
    using pointer_types = typename smart_array_pointer_types<
        handle_type, std::remove_extent_t<Array>, U>::type;
  };

  template <typename U> struct consumed {
    using rvalue_reference_types =
        type_list<wrapper_rebind_leaf_t<std::unique_ptr<Array, Deleter>, U> &&,
                  wrapper_rebind_leaf_t<std::shared_ptr<Array>, U> &&>;
  };
};

template <typename T, typename Deleter>
struct storage_form_traits<std::unique_ptr<T, Deleter>,
                           std::enable_if_t<!std::is_array_v<T>>> {
  static constexpr storage_form form = storage_form::unique_handle;

  template <typename U> struct borrowed {
  private:
    using types = wrapper_storage_types<
        wrapper_rebind_leaf_t<std::unique_ptr<T, Deleter>, U>>;

  public:
    using value_types = type_list<U>;
    using lvalue_reference_types = typename types::lvalue_reference_types;
    using pointer_types = typename types::pointer_types;
  };

  template <typename U> struct consumed {
    using rvalue_reference_types =
        type_list<wrapper_rebind_leaf_t<std::unique_ptr<T, Deleter>, U> &&,
                  std::shared_ptr<wrapper_rebind_leaf_t<T, U>> &&>;
  };
};

template <typename Array>
struct storage_form_traits<std::shared_ptr<Array>,
                           std::enable_if_t<std::is_array_v<Array> &&
                                            (std::extent_v<Array, 0> == 0)>> {
  static constexpr storage_form form = storage_form::shared_array_handle;

  template <typename U> struct borrowed {
  private:
    using handle_type = wrapper_rebind_leaf_t<std::shared_ptr<Array>, U>;

  public:
    using value_types = type_list<handle_type>;
    using lvalue_reference_types = type_list<handle_type &>;
    using pointer_types = typename smart_array_pointer_types<
        handle_type, std::remove_extent_t<Array>, U>::type;
  };

  template <typename U> struct consumed {
    using rvalue_reference_types =
        type_list<wrapper_rebind_leaf_t<std::shared_ptr<Array>, U> &&>;
  };
};

template <typename T>
struct storage_form_traits<std::shared_ptr<T>,
                           std::enable_if_t<!std::is_array_v<T>>> {
  static constexpr storage_form form = storage_form::shared_handle;

  template <typename U> struct borrowed {
  private:
    using types =
        wrapper_storage_types<wrapper_rebind_leaf_t<std::shared_ptr<T>, U>>;

  public:
    using value_types = type_list_unique_t<
        type_list_cat_t<type_list<U>, typename types::copyable_value_types>>;
    using lvalue_reference_types = typename types::lvalue_reference_types;
    using pointer_types = typename types::pointer_types;
  };

  template <typename U> struct consumed {
    using rvalue_reference_types =
        type_list<wrapper_rebind_leaf_t<std::shared_ptr<T>, U> &&>;
  };
};

template <typename T> struct storage_form_traits<std::optional<T>> {
  static constexpr storage_form form = storage_form::optional;

  template <typename U> struct borrowed {
    using value_types = type_list<U>;
    using lvalue_reference_types =
        type_list<U &, exact_lookup<std::optional<T>> &>;
    using pointer_types = type_list<U *, exact_lookup<std::optional<T>> *>;
  };

  template <typename U> struct consumed {
    using rvalue_reference_types = type_list<std::optional<T> &&>;
  };
};

template <bool Active, typename Lists> struct borrowed_storage_lists {
  using value_types = type_list<>;
  using lvalue_reference_types = type_list<>;
  using pointer_types = type_list<>;
};

template <typename Lists> struct borrowed_storage_lists<true, Lists> : Lists {};

template <bool Active, typename Lists> struct consumed_storage_lists {
  using rvalue_reference_types = type_list<>;
};

template <typename Lists> struct consumed_storage_lists<true, Lists> : Lists {};

// A form is stored in a scope if the scope publishes any list for it.
template <typename StorageTag, typename Type>
inline constexpr bool has_storage_cell_v =
    storage_form_traits<Type>::form != storage_form::unknown &&
    storage_scope_policy<StorageTag>::exposed(
        storage_form_traits<Type>::form) != storage_exposure::none;

// The generic rule: publish the lists of the form that the policy exposes.
template <typename StorageTag, typename Type, typename U>
struct storage_cell_rule {
private:
  using policy = storage_scope_policy<StorageTag>;
  using form = storage_form_traits<Type>;
  static constexpr storage_exposure exposed = policy::exposed(form::form);
  static constexpr storage_exposure borrowed_lists = storage_exposure::value |
                                                     storage_exposure::lvalue |
                                                     storage_exposure::pointer;

  using borrowed = borrowed_storage_lists<exposes_any(exposed, borrowed_lists),
                                          typename form::template borrowed<U>>;
  using consumed =
      consumed_storage_lists<exposes_any(exposed, storage_exposure::rvalue),
                             typename form::template consumed<U>>;

public:
  static constexpr bool enabled = true;
  static constexpr bool is_stable = policy::is_stable;

  using value_types =
      std::conditional_t<exposes_any(exposed, storage_exposure::value),
                         typename borrowed::value_types, type_list<>>;
  using lvalue_reference_types =
      std::conditional_t<exposes_any(exposed, storage_exposure::lvalue),
                         typename borrowed::lvalue_reference_types,
                         type_list<>>;
  using rvalue_reference_types =
      std::conditional_t<exposes_any(exposed, storage_exposure::rvalue),
                         typename consumed::rvalue_reference_types,
                         type_list<>>;
  using pointer_types =
      std::conditional_t<exposes_any(exposed, storage_exposure::pointer),
                         typename borrowed::pointer_types, type_list<>>;
};

// A (scope, form) cell. Specialize it for a cell that deviates from the rule.
template <typename StorageTag, storage_form Form, typename Type, typename U>
struct storage_cell : storage_cell_rule<StorageTag, Type, U> {};
} // namespace detail

template <typename StorageTag, typename Type, typename U>
struct storage_traits<
    StorageTag, Type, U,
    std::enable_if_t<detail::has_storage_cell_v<StorageTag, Type>>>
    : detail::storage_cell<StorageTag, detail::storage_form_traits<Type>::form,
                           Type, U> {};

namespace detail {
// A scope without a guard does not consult the factory of its storage.
template <materialization_guard Guard, typename Storage, typename Context>
inline constexpr bool materialization_guard_enabled_v =
    recursion_guard_enabled_v<typename Storage::factory_type, Context>;

template <typename Storage, typename Context>
inline constexpr bool materialization_guard_enabled_v<
    materialization_guard::none, Storage, Context> = false;
} // namespace detail

template <typename StorageTag, typename Type>
struct storage_materialization_traits<
    StorageTag, Type,
    std::enable_if_t<storage_scope_policy<StorageTag>::enabled>> {
private:
  using policy = storage_scope_policy<StorageTag>;

public:
  static constexpr bool can_retain_source = policy::retains_unresolved_source;

  template <typename Leaf, typename Context, typename Storage>
  static auto make_guard([[maybe_unused]] Context &context,
                         [[maybe_unused]] const Storage &storage) {
    if constexpr (!detail::materialization_guard_enabled_v<policy::guard,
                                                           Storage, Context>) {
      return detail::no_materialization_scope();
    } else if constexpr (policy::guard ==
                         materialization_guard::while_unresolved) {
      return detail::recursion_guard_wrapper<Leaf>(context, &storage,
                                                   !storage.is_resolved());
    } else {
      return detail::recursion_guard<Leaf>(context, &storage);
    }
  }

  template <typename Storage>
  static bool retains_source([[maybe_unused]] const Storage &storage) {
    if constexpr (policy::retains_unresolved_source) {
      // Only unresolved storage can retain source/dependency state. Once the
      // instance is resolved, later materialization reads from stable storage
      // and does not need new persistent source data.
      return !storage.is_resolved();
    } else {
      return false;
    }
  }

  template <typename Context, typename Storage, typename Container>
  static auto materialize_source(construction_scope scope, Context &context,
                                 Storage &storage, Container &container) {
    if constexpr (policy::consumes) {
      using source_type =
          std::remove_cv_t<std::remove_reference_t<decltype(storage.resolve(
              scope, context, container))>>;
      return detail::make_rvalue_source<source_type>(
          std::in_place, [&](void *ptr) {
            new (ptr) source_type(storage.resolve(scope, context, container));
          });
    } else {
      return detail::make_resolved_source(
          storage.resolve(scope, context, container));
    }
  }

  // Only a scope that consumes its source can place it in the context.
  template <typename Context, typename Storage, typename Container,
            bool Consumes = policy::consumes,
            typename = std::enable_if_t<Consumes>>
  static auto &materialize_source_in_context(construction_scope scope,
                                             Context &context, Storage &storage,
                                             Container &container) {
    using source_type =
        std::remove_cv_t<std::remove_reference_t<decltype(storage.resolve(
            scope, context, container))>>;
    return context.template construct<detail::rvalue_source<source_type>>(
        scope, std::in_place, [&](void *ptr) {
          new (ptr) source_type(storage.resolve(scope, context, container));
        });
  }
};
} // namespace dingo
