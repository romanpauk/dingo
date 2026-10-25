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

// A selected route. It owns the route descriptor (the resolution) and the
// storage it was selected for, and applies the shared conversion path.
template <typename Storage, typename Resolution> struct resolution_route {
  using storage_type = Storage;
  using resolution_type = Resolution;
  using target_type = typename Resolution::target_type;
  using operation = typename Resolution::operation;
  static constexpr auto kind = operation::kind;
  static constexpr bool requires_source_retention =
      operation::requires_source_retention;

  template <typename Resolver, typename Context, typename MaterializedSource>
  DINGO_ALWAYS_INLINE static resolved_address
  apply(construction_scope scope, Resolver &resolver, Context &context,
        MaterializedSource &&source, type_descriptor requested_type,
        type_descriptor registered_type) {
    return resolve_address_from_source<Resolution, Storage>(
        scope, resolver, context, std::forward<MaterializedSource>(source),
        requested_type, registered_type);
  }
};

// The candidate routes a binding publishes for one request list. Runtime
// selection picks an index; static selection already names the resolution type
// and uses resolution_route directly.
template <typename Storage, typename... Resolutions>
struct resolution_candidates {
  using storage_type = Storage;
  static constexpr std::size_t size = sizeof...(Resolutions);
};

template <typename Storage, typename... Resolutions, typename Resolver,
          typename Context, typename MaterializedSource>
DINGO_ALWAYS_INLINE resolved_address resolve_candidate(
    resolution_candidates<Storage, Resolutions...>, std::size_t index,
    construction_scope scope, Resolver &resolver, Context &context,
    MaterializedSource &&source, type_descriptor requested_type,
    type_descriptor registered_type) {
  resolved_address result{};
  std::size_t current = 0;
  const bool matched =
      (((index == current++)
            ? (result = resolution_route<Storage, Resolutions>::apply(
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
