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
#include <dingo/context/context_base.h>
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

template <typename Request>
inline constexpr bool can_construct_from_normalized_request_v =
    type_traits<std::decay_t<Request>>::enabled &&
    !std::is_pointer_v<std::decay_t<Request>> &&
    !std::is_same_v<normalized_type_t<Request>, std::decay_t<Request>>;

// Rvalue requests are not "construct me a normalized value" requests. They
// are asking the routed storage to publish a movable result, so they must
// reject normalized/wrapper construction instead of silently degrading to `T`.
template <typename Request>
inline constexpr bool rvalue_request_requires_explicit_conversion_v =
    std::is_rvalue_reference_v<Request>;

template <typename Request>
inline constexpr bool construct_normalized_request_v =
    can_construct_from_normalized_request_v<Request> &&
    !rvalue_request_requires_explicit_conversion_v<Request> &&
    std::is_object_v<normalized_type_t<Request>> &&
    !std::is_abstract_v<normalized_type_t<Request>>;

template <typename Request,
          typename ResolvedRequest = request_result_t<Request, true>>
inline constexpr bool construct_factory_request_v =
    !rvalue_request_requires_explicit_conversion_v<Request> &&
    std::is_object_v<normalized_type_t<Request>> &&
    !std::is_abstract_v<normalized_type_t<Request>> &&
    (std::is_pointer_v<ResolvedRequest> ||
     (type_traits<ResolvedRequest>::enabled &&
      !std::is_reference_v<ResolvedRequest>) ||
     std::is_constructible_v<ResolvedRequest, normalized_type_t<Request>>);

template <typename Request, typename Context>
[[noreturn]] void throw_missing_rvalue_conversion(bool has_normalized_request,
                                                  Context &context) {
  static_assert(rvalue_request_requires_explicit_conversion_v<Request>);
  if (has_normalized_request) {
    throw detail::make_type_not_convertible_exception(
        describe_type<Request>(), describe_type<normalized_type_t<Request>>(),
        context);
  }
  throw detail::make_type_not_found_exception<Request>(context);
}

} // namespace dingo

#ifdef _MSC_VER
#pragma warning(pop)
#endif
