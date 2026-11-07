//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/base/exceptions.h>
#include <dingo/base/type_descriptor.h>
#include <dingo/binding/binding_selection.h>
#include <dingo/binding/runtime_binding_interface.h>
#include <dingo/context/resolution_context.h>
#include <dingo/resolution/resolution_operation.h>
#include <dingo/storage/type_storage_traits.h>
#include <dingo/type/annotated.h>
#include <dingo/type/collection_traits.h>
#include <dingo/type/dependency_traits.h>
#include <dingo/type/normalized_type.h>
#include <dingo/type/type_traits.h>

#include <cassert>
#include <memory>
#include <type_traits>
#include <utility>

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4702)
#endif

namespace dingo {
namespace detail {

template <typename T> T convert_resolved_binding(resolved_address result) {
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
    throw make_type_not_convertible_exception(describe_type<T>(),
                                              describe_type<value_type>());
  }
}

template <typename T> T convert_resolved_binding(void *ptr) {
  return convert_resolved_binding<T>(
      {ptr, resolved_address::access_kind::borrow});
}

// The request a static request resolves: the annotation of an annotated or
// selected request does not take part in the resolution.
template <typename Request>
using unwrapped_static_request_t = typename annotated_traits<std::conditional_t<
    is_selected_v<Request>, selected_type_t<Request>, Request>>::type;

template <typename T, typename Instance, typename Fn>
decltype(auto) consume_resolved_binding(Instance &&instance, Fn &&fn) {
  if constexpr (std::is_lvalue_reference_v<T> || std::is_pointer_v<T> ||
                (!std::is_rvalue_reference_v<T> &&
                 !std::is_constructible_v<T, Instance &&>)) {
    return std::forward<Fn>(fn)(instance);
  } else {
    return std::forward<Fn>(fn)(std::forward<Instance>(instance));
  }
}

template <typename Resolution>
constexpr bool matches_resolution_request(type_descriptor requested_type) {
  return matches_qualification_conversion(
      describe_type<typename Resolution::target_type>(), requested_type);
}

} // namespace detail

template <typename T, typename RTTI, typename Binding, typename Context>
T resolve_binding_request(construction_scope scope, Binding &binding,
                          Context &context, const void *cache_key = nullptr) {
  return detail::convert_resolved_binding<T>(binding.resolve_request(
      scope, context,
      {RTTI::template get_type_index<request_lookup_type_t<T>>(),
       describe_type<T>()},
      cache_key));
}

// A wrapper made of the normalized value is a new object, so a reference
// request is not made of it.
template <typename Request>
inline constexpr bool can_construct_from_normalized_request_v =
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
// The checks read the type the request constructs, which an annotation or a
// key selection only names.
template <typename T, bool Selected = detail::is_selected_v<T>>
struct constructed_type {
  using type = typename annotated_traits<T>::type;
};

template <typename T> struct constructed_type<T, true> {
  using type = typename annotated_traits<detail::selected_type_t<T>>::type;
};

template <typename Request>
using constructed_type_t =
    typename constructed_type<normalized_type_t<Request>>::type;

template <typename Request>
inline constexpr bool constructible_normalized_v =
    !std::is_rvalue_reference_v<Request> &&
    std::is_object_v<constructed_type_t<Request>> &&
    !std::is_abstract_v<constructed_type_t<Request>> &&
    std::extent_v<constructed_type_t<Request>> == 0;
// Whether a handle can be made of Args: the array a handle of one holds is
// initialized with them, its first elements copy-initialized from them and
// the rest of a bounded one value-initialized.
template <typename Handle, typename... Args>
struct handle_makeable : std::true_type {};

template <typename Array, typename... Args>
        struct handle_makeable<std::shared_ptr<Array>, Args...>
    : std::bool_constant < !std::is_array_v<Array> ||
    detail::is_bounded_array_initializable_v < std::remove_extent_t<Array>,
    std::extent_v<Array> != 0 ? std::extent_v<Array> : sizeof...(Args),
    Args... >> {};

template <typename Array, typename Deleter, typename... Args>
struct handle_makeable<std::unique_ptr<Array, Deleter>, Args...>
    : handle_makeable<std::shared_ptr<Array>, Args...> {};

template <typename T, size_t> using repeated_argument_t = T;

template <typename Handle, typename Argument, typename Indexes>
struct handle_makeable_by;

template <typename Handle, typename Argument, size_t... Is>
struct handle_makeable_by<Handle, Argument, std::index_sequence<Is...>>
    : handle_makeable<Handle, repeated_argument_t<Argument, Is>...> {};

// A wrapper is made of the normalized value the request resolves, which it
// copies or moves.
template <typename Request>
inline constexpr bool construct_normalized_request_v = [] {
  if constexpr (can_construct_from_normalized_request_v<Request> &&
                constructible_normalized_v<Request>) {
    using value = constructed_type_t<Request>;
    return (std::is_move_constructible_v<value> ||
            std::is_copy_constructible_v<value>) &&
           handle_makeable<std::decay_t<Request>, value>::value;
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
