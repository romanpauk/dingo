//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/base/config.h>

#include <dingo/base/construction_scope.h>
#include <dingo/base/type_list.h>
#include <dingo/context/recursion_guard.h>
#include <dingo/conversion/source.h>
#include <dingo/semantics/model.h>
#include <dingo/storage/storage.h>
#include <dingo/storage/type_storage_traits.h>
#include <dingo/type/shape.h>
#include <dingo/type/type_traits.h>

#include <cstddef>
#include <memory>
#include <new>
#include <type_traits>
#include <utility>

// A storage scope is a tag type and one specialization of scope_traits, which
// declares how the scope stores an instance and which requests it publishes for
// it. From it this header derives the request lists and the materialization
// behavior of the scope.
//
// Request lists are produced in three steps:
//   1. storage_form_traits<Type> classifies the registered type into a
//      storage_form and names the lists the form offers: borrowed lists
//      (value, lvalue reference, pointer) and consumed lists (rvalue
//      reference).
//   2. scope_traits<Scope>::exposed(form) selects which of those lists the
//      scope publishes.
//   3. The selection becomes the four lists, unless the scope declares the
//      lists of a form itself as requests<Form, Type, U>.

namespace dingo {

// The registered shapes storage exposure distinguishes. A reference is a form
// of its own: only a scope that is given its instance can hold one.
enum class storage_form {
  unknown,
  reference,
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

// The members of a scope_traits specialization:
//
//   enabled                    true
//   is_stable                  references and pointers handed out remain valid
//   consumes                   resolution consumes a produced value (an
//                              rvalue source) instead of borrowing the stored
//                              instance
//   guard                      recursion guard kind
//   retains_unresolved_source  an unresolved storage may retain source state
//                              across a resolution
//   exposed(form)              request lists published for a form; none
//                              disables the form. For a stored reference,
//                              exposed(reference) enables it and the form it
//                              refers to selects the lists
//   storage<Type, StoredType, Factory>
//                              the storage of a registered type: resolve(),
//                              and is_resolved() and reset() for a scope that
//                              keeps its instance

namespace detail {
template <typename Type, typename = void> struct storage_form_traits {
  static constexpr storage_form form = storage_form::unknown;
};

template <typename Type>
struct storage_form_traits<Type, std::enable_if_t<!type_traits<Type>::enabled &&
                                                  !std::is_reference_v<Type> &&
                                                  !std::is_array_v<Type>>> {
  static constexpr storage_form form = is_alternative_type_v<Type>
                                           ? storage_form::alternative
                                           : storage_form::plain;
};

template <typename T> struct storage_form_traits<T *> {
  static constexpr storage_form form = storage_form::pointer;
};

template <typename T> struct storage_form_traits<T[]> {
  static constexpr storage_form form = storage_form::array;
};

template <typename T, size_t N> struct storage_form_traits<T[N]> {
  static constexpr storage_form form = storage_form::bounded_array;
};

// A wrapper is classified by its semantics, the standard ones and a user's
// alike. The element of an array handle cannot be borrowed as an object.
template <typename Wrapper, typename = void> struct wrapper_semantics {
  using type = void;
};

template <typename Wrapper>
struct wrapper_semantics<
    Wrapper, std::void_t<typename type_traits<Wrapper>::semantics>> {
  using type = typename type_traits<Wrapper>::semantics;
};

template <typename Wrapper,
          typename Semantics = typename wrapper_semantics<Wrapper>::type,
          bool Borrowable = is_value_borrowable_v<Wrapper>>
struct wrapper_storage_form {
  static constexpr storage_form form = storage_form::unknown;
};

template <typename Wrapper>
struct storage_form_traits<Wrapper,
                           std::enable_if_t<type_traits<Wrapper>::enabled &&
                                            !std::is_pointer_v<Wrapper>>>
    : wrapper_storage_form<Wrapper> {};

template <typename Wrapper, bool Borrowable>
struct wrapper_storage_form<Wrapper, semantics::sole_owner, Borrowable> {
  static constexpr storage_form form = Borrowable
                                           ? storage_form::unique_handle
                                           : storage_form::unique_array_handle;
};

template <typename Wrapper, bool Borrowable>
struct wrapper_storage_form<Wrapper, semantics::shared_owner, Borrowable> {
  static constexpr storage_form form = Borrowable
                                           ? storage_form::shared_handle
                                           : storage_form::shared_array_handle;
};

template <typename Wrapper>
struct wrapper_storage_form<Wrapper, semantics::optional_value, true> {
  static constexpr storage_form form = storage_form::optional;
};

// A form is stored in a scope if the scope exposes it. A reference is stored
// if the scope exposes references, and the form it refers to.
template <typename StorageTag, typename Type>
inline constexpr bool has_storage_cell_v = [] {
  constexpr storage_form form = storage_form_traits<remove_cvref_t<Type>>::form;
  if constexpr (!scope_traits<StorageTag>::enabled ||
                form == storage_form::unknown) {
    return false;
  } else {
    using policy = scope_traits<StorageTag>;
    return policy::exposed(form) != storage_exposure::none &&
           (!std::is_reference_v<Type> ||
            policy::exposed(storage_form::reference) != storage_exposure::none);
  }
}();

} // namespace detail

namespace detail {
// A scope without a guard does not consult the factory of its storage.
template <materialization_guard Guard, typename Storage, typename Context>
inline constexpr bool materialization_guard_enabled_v =
    recursion_guard_enabled_v<typename Storage::factory_type, Context>;

template <typename Storage, typename Context>
inline constexpr bool materialization_guard_enabled_v<
    materialization_guard::none, Storage, Context> = false;
} // namespace detail

namespace detail {
// How a binding materializes the instance of its storage, derived from the
// scope_traits of its scope.
template <typename StorageTag> struct scope_materialization {
private:
  using policy = scope_traits<StorageTag>;
  static_assert(policy::enabled,
                "a scope tag requires a dingo::scope_traits specialization");

  template <typename Storage, typename Context, typename Container>
  using source_t = remove_cvref_t<decltype(std::declval<Storage &>().resolve(
      std::declval<construction_scope>(), std::declval<Context &>(),
      std::declval<Container &>()))>;

  // Constructs the value the storage produces in place.
  template <typename Context, typename Storage, typename Container,
            typename Scope>
  static auto source_builder(Scope scope, Context &context, Storage &storage,
                             Container &container) {
    return [&, scope](void *ptr) {
      new (ptr) source_t<Storage, Context, Container>(
          storage.resolve(scope, context, container));
    };
  }

public:
  // A scope that consumes its instances produces a value for every request.
  static constexpr bool consumes = policy::consumes;

  template <typename Context, typename Storage, typename Container,
            typename Scope>
  static auto materialize_source(Scope scope, Context &context,
                                 Storage &storage, Container &container) {
    if constexpr (policy::consumes) {
      return make_rvalue_source<source_t<Storage, Context, Container>>(
          std::in_place, source_builder(scope, context, storage, container));
    } else {
      return make_resolved_source(storage.resolve(scope, context, container));
    }
  }

  // The source of a consuming scope, placed in the context.
  template <typename Context, typename Storage, typename Container,
            typename Scope>
  static auto &materialize_context_source(Scope scope, Context &context,
                                          Storage &storage,
                                          Container &container) {
    static_assert(policy::consumes);
    return context.template construct<
        rvalue_source<source_t<Storage, Context, Container>>>(
        scope, std::in_place,
        source_builder(scope, context, storage, container));
  }
};

// The semantic properties (dingo/semantics/model.h) of a scope, from its
// scope_traits: it makes a new instance for each request when it consumes
// what it resolves; it is given its instance when it can hold a reference,
// which only such a scope can; it publishes its instance before constructing
// it when it allows cycles.
template <typename StorageTag>
inline constexpr semantics::scope_traits scope_semantics_v = {
    scope_traits<StorageTag>::consumes,
    scope_traits<StorageTag>::exposed(storage_form::reference) !=
        storage_exposure::none,
    scope_allows_cycles_v<StorageTag>};
} // namespace detail
} // namespace dingo
