//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/static/activation_set.h>
#include <dingo/type/request_lift.h>

#include <type_traits>
#include <utility>

namespace dingo {
namespace detail {

// How a backend decides between a direct route and a request lift.
enum class request_route_policy {
  // The static container knows at compile time whether the direct route exists.
  compile_time,
  // A container with a runtime path tries the direct route first and falls back
  // to the lift only when the binding publishes no wrapper route.
  runtime_fallback,
};

// The inner request type only exists when the lift is enabled. Keeping it in a
// base class avoids naming it from a branch that a non-liftable request would
// instantiate.
template <typename Lift, bool Enabled> struct request_plan_inner {};

template <typename Lift> struct request_plan_inner<Lift, true> {
  using inner_type = typename Lift::inner_type;
};

// Probe a static binding only when the request can lift. A non-liftable
// request never consults the static selection graph, which keeps the plan's
// instantiation set the same as the direct-only path.
template <typename Bindings, typename T, typename LookupKey, bool Enabled>
struct request_plan_static_probe {
  static constexpr bool present = false;
  static constexpr bool served = false;
};

template <typename Bindings, typename T, typename LookupKey>
struct request_plan_static_probe<Bindings, T, LookupKey, true> {
  static constexpr bool present =
      static_request_present_v<Bindings, std::remove_cv_t<T>, LookupKey>;
  static constexpr bool served =
      static_request_served_v<Bindings, std::remove_cv_t<T>, LookupKey>;
};

// One request plan for every entry point. The plan owns the static route
// probe, the liftability check, and the direct/lift precedence; the entry point
// only supplies the backend policy and the two route bodies.
template <typename T, typename LookupKey, typename Bindings,
          request_route_policy Policy>
struct request_plan
    : request_plan_inner<request_lift<std::remove_cv_t<T>>,
                         request_lift<std::remove_cv_t<T>>::enabled>,
      request_plan_static_probe<Bindings, T, LookupKey,
                                request_lift<std::remove_cv_t<T>>::enabled> {
  using wrapper = std::remove_cv_t<T>;
  using lift_type = request_lift<wrapper>;

  static constexpr bool lift_enabled = lift_type::enabled;
  static constexpr bool static_binding_present =
      request_plan_static_probe<Bindings, T, LookupKey, lift_enabled>::present;
  static constexpr bool static_route_served =
      request_plan_static_probe<Bindings, T, LookupKey, lift_enabled>::served;

  // A static binding that does not publish the wrapper route is lifted
  // without forming the direct request, which would reject the shape at
  // compile time. A runtime fallback also takes this path when the static
  // route is absent, so the direct request is never instantiated for it.
  static constexpr bool lift_before_direct =
      lift_enabled && (Policy == request_route_policy::runtime_fallback
                           ? static_binding_present && !static_route_served
                           : !static_route_served);

  static constexpr bool direct_with_runtime_fallback =
      lift_enabled && Policy == request_route_policy::runtime_fallback &&
      !lift_before_direct;
};

} // namespace detail
} // namespace dingo
