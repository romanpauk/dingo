//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/base/type_list.h>
#include <dingo/key/key.h>
#include <dingo/registration/type_registration.h>
#include <dingo/static/bindings.h>
#include <dingo/static/graph/cost.h>
#include <dingo/static/graph/structure.h>
#include <dingo/static/graph/topology.h>

#include <cstddef>
#include <tuple>
#include <type_traits>
#include <utility>

namespace dingo {

namespace detail {

// ---------------------------------------------------------------------------
// Graph of a static registry.
// ---------------------------------------------------------------------------

// Structure and topology of a registry. It does not read the costs, so
// checking that a graph is resolvable never instantiates them.
template <typename StaticRegistry> struct graph_model {
  using host_bindings = typename StaticRegistry::interface_bindings;
  using universe = typename graph_universe<host_bindings>::type;

  static constexpr std::size_t root_count = type_list_size_v<host_bindings>;
  static constexpr auto structure =
      graph_make_structure<host_bindings>(universe{});
  static constexpr auto topology = graph_sort(structure, root_count);
  static constexpr bool bounds_known =
      graph_roots_bounded(structure, root_count);
};

template <typename StaticRegistry> struct graph_cost_model {
  static constexpr auto costs =
      graph_make_costs(graph_model<StaticRegistry>::structure,
                       typename graph_model<StaticRegistry>::universe{});
};

template <typename StaticRegistry,
          bool Acyclic = graph_model<StaticRegistry>::topology.resolvable &&
                         !graph_model<StaticRegistry>::topology.contains_cycle>
struct graph_topological_bindings {
  using type = void;
};

// MSVC does not see a pack used only inside a subscript of a static member,
// so the index is read through a variable template.
template <typename StaticRegistry, std::size_t Position>
inline constexpr std::size_t graph_topological_index_v =
    graph_model<StaticRegistry>::topology.order[Position];

template <typename StaticRegistry, typename... Universe,
          std::size_t... Position>
auto graph_topological_list(type_list<Universe...>,
                            std::index_sequence<Position...>)
    -> type_list<std::tuple_element_t<
        graph_topological_index_v<StaticRegistry, Position>,
        std::tuple<Universe...>>...>;

template <typename StaticRegistry>
struct graph_topological_bindings<StaticRegistry, true> {
  using type = decltype(graph_topological_list<StaticRegistry>(
      typename graph_model<StaticRegistry>::universe{},
      std::make_index_sequence<
          graph_model<StaticRegistry>::topology.order_size>{}));
};

template <typename StaticRegistry>
using graph_topological_bindings_t =
    typename graph_topological_bindings<StaticRegistry>::type;

template <typename Binding, typename StaticRegistry>
inline constexpr bool static_binding_resolvable_v = graph_binding_resolvable(
    graph_model<StaticRegistry>::structure,
    graph_type_index_in<Binding,
                        typename graph_model<StaticRegistry>::universe>::value);

template <typename StaticRegistry>
constexpr bool graph_bindings_resolvable(void *) {
  return false;
}

template <typename StaticRegistry, typename... Bindings>
constexpr bool graph_bindings_resolvable(type_list<Bindings...> *) {
  return (static_binding_resolvable_v<Bindings, StaticRegistry> && ...);
}

template <typename Bindings, typename StaticRegistry>
inline constexpr bool static_bindings_resolvable_v =
    graph_bindings_resolvable<StaticRegistry>(static_cast<Bindings *>(nullptr));

// The RuntimeDependencies parameter selects how dependency requests are
// accounted for in the bounds; edges without a binding are never followed, so
// the topology does not depend on it.
template <typename StaticRegistry, bool RuntimeDependencies = false>
struct basic_static_graph_topology_analysis {
  static constexpr bool resolvable =
      graph_model<StaticRegistry>::topology.resolvable;
  static constexpr bool contains_cycle =
      graph_model<StaticRegistry>::topology.contains_cycle;
  static constexpr bool acyclic = resolvable && !contains_cycle;
};

template <typename StaticRegistry, bool RuntimeDependencies = false>
struct basic_static_execution_traits {
private:
  using model = graph_model<StaticRegistry>;

  static constexpr graph_bounds bounds =
      graph_bound(model::structure, graph_cost_model<StaticRegistry>::costs,
                  model::topology, model::root_count, RuntimeDependencies);

public:
  static constexpr bool resolvable = model::topology.resolvable;
  static constexpr bool contains_cycle = model::topology.contains_cycle;
  static constexpr bool acyclic = resolvable && !contains_cycle;
  static constexpr bool static_context_eligible = model::bounds_known;
  static constexpr std::size_t max_retained_frame_depth =
      bounds.retained_frame_depth;
  static constexpr std::size_t max_destructible_slots =
      bounds.destructible_slots;
  static constexpr std::size_t max_temporary_slots = bounds.temporary_slots;
  static constexpr std::size_t max_temporary_size = bounds.temporary_size;
  static constexpr std::size_t max_temporary_align = bounds.temporary_align;
};

template <typename StaticRegistry, bool RuntimeDependencies = false>
using graph_analysis =
    basic_static_graph_topology_analysis<StaticRegistry, RuntimeDependencies>;

template <typename StaticRegistry, bool RuntimeDependencies = false>
using execution_traits =
    basic_static_execution_traits<StaticRegistry, RuntimeDependencies>;

template <typename StaticRegistry>
using static_execution_traits =
    basic_static_execution_traits<StaticRegistry, false>;

} // namespace detail

template <typename StaticSource, typename = void> struct static_graph;

template <typename StaticSource>
struct static_graph<StaticSource,
                    std::void_t<static_bindings_source_t<StaticSource>>>
    : static_graph<static_bindings_source_t<StaticSource>, void> {};

template <typename... Registrations>
struct static_graph<static_bindings<Registrations...>, void>
    : private detail::static_registry_dependency_diagnostics<
          typename static_bindings<Registrations...>::interface_bindings,
          detail::binding_model<Registrations>...> {
  using static_registry_type = static_bindings<Registrations...>;
  using interface_bindings = typename static_registry_type::interface_bindings;
  using nodes =
      detail::static_graph_nodes_t<interface_bindings, static_registry_type>;

  static_assert(static_registry_type::valid,
                "static_graph requires a valid compile-time bindings source");

  static constexpr bool resolvable =
      detail::graph_analysis<static_registry_type>::resolvable;
  static constexpr bool contains_cycle =
      detail::graph_analysis<static_registry_type>::contains_cycle;
  static constexpr bool acyclic =
      detail::graph_analysis<static_registry_type>::acyclic;
  using topological_bindings =
      detail::graph_topological_bindings_t<static_registry_type>;
  using topological_nodes =
      detail::static_graph_nodes_t<topological_bindings, static_registry_type>;

  template <typename Interface>
  using binding =
      typename static_registry_type::template binding<Interface,
                                                      detail::no_lookup_key_t>;

  template <typename Interface>
  using node =
      detail::static_graph_node_t<binding<Interface>, static_registry_type>;

  template <typename Interface>
  using dependency_bindings =
      typename static_registry_type::template dependency_bindings<Interface>;

  template <typename Interface>
  using dependency_nodes =
      detail::static_graph_nodes_t<dependency_bindings<Interface>,
                                   static_registry_type>;
};

} // namespace dingo
