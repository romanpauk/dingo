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
#include <dingo/binding/request_composition.h>
#include <dingo/binding/request_derivation.h>
#include <dingo/binding/runtime_binding_interface.h>
#include <dingo/context/resolution_context.h>
#include <dingo/resolution/resolution_operation.h>
#include <dingo/storage/retention.h>
#include <dingo/storage/type_storage_traits.h>
#include <dingo/type/annotated.h>
#include <dingo/type/collection_traits.h>
#include <dingo/type/request.h>
#include <dingo/type/shape.h>
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
    throw_type_not_convertible(detail::described_type_v<T>,
                               detail::described_type_v<value_type>);
  }
}

template <typename T> T convert_resolved_binding(void *ptr) {
  return convert_resolved_binding<T>(
      {ptr, resolved_address::access_kind::borrow});
}

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
      detail::described_type_v<typename Resolution::target_type>,
      requested_type);
}

} // namespace detail

template <typename T, typename RTTI, typename Binding, typename Context>
T resolve_binding_request(construction_scope scope, Binding &binding,
                          Context &context, const void *cache_key = nullptr);

namespace detail {
template <typename T, typename RTTI, typename Binding, typename Context>
constexpr typename instance_request<RTTI>::derive_function deriver();

// The erased request of T.
template <typename T, typename RTTI, typename Binding, typename Context>
instance_request<RTTI> erased_request() {
  return {RTTI::template get_type_index<request_lookup_type_t<T>>(),
          described_type_v<T>, deriver<T, RTTI, Binding, Context>()};
}

// Resolves the request a derivation of T resolves through the binding.
template <derivation Derivation, typename T, typename RTTI, typename Binding,
          typename Context>
resolved_address resolve_derived(construction_scope scope, Binding &binding,
                                 Context &context) {
  using derived = derived_request_t<Derivation, T>;
  return binding.resolve_request(
      scope, context, erased_request<derived, RTTI, Binding, Context>(),
      nullptr);
}

// Finishes a derivation of T, of a shape it applies to.
template <derivation Derivation, typename T, typename RTTI, typename Binding,
          typename Context>
resolved_address finish_derivation(const derivation_site &site,
                                   construction_scope scope, Binding &binding,
                                   Context &context) {
  using borrow = std::integral_constant<resolved_address::access_kind,
                                        resolved_address::access_kind::borrow>;
  if constexpr (Derivation == derivation::adopt ||
                Derivation == derivation::place) {
    // A reference into the context ends before the caller uses it.
    if (scope.is_caller()) {
      throw_type_not_convertible(described_type_v<T>, site.registered, context);
    }
    auto resolved =
        resolve_derived<Derivation, T, RTTI>(scope, binding, context);
    if constexpr (Derivation == derivation::adopt) {
      place_adopted_pointer(
          scope, context,
          static_cast<derived_request_t<Derivation, T>>(resolved.address));
    }
    return {resolved.address, borrow::value};
  } else if constexpr (Derivation == derivation::take) {
    using pointer = derived_request_t<Derivation, T>;
    auto *address = static_cast<pointer>(
        resolve_derived<Derivation, T, RTTI>(scope, binding, context).address);
    place_adopted_pointer(scope, context, address);
    return {address, resolved_address::access_kind::consume};
  } else if constexpr (Derivation == derivation::keep) {
    // Kept with the instance, and published as the request's address.
    using wrapper = derived_request_t<Derivation, T>;
    auto &value = site.kept->template keep<wrapper>(
        cache::key<wrapper>(), context, [&]() -> wrapper {
          return resolve_binding_request<wrapper, RTTI>(scope, binding,
                                                        context);
        });
    if (site.cache_key != nullptr) {
      binding.publish_cache(context, site.cache_key, std::addressof(value));
    }
    return {std::addressof(value), borrow::value};
  } else if constexpr (Derivation == derivation::compose) {
    using wrapper = composed_type_t<T>;
    using inner = derived_request_t<Derivation, T>;
    auto &value =
        context.template construct_from<wrapper>(scope, [&]() -> wrapper {
          return compose<wrapper>(
              resolve_binding_request<inner, RTTI>(scope, binding, context));
        });
    return {std::addressof(value), resolved_address::access_kind::consume};
  } else {
    throw_type_not_convertible(described_type_v<T>, site.registered, context);
  }
}

template <derivation Derivation, typename T, typename RTTI, typename Binding,
          typename Context>
resolved_address derive_as(const derivation_site &site,
                           construction_scope scope, Binding &binding,
                           Context &context) {
  if constexpr (derivation_shape_v<Derivation, T>) {
    return finish_derivation<Derivation, T, RTTI>(site, scope, binding,
                                                  context);
  } else {
    throw_type_not_convertible(described_type_v<T>, site.registered, context);
  }
}

// Derives a request the binding does not publish from one it resolves.
template <typename T, typename RTTI, typename Binding, typename Context>
resolved_address
derive_binding_request(void *erased, const derivation_site &site,
                       construction_scope scope, void *erased_context) {
  auto &binding = *static_cast<Binding *>(erased);
  auto &context = *static_cast<Context *>(erased_context);
  switch (derive_request<T>(site.facts)) {
  case derivation::adopt:
    return derive_as<derivation::adopt, T, RTTI>(site, scope, binding, context);
  case derivation::keep:
    return derive_as<derivation::keep, T, RTTI>(site, scope, binding, context);
  case derivation::place:
    return derive_as<derivation::place, T, RTTI>(site, scope, binding, context);
  case derivation::take:
    return derive_as<derivation::take, T, RTTI>(site, scope, binding, context);
  case derivation::compose:
    return derive_as<derivation::compose, T, RTTI>(site, scope, binding,
                                                   context);
  case derivation::none:
    break;
  }
  throw_type_not_convertible(described_type_v<T>, site.registered, context);
}

template <typename T, typename RTTI, typename Binding, typename Context>
constexpr typename instance_request<RTTI>::derive_function deriver() {
  if constexpr (derivable_v<T>) {
    return &derive_binding_request<T, RTTI, Binding, Context>;
  } else {
    return nullptr;
  }
}
} // namespace detail

// Resolves a request through a binding. A request the binding does not
// publish is derived from one it resolves.
template <typename T, typename RTTI, typename Binding, typename Context>
T resolve_binding_request(construction_scope scope, Binding &binding,
                          Context &context, const void *cache_key) {
  return detail::convert_resolved_binding<T>(binding.resolve_request(
      scope, context, detail::erased_request<T, RTTI, Binding, Context>(),
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
template <typename Request>
inline constexpr bool constructible_normalized_v =
    !std::is_rvalue_reference_v<Request> &&
    std::is_object_v<request_constructed_t<Request>> &&
    !std::is_abstract_v<request_constructed_t<Request>> &&
    std::extent_v<request_constructed_t<Request>> == 0;
// A wrapper is made of the normalized value the request resolves, which it
// copies or moves, by the conversion from that value.
template <typename Request>
inline constexpr bool construct_normalized_request_v = [] {
  if constexpr (can_construct_from_normalized_request_v<Request> &&
                constructible_normalized_v<Request>) {
    using value = request_constructed_t<Request>;
    return (std::is_move_constructible_v<value> ||
            std::is_copy_constructible_v<value>) &&
           detail::is_type_conversion_available_v<std::decay_t<Request>,
                                                  value &&, consume>;
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
