//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/base/construction_scope.h>
#include <dingo/base/exceptions.h>
#include <dingo/base/type_descriptor.h>
#include <dingo/base/type_list.h>
#include <dingo/binding/binding_resolution.h>
#include <dingo/factory/constructor_detection.h>
#include <dingo/type/auto_constructible.h>
#include <dingo/type/rebind_type.h>

#include <cassert>
#include <cstddef>
#include <type_traits>
#include <utility>

namespace dingo {
template <typename Allocator> class runtime_context;

template <typename RTTI> struct instance_request {
  using type_index = typename RTTI::type_index;

  type_index lookup_type;
  type_descriptor requested_type;
};

template <typename T> struct request_lookup_type {
  using type = rebind_leaf_t<T, runtime_type>;
};

template <typename T>
struct request_lookup_type<const T> : request_lookup_type<T> {};

template <typename T>
struct request_lookup_type<volatile T> : request_lookup_type<T> {};

template <typename T>
struct request_lookup_type<const volatile T> : request_lookup_type<T> {};

template <typename T> struct request_lookup_type<T *> {
  using type = std::add_pointer_t<typename request_lookup_type<T>::type>;
};

template <typename T> struct request_lookup_type<T &> {
  using type =
      std::add_lvalue_reference_t<typename request_lookup_type<T>::type>;
};

template <typename T> struct request_lookup_type<T &&> {
  using type =
      std::add_rvalue_reference_t<typename request_lookup_type<T>::type>;
};

template <typename T>
using request_lookup_type_t = typename request_lookup_type<T>::type;

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

template <typename... Resolutions>
std::size_t resolution_request_index(type_list<Resolutions...>,
                                     type_descriptor requested_type) {
  std::size_t index = 0;
  std::size_t result = sizeof...(Resolutions);
  (void)(((matches_resolution_request<Resolutions>(requested_type)
               ? (result = index, true)
               : (++index, false)) ||
          ...));
  return result;
}

template <typename Storage, typename Resolver, typename Context,
          typename MaterializedSource, typename... Resolutions>
resolved_address resolve_request_address_from_source(
    construction_scope scope, Resolver &resolver, Context &context,
    MaterializedSource &&source, std::size_t resolution_index,
    type_list<Resolutions...>, type_descriptor requested_type,
    type_descriptor registered_type) {
  resolved_address result{};
  std::size_t index = 0;
  // Short-circuiting guarantees that exactly one arm forwards the source.
  const bool matched =
      (((resolution_index == index++)
            ? (result = resolve_address_from_source<Resolutions, Storage>(
                   scope, resolver, context,
                   std::forward<MaterializedSource>(
                       source), // NOLINT(bugprone-use-after-move)
                   requested_type, registered_type),
               true)
            : false) ||
       ...);
  (void)index;
  (void)matched;
  assert(matched);
  return result;
}

// The last resort for a request no binding answers: a concrete type that may
// be auto-constructed is constructed from the container's bindings, anything
// else is reported as not found.
template <typename Request, bool MayAutoConstruct, typename R,
          typename LookupKey, typename Context, typename Container>
R construct_unbound_request(construction_scope scope, Context &context,
                            Container &container, const LookupKey &key) {
  using Type = typename Request::value_type;
  using user_type = typename Request::user_type;

  if constexpr (MayAutoConstruct &&
                is_auto_constructible<std::decay_t<user_type>>::value &&
                constructor<Type>::kind == constructor_kind::concrete) {
    static_assert(is_complete<Type>::value,
                  "auto-construction requires a complete type");
    return context.template construct<typename Request::interface_type,
                                      constructor_shape>(scope, container);
  } else {
    throw make_type_not_found_exception<typename Request::lookup_type>(context,
                                                                       key);
  }
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

} // namespace dingo
