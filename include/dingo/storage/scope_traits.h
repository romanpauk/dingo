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
#include <dingo/resolution/materialized_source.h>
#include <dingo/storage/storage.h>
#include <dingo/storage/type_storage_traits.h>
#include <dingo/type/rebind_type.h>
#include <dingo/type/type_traits.h>

#include <cstddef>
#include <memory>
#include <new>
#include <optional>
#include <type_traits>
#include <utility>

// A storage scope is a tag type and one specialization of scope_traits, which
// says how the scope stores an instance and which requests it publishes for
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
//   storage<Type, StoredType, Factory, Conversions>
//                              the storage of a registered type: resolve(),
//                              and is_resolved() and reset() for a scope that
//                              keeps its instance
//   requests<Form, Type, U>    optional: the request lists of a form, when
//                              the scope does not publish the exposed lists of
//                              the form; detail::scope_rule_requests is the
//                              default to extend

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

  // A shared_ptr of the bounded array takes it over too.
  template <typename U> struct consumed {
    using rvalue_reference_types = type_list_cat_t<
        typename array_row_handle_promotions<T, U>::type,
        type_list<wrapper_rebind_leaf_t<std::shared_ptr<T[N]>, U> &&>>;
  };
};

// A wrapper is classified by its semantics; the lists of a form are those of
// any wrapper of the form, the standard ones and a user's alike. The element
// of an array handle cannot be borrowed as an object.
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

template <typename Wrapper, typename U>
using wrapper_element_t =
    wrapper_rebind_leaf_t<typename type_traits<Wrapper>::value_type, U>;

// The sharing handle of the element U of a sole owner.
template <typename Wrapper, typename U>
using shared_handle_t = typename type_traits<Wrapper>::template share_t<
    wrapper_element_t<Wrapper, U>>;

template <typename Wrapper, bool Array> struct owner_storage_form_base {
  template <typename U> struct borrowed {
  private:
    using handle = wrapper_rebind_leaf_t<Wrapper, U>;
    using types = wrapper_storage_types<handle>;

  public:
    using lvalue_reference_types =
        std::conditional_t<Array, type_list<handle &>,
                           typename types::lvalue_reference_types>;
    using pointer_types = std::conditional_t<
        Array,
        typename smart_array_pointer_types<
            handle, typename type_traits<Wrapper>::value_type, U>::type,
        typename types::pointer_types>;
    using copyable_value_types = typename types::copyable_value_types;
  };
};

template <typename Wrapper, bool Borrowable>
struct wrapper_storage_form<Wrapper, semantics::sole_owner, Borrowable> {
  static constexpr storage_form form = Borrowable
                                           ? storage_form::unique_handle
                                           : storage_form::unique_array_handle;

  template <typename U>
  struct borrowed
      : owner_storage_form_base<Wrapper, !Borrowable>::template borrowed<U> {
    using value_types =
        std::conditional_t<Borrowable, type_list<U>, type_list<>>;
  };

  template <typename U> struct consumed {
    using rvalue_reference_types =
        type_list<wrapper_rebind_leaf_t<Wrapper, U> &&,
                  shared_handle_t<Wrapper, U> &&>;
  };
};

template <typename Wrapper, bool Borrowable>
struct wrapper_storage_form<Wrapper, semantics::shared_owner, Borrowable> {
  static constexpr storage_form form = Borrowable
                                           ? storage_form::shared_handle
                                           : storage_form::shared_array_handle;

  template <typename U>
  struct borrowed
      : owner_storage_form_base<Wrapper, !Borrowable>::template borrowed<U> {
    using value_types = std::conditional_t<
        Borrowable,
        type_list_unique_t<type_list_cat_t<
            type_list<U>, typename owner_storage_form_base<Wrapper, false>::
                              template borrowed<U>::copyable_value_types>>,
        type_list<wrapper_rebind_leaf_t<Wrapper, U>>>;
  };

  template <typename U> struct consumed {
    using rvalue_reference_types =
        type_list<wrapper_rebind_leaf_t<Wrapper, U> &&>;
  };
};

template <typename Wrapper>
struct wrapper_storage_form<Wrapper, semantics::optional_value, true> {
  static constexpr storage_form form = storage_form::optional;

  // A kept optional is copied as its element or as itself.
  template <typename U> struct borrowed {
    using value_types = type_list<U, exact_lookup<Wrapper>>;
    using lvalue_reference_types = type_list<U &, exact_lookup<Wrapper> &>;
    using pointer_types = type_list<U *, exact_lookup<Wrapper> *>;
  };

  template <typename U> struct consumed {
    using rvalue_reference_types = type_list<Wrapper &&>;
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

// A form is stored in a scope if the scope publishes any list for it. A
// reference is stored if the scope exposes references, with the lists of the
// form it refers to.
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

// The generic rule: publish the lists of the form that the scope exposes.
template <typename StorageTag, typename Type, typename U>
struct scope_rule_requests {
private:
  using policy = scope_traits<StorageTag>;
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

// The request lists of a scope for a form: those it declares, or the rule.
template <typename StorageTag, storage_form Form, typename Type, typename U,
          typename = void>
struct scope_requests : scope_rule_requests<StorageTag, Type, U> {};

template <typename StorageTag, storage_form Form, typename Type, typename U>
struct scope_requests<StorageTag, Form, Type, U,
                      std::void_t<typename scope_traits<
                          StorageTag>::template requests<Form, Type, U>>>
    : scope_traits<StorageTag>::template requests<Form, Type, U> {};

struct no_scope_requests {};

template <typename StorageTag, typename Stored, typename U>
struct stored_type_requests
    : std::conditional_t<
          has_storage_cell_v<StorageTag, Stored>,
          scope_requests<StorageTag,
                         storage_form_traits<remove_cvref_t<Stored>>::form,
                         remove_cvref_t<Stored>, U>,
          no_scope_requests> {
  static_assert(has_storage_cell_v<StorageTag, Stored>,
                "the scope does not store this type");
  static constexpr bool is_stable = scope_traits<StorageTag>::is_stable;
};

// The requests a registration publishes when it declares no conversions: the
// request lists its scope publishes for the form of the stored type.
template <typename StorageTag, typename Type, typename U>
struct conversions : stored_type_requests<StorageTag, Type, U> {};
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
  static constexpr bool retains_unresolved_source =
      policy::retains_unresolved_source;

  template <typename Leaf, typename Context, typename Storage>
  static auto make_guard([[maybe_unused]] Context &context,
                         [[maybe_unused]] const Storage &storage) {
    if constexpr (!materialization_guard_enabled_v<policy::guard, Storage,
                                                   Context>) {
      return no_materialization_scope();
    } else if constexpr (policy::guard ==
                         materialization_guard::while_unresolved) {
      return recursion_guard_wrapper<Leaf>(context, &storage,
                                           !storage.is_resolved());
    } else {
      return recursion_guard<Leaf>(context, &storage);
    }
  }

  // Only unresolved storage can retain source or dependency state; once
  // resolved, materialization reads from stable storage.
  template <typename Storage>
  static bool retains_source([[maybe_unused]] const Storage &storage) {
    if constexpr (policy::retains_unresolved_source) {
      return !storage.is_resolved();
    } else {
      return false;
    }
  }

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
  static auto &materialize_source_in_context(Scope scope, Context &context,
                                             Storage &storage,
                                             Container &container) {
    static_assert(policy::consumes);
    return context.template construct<
        rvalue_source<source_t<Storage, Context, Container>>>(
        scope, std::in_place,
        source_builder(scope, context, storage, container));
  }
};
} // namespace detail
} // namespace dingo
