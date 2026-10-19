//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/base/exceptions.h>
#include <dingo/base/type_descriptor.h>
#include <dingo/binding/conversion.h>
#include <dingo/binding/interface.h>
#include <dingo/binding/selection.h>
#include <dingo/binding/source.h>
#include <dingo/context/context.h>
#include <dingo/conversion/access.h>
#include <dingo/conversion/type_conversion_traits.h>
#include <dingo/dependency/collection.h>
#include <dingo/dependency/normalized_type.h>
#include <dingo/dependency/request.h>
#include <dingo/key/annotated.h>
#include <dingo/resolution/operation.h>
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

template <typename RTTI> struct binding_request {
  instance_request<RTTI> request;
  const void *cache_key;
};

template <typename T, typename RTTI>
constexpr binding_request<RTTI>
make_binding_request(const void *cache_key = nullptr) {
  return {{RTTI::template get_type_index<request_lookup_type_t<T>>(),
           describe_type<T>()},
          cache_key};
}

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

template <typename T, typename Instance, typename Fn>
decltype(auto) forward_resolved_binding(Instance &&instance, Fn &&fn) {
  if constexpr (std::is_lvalue_reference_v<T> || std::is_pointer_v<T>) {
    return std::forward<Fn>(fn)(instance);
  } else if constexpr (std::is_rvalue_reference_v<T>) {
    return std::forward<Fn>(fn)(
        static_cast<std::remove_reference_t<Instance> &&>(instance));
  } else {
    return std::forward<Fn>(fn)(std::forward<Instance>(instance));
  }
}

// The request a static request resolves: the annotation of an annotated or
// selected request does not take part in the resolution.
template <typename Request>
using unwrapped_static_request_t = typename annotated_traits<std::conditional_t<
    is_selected_v<Request>, selected_type_t<Request>, Request>>::type;

template <typename T, typename Instance, typename Fn>
decltype(auto) consume_resolved_binding(Instance &&instance, Fn &&fn) {
  if constexpr (std::is_lvalue_reference_v<unwrapped_static_request_t<T>> &&
                std::is_pointer_v<std::remove_reference_t<Instance>>) {
    // An object route presents an address; the request takes a reference.
    return consume_resolved_binding<T>(*instance, std::forward<Fn>(fn));
  } else if constexpr (std::is_lvalue_reference_v<T> || std::is_pointer_v<T> ||
                       (!std::is_rvalue_reference_v<T> &&
                        !std::is_constructible_v<T, Instance &&>)) {
    return std::forward<Fn>(fn)(instance);
  } else {
    return std::forward<Fn>(fn)(std::forward<Instance>(instance));
  }
}

template <typename Resolution>
constexpr bool matches_resolution_request(type_descriptor requested_type) {
  if (matches_qualification_conversion(
          describe_type<typename Resolution::target_type>(), requested_type)) {
    return true;
  }
  // An object route serves the reference form and the copy of its object as
  // well.
  if constexpr (Resolution::serves_reference) {
    if (matches_qualification_conversion(
            describe_type<resolution_reference_target_t<Resolution>>(),
            requested_type)) {
      return true;
    }
  }
  if constexpr (Resolution::serves_value) {
    return matches_qualification_conversion(
        describe_type<resolution_value_target_t<Resolution>>(), requested_type);
  } else {
    return false;
  }
}

template <typename Target, typename Context, typename T>
void *result_address(construction_scope scope, Context &context, T &&instance) {
  using instance_type = std::remove_reference_t<T>;

  if constexpr (std::is_pointer_v<instance_type>) {
    return const_cast<std::remove_cv_t<std::remove_pointer_t<instance_type>> *>(
        instance);
  } else if constexpr (std::is_reference_v<T>) {
    return const_cast<std::remove_cv_t<instance_type> *>(&instance);
  } else if constexpr (std::is_constructible_v<Target, T &&>) {
    return &context.template construct<Target>(scope,
                                               std::forward<T>(instance));
  } else {
    static_assert(std::is_constructible_v<Target, T &>,
                  "resolution result must construct the requested type");
    return &context.template construct<Target>(scope, instance);
  }
}

template <typename Target, typename Resolver, typename Context,
          typename MaterializedSource>
decltype(auto) resolve_binding_value(Resolver &resolver, Context &context,
                                     MaterializedSource &&source) {
  using source_type = decltype(std::forward<MaterializedSource>(source).get());
  using conversion = type_conversion_path_t<Target &, source_type, borrow>;
  static_assert(conversion::available);
  return type_conversion<conversion>::apply(
      resolver, context, std::forward<MaterializedSource>(source).get(),
      describe_type<Target>(),
      describe_type<std::remove_reference_t<source_type>>());
}

template <typename Request, typename Resolution, typename Storage,
          typename Resolver, typename Context, typename MaterializedSource>
decltype(auto) apply_resolution(Resolver &resolver, Context &context,
                                MaterializedSource &&source) {
  using operation = typename Resolution::operation;
  return operation::template apply<Storage>(
      resolver, context, std::forward<MaterializedSource>(source),
      describe_type<Request>(), describe_type<typename Storage::type>());
}

template <typename Request, typename Resolution, typename Storage,
          typename Resolver, typename Context, typename MaterializedSource,
          typename Fn>
decltype(auto) apply_consumed_resolution(Resolver &resolver, Context &context,
                                         MaterializedSource &&source, Fn &&fn) {
  auto &&instance = apply_resolution<Request, Resolution, Storage>(
      resolver, context, std::forward<MaterializedSource>(source));
  return consume_resolved_binding<Request>(
      std::forward<decltype(instance)>(instance), std::forward<Fn>(fn));
}

template <typename Request, typename Resolution, typename Storage,
          typename Resolver, typename Context, typename Owner, typename Fn>
decltype(auto) consume_runtime_binding_resolution_request(
    construction_scope scope, Context &context, Storage &storage, Owner &owner,
    Resolver &resolver, Fn &&fn) {
  return with_runtime_binding_source(
      scope, context, storage, owner, [&](auto &&source) -> decltype(auto) {
        return apply_consumed_resolution<Request, Resolution, Storage>(
            resolver, context, std::forward<decltype(source)>(source),
            std::forward<Fn>(fn));
      });
}

template <typename Request, typename Resolution, typename Storage,
          typename Resolver, typename Context, typename Owner, typename Fn>
decltype(auto)
consume_binding_resolution_request(construction_scope scope, Context &context,
                                   Storage &storage, Owner &owner,
                                   Resolver &resolver, Fn &&fn) {
  auto consume = [&](auto &&source) -> decltype(auto) {
    return apply_consumed_resolution<Request, Resolution, Storage>(
        resolver, context, std::forward<decltype(source)>(source),
        std::forward<Fn>(fn));
  };
  using operation = typename Resolution::operation;
  if constexpr (operation_requires_source_retention_v<operation>) {
    return with_retained_binding_source(scope, context, storage, owner,
                                        std::move(consume));
  } else {
    return with_binding_source(scope, context, storage, owner,
                               std::move(consume));
  }
}

template <typename Target, typename Context, typename Instance>
resolved_address resolve_binding_address_from_instance(construction_scope scope,
                                                       Context &context,
                                                       Instance &&instance) {
  constexpr bool owns_value =
      !std::is_reference_v<Instance> &&
      !std::is_pointer_v<std::remove_reference_t<Instance>>;
  return {detail::result_address<Target>(
              scope, context, std::forward<decltype(instance)>(instance)),
          owns_value ? resolved_address::access_kind::consume
                     : resolved_address::access_kind::borrow};
}

template <typename Resolution, typename Storage, typename Resolver,
          typename Context, typename MaterializedSource>
resolved_address
resolve_address_from_source(construction_scope scope, Resolver &resolver,
                            Context &context, MaterializedSource &&source,
                            type_descriptor requested_type,
                            type_descriptor registered_type) {
  using address_type = typename Resolution::result_type;
  using operation = typename Resolution::operation;
  auto &&instance = operation::template apply<Storage>(
      resolver, context, std::forward<MaterializedSource>(source),
      requested_type, registered_type);
  return resolve_binding_address_from_instance<address_type>(
      scope, context, std::forward<decltype(instance)>(instance));
}

template <typename RTTI, typename Binding, typename Context>
resolved_address
dispatch_binding_request(construction_scope scope, Binding &binding,
                         Context &context,
                         const binding_request<RTTI> &request) {
  return binding.resolve_request(scope, context, request.request,
                                 request.cache_key);
}

} // namespace detail

template <typename T, typename RTTI, typename Binding, typename Context>
T resolve_binding_request(construction_scope scope, Binding &binding,
                          Context &context, const void *cache_key = nullptr) {
  auto request = detail::make_binding_request<T, RTTI>(cache_key);
  auto result =
      detail::dispatch_binding_request<RTTI>(scope, binding, context, request);
  return detail::convert_resolved_binding<T>(result);
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

template <typename Request, typename MakeNotConvertible, typename MakeNotFound>
[[noreturn]] void
throw_missing_rvalue_conversion(bool has_normalized_request,
                                MakeNotConvertible &&make_not_convertible,
                                MakeNotFound &&make_not_found) {
  static_assert(rvalue_request_requires_explicit_conversion_v<Request>);

  if (has_normalized_request) {
    throw std::forward<MakeNotConvertible>(make_not_convertible)();
  }

  throw std::forward<MakeNotFound>(make_not_found)();
}

template <typename Request, typename Context>
[[noreturn]] void throw_missing_rvalue_conversion(bool has_normalized_request,
                                                  Context &context) {
  using normalized_request_type = normalized_type_t<Request>;
  throw_missing_rvalue_conversion<Request>(
      has_normalized_request,
      [&]() {
        return detail::make_type_not_convertible_exception(
            describe_type<Request>(), describe_type<normalized_request_type>(),
            context);
      },
      [&]() {
        return detail::make_type_not_found_exception<Request>(context);
      });
}

template <typename Request>
[[noreturn]] void throw_missing_rvalue_conversion(bool has_normalized_request) {
  using normalized_request_type = normalized_type_t<Request>;
  throw_missing_rvalue_conversion<Request>(
      has_normalized_request,
      [&]() {
        return detail::make_type_not_convertible_exception(
            describe_type<Request>(), describe_type<normalized_request_type>());
      },
      [&]() { return detail::make_type_not_found_exception<Request>(); });
}

template <typename Request, typename ResolveExact, typename ResolveNormalized>
request_result_t<Request, true>
construct_resolved_request(ResolveExact &&resolve_exact,
                           ResolveNormalized &&resolve_normalized) {
  try {
    return std::forward<ResolveExact>(resolve_exact)();
  } catch (const type_not_convertible_exception &) {
    if constexpr (can_construct_from_normalized_request_v<Request>) {
      auto &&value = std::forward<ResolveNormalized>(resolve_normalized)();
      return type_traits<std::decay_t<Request>>::make(
          std::forward<decltype(value)>(value));
    } else {
      throw;
    }
  }
}
} // namespace dingo

#ifdef _MSC_VER
#pragma warning(pop)
#endif
