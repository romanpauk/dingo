//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/binding/binding_resolution.h>

#include <cassert>
#include <cstddef>
#include <utility>

namespace dingo {
namespace detail {

// A selected route plan. It owns the route descriptor (the resolution) and the
// storage it was selected for, and executes the shared conversion path.
template <typename Storage, typename Resolution> struct route_plan {
  using storage_type = Storage;
  using resolution_type = Resolution;
  using target_type = typename Resolution::target_type;
  using operation = typename Resolution::operation;
  static constexpr auto kind = operation::kind;
  static constexpr bool requires_source_retention =
      operation::requires_source_retention;

  template <typename Resolver, typename Context, typename MaterializedSource>
  DINGO_ALWAYS_INLINE static resolved_address
  execute(construction_scope scope, Resolver &resolver, Context &context,
          MaterializedSource &&source, type_descriptor requested_type,
          type_descriptor registered_type) {
    return resolve_address_from_source<Resolution, Storage>(
        scope, resolver, context, std::forward<MaterializedSource>(source),
        requested_type, registered_type);
  }
};

// The plans a binding publishes for one request list. Runtime selection picks
// an index; static selection already names the resolution type and uses
// route_plan directly.
template <typename Storage, typename... Resolutions> struct route_plan_list {
  using storage_type = Storage;
  static constexpr std::size_t size = sizeof...(Resolutions);
};

template <typename Storage, typename... Resolutions, typename Resolver,
          typename Context, typename MaterializedSource>
DINGO_ALWAYS_INLINE resolved_address execute_route_plan_list(
    route_plan_list<Storage, Resolutions...>, std::size_t index,
    construction_scope scope, Resolver &resolver, Context &context,
    MaterializedSource &&source, type_descriptor requested_type,
    type_descriptor registered_type) {
  resolved_address result{};
  std::size_t current = 0;
  const bool matched =
      (((index == current++)
            ? (result = route_plan<Storage, Resolutions>::execute(
                   scope, resolver, context,
                   std::forward<MaterializedSource>(
                       source), // NOLINT(bugprone-use-after-move)
                   requested_type, registered_type),
               true)
            : false) ||
       ...);
  (void)current;
  (void)matched;
  assert(matched);
  return result;
}

} // namespace detail
} // namespace dingo
