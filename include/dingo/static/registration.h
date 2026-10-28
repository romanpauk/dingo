//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

// The compile-time requirements of a container's static registrations: valid
// bindings, declared and unique key values, a resolvable dependency graph, and
// default-constructible factories and storage objects. They are instantiated
// only for containers that carry static bindings.

#include <dingo/binding/binding_model.h>
#include <dingo/lookup/container_lookup_definition.h>
#include <dingo/lookup/lookup.h>
#include <dingo/static/binding.h>
#include <dingo/static/graph.h>
#include <dingo/static/registry.h>

#include <type_traits>

namespace dingo {
namespace detail {

// The conditions both container shapes assert. Each container keeps its own
// diagnostics, but the conditions themselves are written once.
template <typename Bindings, typename ContainerTraits>
struct static_registration_checks;

template <typename... Registrations, typename ContainerTraits>
struct static_registration_checks<static_bindings<Registrations...>,
                                  ContainerTraits> {
  using bindings_type = static_bindings<Registrations...>;
  using index_entries = normalize_lookup_definitions_t<
      container_lookup_definition_type_t<ContainerTraits>>;

  static constexpr bool valid = bindings_type::valid;
  static constexpr bool key_values_declared = key_value_bindings_are_declared<
      typename bindings_type::interface_bindings, index_entries>::value;
  static constexpr bool key_values_unique =
      key_value_bindings_are_unique<typename bindings_type::interface_bindings,
                                    index_entries>::value;
  static constexpr bool graph_resolvable =
      graph_analysis<bindings_type>::resolvable;
  static constexpr bool factories_constructible =
      (binding_factory_is_default_constructible<
           binding_model<Registrations>>::value &&
       ...);
  static constexpr bool storage_constructible =
      (binding_storage_is_default_constructible<
           binding_model<Registrations>>::value &&
       ...);
};

template <bool Enabled, typename Bindings, typename ContainerTraits,
          typename RuntimeConfig>
struct static_registration_requirements {
  static constexpr bool value = true;
};

template <typename... Registrations, typename ContainerTraits,
          typename RuntimeConfig>
struct static_registration_requirements<true, static_bindings<Registrations...>,
                                        ContainerTraits, RuntimeConfig> {
  using checks = static_registration_checks<static_bindings<Registrations...>,
                                            ContainerTraits>;

  static_assert(checks::valid,
                "container requires a valid compile-time bindings source");
  static_assert(checks::key_values_declared,
                "container fixed dingo::key_type<Key, Value> bindings require "
                "static_container with associative<Key, Interface, one> or "
                "associative<Key, Interface, many>");
  static_assert(checks::key_values_unique,
                "container fixed runtime-key lookup bindings must be unique "
                "for one lookups and unique by storage for many lookups");
  static_assert(
      std::is_void_v<RuntimeConfig> || checks::graph_resolvable,
      "register_type bindings<...> requires a resolvable compile-time "
      "binding graph");
  static_assert(!std::is_void_v<RuntimeConfig> || checks::graph_resolvable,
                "container requires a resolvable compile-time binding graph");
  static_assert(checks::factories_constructible,
                "container requires default-constructible compile-time "
                "factories");
  static_assert(checks::storage_constructible,
                "container requires default-constructible compile-time "
                "storage objects");

  static constexpr bool value = true;
};

} // namespace detail
} // namespace dingo
