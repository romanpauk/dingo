//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/base/exceptions.h>
#include <dingo/base/type_descriptor.h>
#include <dingo/binding/interface.h>
#include <dingo/binding/selection.h>
#include <dingo/context/resolution.h>
#include <dingo/conversion/source.h>
#include <dingo/request/decomposition.h>
#include <dingo/request/execution.h>
#include <dingo/storage/retention.h>
#include <dingo/storage/scope_traits.h>
#include <dingo/storage/type_storage_traits.h>
#include <dingo/type/annotated.h>
#include <dingo/type/collection_traits.h>
#include <dingo/type/shape.h>
#include <dingo/type/type_traits.h>

#include <cassert>
#include <cstddef>
#include <memory>
#include <type_traits>
#include <utility>

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4702)
#endif

namespace dingo {
namespace detail {

template <typename T> T resolved_value(resolved_address result) {
  using result_type = std::remove_reference_t<T>;
  using value_type = std::remove_cv_t<result_type>;

  if constexpr (std::is_lvalue_reference_v<T>) {
    return *static_cast<result_type *>(result.address);
  } else if constexpr (std::is_rvalue_reference_v<T>) {
    return std::move(*static_cast<result_type *>(result.address));
  } else if constexpr (std::is_pointer_v<T>) {
    return static_cast<T>(result.address);
  } else if constexpr (std::is_move_constructible_v<value_type> &&
                       is_copy_constructible_v<value_type>) {
    if (result.access == resolved_address::access_kind::consume) {
      return std::move(*static_cast<value_type *>(result.address));
    }
    return *static_cast<value_type *>(result.address);
  } else if constexpr (std::is_move_constructible_v<value_type>) {
    assert(result.access == resolved_address::access_kind::consume);
    return std::move(*static_cast<value_type *>(result.address));
  } else if constexpr (is_copy_constructible_v<value_type>) {
    return *static_cast<value_type *>(result.address);
  } else {
    throw_type_not_convertible(detail::described_type_v<T>,
                               detail::described_type_v<value_type>);
  }
}

template <typename T, typename Instance, typename Fn>
decltype(auto) consume_resolved(Instance &&instance, Fn &&fn) {
  if constexpr (std::is_lvalue_reference_v<T> || std::is_pointer_v<T> ||
                (!std::is_rvalue_reference_v<T> &&
                 !std::is_constructible_v<T, Instance &&>)) {
    return std::forward<Fn>(fn)(instance);
  } else {
    return std::forward<Fn>(fn)(std::forward<Instance>(instance));
  }
}

} // namespace detail

namespace detail {
// A reference into the context ends before the caller uses what a resolution
// returns: the caller is refused it, as a static container's compiles.
template <typename Request, typename Scope, typename Context>
void reject_escape(Scope scope, type_descriptor registered, Context &context) {
  if constexpr (is_fixed_caller_scope_v<Scope>) {
    static_assert(!is_fixed_caller_scope_v<Scope>,
                  "a reference or a pointer to an instance the resolution "
                  "makes would dangle in what is returned to the caller; "
                  "take it by value, or let a shared scope keep what refers "
                  "to it");
  } else if (scope.is_caller()) {
    throw_type_not_convertible(described_type_v<Request>, registered, context);
  }
}

// The leaf a binding of an interface stores: the leaf of its stored type, or,
// when that is an alternative type, the leaf of the alternative of it whose
// leaf the interface's is.
template <typename Leaf, typename Interface, typename = void>
struct bound_leaf {
  using type = Leaf;
};

template <typename Leaf, typename Interface>
struct bound_leaf<Leaf, Interface,
                  std::enable_if_t<is_alternative_type_v<Leaf>>> {
  template <typename... Alternatives>
  static constexpr bool holds(type_list<Alternatives...>) {
    return (std::is_same_v<leaf_type_t<Interface>, leaf_type_t<Alternatives>> ||
            ...);
  }
  using type =
      std::conditional_t<holds(alternative_type_alternatives_t<Leaf>{}),
                         leaf_type_t<Interface>, Leaf>;
};

// What a binding of an interface and a storage plans its requests by, in
// both containers: the leaf it stores, the element of its interface, the
// stored object its storage gives, made for the request by a scope that
// makes its instance or kept, of the qualifiers it is kept of, and whether a
// request of the interface asks for the leaf as its abstract interface.
template <typename Storage, typename Interface> struct binding_execution {
  // The leaf, and the element of the interface, of no qualifiers: those of
  // what the binding stores are of each level of it.
  using leaf_type =
      std::remove_cv_t<typename bound_leaf<leaf_type_t<typename Storage::type>,
                                           Interface>::type>;
  using interface_type = execution::innermost_t<Interface, leaf_type>;
  static constexpr semantics::scope_traits scope =
      scope_semantics_v<typename Storage::tag_type>;
  // A bounded array a scope makes is given as the pointer to its first
  // element: the stored object is the array.
  using stored_type = std::conditional_t<
      scope.makes && std::is_array_v<typename Storage::type>,
      typename Storage::type,
      std::conditional_t<scope.makes,
                         remove_cvref_t<typename storage_source_traits<
                             Storage>::consumed_type>,
                         std::remove_reference_t<typename storage_source_traits<
                             Storage>::borrowed_type>>>;
  using plan = execution::binding_plan<stored_type, leaf_type, scope.makes,
                                       scope.publishes_early>;
  static constexpr bool abstract = !std::is_same_v<leaf_type, interface_type>;
  static constexpr bool modeled =
      plan::stored_stack::element != stack_element::other;
};

// The stored object a source gives, where a plan starts: a value made for
// the request, which it owns, or the object or the pointer the storage
// keeps, which it borrows.
template <typename Stored, bool Makes, typename Source, typename Store>
auto plan_start(Source &&source, Store &store) {
  using source_type = remove_cvref_t<Source>;
  if constexpr (is_array_owner_v<remove_cvref_t<decltype(*source)>>) {
    // The array a scope made, which the request owns until a step takes it
    // over.
    auto &made = *source;
    return execution::carrier<Stored, Makes>{
        reinterpret_cast<Stored *>(made.get()), std::addressof(made),
        &remove_cvref_t<decltype(made)>::release};
  } else if constexpr (is_pointer_source_v<source_type> &&
                       std::is_pointer_v<Stored>) {
    // The pointer the storage keeps, as an object of its own.
    auto &kept = store.template keep<Stored>([&] { return source.get(); });
    return execution::carry<Makes>(kept);
  } else if constexpr (is_pointer_source_v<source_type>) {
    return execution::carry<Makes>(*source.get());
  } else {
    return execution::carry<Makes>(*source);
  }
}

} // namespace detail

// A wrapper made of the normalized value is a new object, so a reference
// request is not made of it.
template <typename Request>
inline constexpr bool constructs_normalized_v =
    !std::is_reference_v<Request> &&
    type_traits<std::decay_t<Request>>::enabled &&
    !std::is_pointer_v<std::decay_t<Request>> &&
    !std::is_same_v<normalized_type_t<Request>, std::decay_t<Request>>;

// Whether the normalized value of a request is an object that can be built.
// An rvalue request is not one: it asks the routed storage to publish a
// movable result, and is rejected rather than degraded to the normalized value.
// Nor is a bounded array: a pointer to a new one cannot be made with new, and
// a handle of one only decays it.
//
template <typename Request>
inline constexpr bool constructible_normalized_v =
    !std::is_rvalue_reference_v<Request> &&
    std::is_object_v<request_constructed_t<Request>> &&
    !std::is_abstract_v<request_constructed_t<Request>> &&
    std::extent_v<request_constructed_t<Request>> == 0;
// A wrapper is made of the normalized value the request resolves: moved into
// all its layers at once, or into a handle of an array of it.
template <typename Request>
inline constexpr bool construct_normalized_request_v = [] {
  if constexpr (constructs_normalized_v<Request> &&
                constructible_normalized_v<Request>) {
    return detail::execution::makes_as<std::decay_t<Request>,
                                       request_constructed_t<Request>>();
  } else {
    return false;
  }
}();
// Whether the factory constructs an unbound request. A handle of a dynamic
// array is not constructed: a default factory has no size to make it with.
template <typename Request,
          typename ResolvedRequest = request_result_t<Request, true>>
inline constexpr bool construct_factory_request_v =
    constructible_normalized_v<Request> &&
    !std::is_lvalue_reference_v<ResolvedRequest> &&
    !detail::is_array_like_type_v<std::decay_t<ResolvedRequest>> &&
    (std::is_pointer_v<ResolvedRequest> ||
     (type_traits<ResolvedRequest>::enabled &&
      !std::is_reference_v<ResolvedRequest>) ||
     std::is_constructible_v<ResolvedRequest, normalized_type_t<Request>>);

} // namespace dingo

#ifdef _MSC_VER
#pragma warning(pop)
#endif
