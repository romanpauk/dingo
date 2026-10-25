//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/base/type_list.h>
#include <dingo/binding/binding_resolution.h>
#include <dingo/storage/type_storage_traits.h>
#include <dingo/type/request_lift.h>
#include <dingo/type/type_conversion_traits.h>

#include <cstddef>
#include <type_traits>

namespace dingo {
namespace model {
namespace probe {

// Route-list checks shared by the generated matrix, behavior, route, and
// interface probes. Keeping them here makes the C++ side of the model one
// implementation instead of one copy per generated suite.

enum class target_shape {
  value,
  lvalue_reference,
  rvalue_reference,
  pointer,
  array,
  array_pointer,
  wrapper,
  wrapper_pointer,
};

template <typename T> struct target_shape_of {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  static constexpr target_shape value = [] {
    if constexpr (std::is_lvalue_reference_v<T>) {
      return target_shape::lvalue_reference;
    } else if constexpr (std::is_rvalue_reference_v<T>) {
      return target_shape::rvalue_reference;
    } else if constexpr (std::is_array_v<type>) {
      return target_shape::array;
    } else if constexpr (std::is_pointer_v<type>) {
      using pointee = std::remove_cv_t<std::remove_pointer_t<type>>;
      if constexpr (std::is_array_v<pointee>) {
        return target_shape::array_pointer;
      } else if constexpr (dingo::type_traits<pointee>::enabled) {
        return target_shape::wrapper_pointer;
      } else {
        return target_shape::pointer;
      }
    } else if constexpr (dingo::type_traits<type>::enabled) {
      return target_shape::wrapper;
    } else {
      return target_shape::value;
    }
  }();
};

template <typename Request, typename Resolutions>
struct static_request_supported;

template <typename Request>
struct static_request_supported<Request, type_list<>> : std::false_type {};

template <typename Request, typename Head, typename... Tail>
struct static_request_supported<Request, type_list<Head, Tail...>>
    : std::bool_constant<
          detail::is_resolution_request_for_v<Head, Request> ||
          static_request_supported<Request, type_list<Tail...>>::value> {};

template <typename Request, typename... Resolutions>
constexpr bool runtime_request_supported(type_list<Resolutions...>) {
  return (detail::matches_resolution_request<Resolutions>(
              describe_type<Request>()) ||
          ...);
}

template <typename Resolutions> struct all_targets_match;

template <> struct all_targets_match<type_list<>> : std::true_type {};

template <typename Head, typename... Tail>
struct all_targets_match<type_list<Head, Tail...>>
    : std::bool_constant<
          static_request_supported<typename Head::target_type,
                                   type_list<Head, Tail...>>::value &&
          runtime_request_supported<typename Head::target_type>(
              type_list<Head, Tail...>{}) &&
          all_targets_match<type_list<Tail...>>::value> {};

template <typename Supplied, typename... Resolutions>
constexpr bool all_routes_accept_access(type_list<Resolutions...>) {
  return (
      detail::access_satisfies<
          Supplied, typename Resolutions::operation::required_access>::value &&
      ...);
}

template <std::size_t Index, typename Resolutions> struct route_kind_at;

template <typename Head, typename... Tail>
struct route_kind_at<0, type_list<Head, Tail...>> {
  static constexpr auto value = Head::operation::kind;
};

template <std::size_t Index, typename Head, typename... Tail>
struct route_kind_at<Index, type_list<Head, Tail...>>
    : route_kind_at<Index - 1, type_list<Tail...>> {};

template <std::size_t Index, typename Resolutions> struct route_retention_at;

template <typename Head, typename... Tail>
struct route_retention_at<0, type_list<Head, Tail...>> {
  static constexpr bool value = Head::operation::requires_source_retention;
};

template <std::size_t Index, typename Head, typename... Tail>
struct route_retention_at<Index, type_list<Head, Tail...>>
    : route_retention_at<Index - 1, type_list<Tail...>> {};

template <std::size_t Index, typename Resolutions> struct route_step_count_at;

template <typename Head, typename... Tail>
struct route_step_count_at<0, type_list<Head, Tail...>> {
  static constexpr std::size_t value =
      type_list_size_v<typename Head::operation::conversion_steps>;
};

template <std::size_t Index, typename Head, typename... Tail>
struct route_step_count_at<Index, type_list<Head, Tail...>>
    : route_step_count_at<Index - 1, type_list<Tail...>> {};

template <std::size_t Index, typename Resolutions> struct route_target_shape_at;

template <typename Head, typename... Tail>
struct route_target_shape_at<0, type_list<Head, Tail...>> {
  static constexpr auto value =
      target_shape_of<typename Head::target_type>::value;
};

template <std::size_t Index, typename Head, typename... Tail>
struct route_target_shape_at<Index, type_list<Head, Tail...>>
    : route_target_shape_at<Index - 1, type_list<Tail...>> {};

template <std::size_t Index, typename Steps> struct conversion_step_kind_at;

template <typename Head, typename... Tail>
struct conversion_step_kind_at<0, type_list<Head, Tail...>> {
  static constexpr auto value = Head::kind;
};

template <std::size_t Index, typename Head, typename... Tail>
struct conversion_step_kind_at<Index, type_list<Head, Tail...>>
    : conversion_step_kind_at<Index - 1, type_list<Tail...>> {};

template <std::size_t RouteIndex, std::size_t StepIndex, typename Resolutions>
struct route_step_kind_at;

template <std::size_t StepIndex, typename Head, typename... Tail>
struct route_step_kind_at<0, StepIndex, type_list<Head, Tail...>> {
  static constexpr auto value = conversion_step_kind_at<
      StepIndex, typename Head::operation::conversion_steps>::value;
};

template <std::size_t RouteIndex, std::size_t StepIndex, typename Head,
          typename... Tail>
struct route_step_kind_at<RouteIndex, StepIndex, type_list<Head, Tail...>>
    : route_step_kind_at<RouteIndex - 1, StepIndex, type_list<Tail...>> {};

template <typename... Resolutions>
constexpr bool all_routes_have_no_cache(type_list<Resolutions...>) {
  return (
      (type_list_size_v<typename Resolutions::operation::cache_types> == 0) &&
      ...);
}

template <typename Request, typename... Resolutions>
constexpr bool request_supported(type_list<Resolutions...> resolutions) {
  return runtime_request_supported<Request>(resolutions);
}

// A direct request is published by either the leaf interface or the stored
// wrapper interface. The two lists overlap for some cells and diverge for
// alternative and wrapper-as-leaf rows.
template <typename Request, typename Storage, typename Leaf>
constexpr bool direct_request_supported() {
  return request_supported<Request>(
             typename detail::binding_resolutions<Leaf, Storage>::type{}) ||
         request_supported<Request>(
             typename detail::binding_resolutions<typename Storage::type,
                                                  Storage>::type{});
}

template <typename Request, typename Storage, typename Leaf>
constexpr bool lifted_request_supported_impl() {
  if constexpr (detail::request_lift<Request>::enabled) {
    using lift = detail::request_lift<Request>;
    return direct_request_supported<Request, Storage, Leaf>() ||
           lifted_request_supported_impl<typename lift::inner_type, Storage,
                                         Leaf>();
  } else {
    return direct_request_supported<Request, Storage, Leaf>();
  }
}

template <typename Request, typename Storage, typename Leaf>
constexpr bool lifted_request_supported() {
  return lifted_request_supported_impl<Request, Storage, Leaf>();
}

} // namespace probe
} // namespace model
} // namespace dingo
