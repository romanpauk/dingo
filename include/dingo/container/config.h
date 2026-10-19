//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/binding/model.h>
#include <dingo/lookup/definition.h>
#include <dingo/lookup/lookup.h>
#include <dingo/runtime/context.h>
#include <dingo/runtime/registry.h>
#include <dingo/runtime/traits.h>
#include <dingo/static/activation/set.h>
#include <dingo/static/bindings.h>
#include <dingo/static/graph/graph.h>

#include <type_traits>

namespace dingo {
namespace detail {

template <typename Self, typename RuntimeConfig>
struct static_container_runtime_configuration {
  using runtime_base = runtime_registry<
      typename RuntimeConfig::container_traits_type,
      typename RuntimeConfig::allocator_type,
      typename RuntimeConfig::parent_registry_type,
      std::conditional_t<
          std::is_void_v<typename RuntimeConfig::resolve_root_type>, Self,
          typename RuntimeConfig::resolve_root_type>,
      RuntimeConfig::owns_runtime_data>;

  static constexpr bool owns_runtime_data = RuntimeConfig::owns_runtime_data;
  static constexpr bool merge_parent_collections =
      RuntimeConfig::merge_parent_collections;
  // A runtime-only front-end has no parent registry: the container is the
  // resolve root and consults its parent container directly.
  static constexpr bool runtime_only =
      std::is_void_v<typename RuntimeConfig::parent_registry_type>;
};

template <typename Self>
struct static_container_runtime_configuration<Self, void> {
  using runtime_base =
      runtime_registry<dynamic_container_traits,
                       typename dynamic_container_traits::allocator_type, Self,
                       Self>;

  static constexpr bool owns_runtime_data = true;
  static constexpr bool merge_parent_collections = false;
  static constexpr bool runtime_only = false;
};

// Compile-time requirements of the static part of a container. They are
// instantiated only for containers that actually carry static bindings.
template <bool Enabled, typename Bindings, typename ContainerTraits,
          typename RuntimeConfig>
struct static_requirements {
  static constexpr bool value = true;
};

template <typename... Registrations, typename ContainerTraits,
          typename RuntimeConfig>
struct static_requirements<true, static_bindings<Registrations...>,
                           ContainerTraits, RuntimeConfig> {
  using bindings_type = static_bindings<Registrations...>;
  using index_entries = normalize_lookup_definitions_t<
      container_lookup_definition_type_t<ContainerTraits>>;

  static_assert(bindings_type::valid,
                "container requires a valid compile-time bindings source");
  static_assert(
      key_value_bindings_are_declared<
          typename bindings_type::interface_bindings, index_entries>::value,
      "container fixed dingo::key_type<Key, Value> bindings require "
      "static_container with associative<Key, Interface, one> or "
      "associative<Key, Interface, many>");
  static_assert(
      key_value_bindings_are_unique<typename bindings_type::interface_bindings,
                                    index_entries>::value,
      "container fixed runtime-key lookup bindings must be unique "
      "for one lookups and unique by storage for many lookups");
  static_assert(
      std::is_void_v<RuntimeConfig> ||
          graph_analysis<bindings_type, true>::resolvable,
      "register_type bindings<...> requires a resolvable compile-time binding "
      "graph");
  static_assert(!std::is_void_v<RuntimeConfig> ||
                    graph_analysis<bindings_type, true>::resolvable,
                "container requires a resolvable compile-time binding graph");
  static_assert((binding_factory_is_default_constructible<
                     binding_model<Registrations>>::value &&
                 ...),
                "container requires default-constructible compile-time "
                "factories");
  static_assert((binding_storage_is_default_constructible<
                     binding_model<Registrations>>::value &&
                 ...),
                "container requires default-constructible compile-time "
                "storage objects");

  static constexpr bool value = true;
};

// The runtime context type of a container that has the given runtime
// configuration, spelled without completing the container.
template <typename Self, typename RuntimeConfig>
using static_container_runtime_context_t =
    runtime_context<typename static_container_runtime_configuration<
        Self, RuntimeConfig>::runtime_base::allocator_type>;

} // namespace detail
} // namespace dingo
