//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/registration/type_registration.h>
#include <dingo/resolution/resolution_operation.h>
#include <dingo/static/registry.h>
#include <dingo/storage/type_storage_traits.h>
#include <dingo/type/type_list.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <tuple>
#include <type_traits>
#include <utility>

namespace dingo {

struct shared;
struct unique;
struct shared_cyclical;

namespace detail {

// A static binding graph is analysed as constexpr data. The bindings reachable
// from a registry (its interface bindings followed by the local bindings its
// registrations declare) are numbered once, every dependency becomes an index
// into that numbering, and the topology and the execution bounds are computed
// by plain constexpr functions over std::array tables. Only the per-binding
// leaves that read types (dependency lists, temporary storage, costs) are
// templates.

inline constexpr std::size_t graph_npos = static_cast<std::size_t>(-1);

// ---------------------------------------------------------------------------
// Type-level introspection of a graph, used by static_graph.
// ---------------------------------------------------------------------------

template <typename Binding, typename DependencyBindings> struct graph_node {
  using binding_type = Binding;
  using dependency_bindings = DependencyBindings;
};

template <typename InterfaceBinding, typename StaticRegistry>
struct static_graph_node {
  using type = graph_node<InterfaceBinding,
                          resolved_dependency_bindings_t<
                              typename InterfaceBinding::binding_model_type,
                              typename StaticRegistry::interface_bindings>>;
};

template <typename StaticRegistry>
struct static_graph_node<void, StaticRegistry> {
  using type = void;
};

template <typename InterfaceBinding, typename StaticRegistry>
using static_graph_node_t =
    typename static_graph_node<InterfaceBinding, StaticRegistry>::type;

template <typename InterfaceBindings, typename StaticRegistry>
struct static_graph_nodes {
  using type = void;
};

template <typename... InterfaceBindings, typename StaticRegistry>
struct static_graph_nodes<type_list<InterfaceBindings...>, StaticRegistry> {
  using type =
      type_list<static_graph_node_t<InterfaceBindings, StaticRegistry>...>;
};

template <typename InterfaceBindings, typename StaticRegistry>
using static_graph_nodes_t =
    typename static_graph_nodes<InterfaceBindings, StaticRegistry>::type;

// ---------------------------------------------------------------------------
// Temporary conversion storage of a binding. This leaf reads the resolution
// operations of the binding and stays a template.
// ---------------------------------------------------------------------------

template <typename Types> struct temporary_storage_traits;

template <> struct temporary_storage_traits<type_list<>> {
  static constexpr std::size_t slots = 0;
  static constexpr std::size_t destructible_slots = 0;
  static constexpr std::size_t size = 0;
  static constexpr std::size_t align = 0;
};

template <typename... Types>
struct temporary_storage_traits<type_list<Types...>> {
  static constexpr std::size_t slots = 1;
  static constexpr std::size_t destructible_slots =
      (!std::is_trivially_destructible_v<Types> || ...) ? 1 : 0;
  static constexpr std::size_t size =
      std::max({sizeof(Types)..., std::size_t{0}});
  static constexpr std::size_t align =
      std::max({alignof(Types)..., std::size_t{0}});
};

template <typename InterfaceBinding,
          bool Stable = InterfaceBinding::binding_model_type::storage_type::
              conversions::is_stable>
struct binding_temporary_types {
  using storage_type =
      typename InterfaceBinding::binding_model_type::storage_type;
  using type = resolution_temporary_types_t<
      typename binding_resolutions<
          typename annotated_traits<
              typename InterfaceBinding::interface_type>::type,
          storage_type>::type,
      storage_shape_t<storage_type>>;
};

template <typename InterfaceBinding>
struct binding_temporary_types<InterfaceBinding, true> {
  // Stable resolution operations never allocate temporary conversion values.
  using type = type_list<>;
};

template <typename InterfaceBinding>
using binding_temporary_types_t =
    typename binding_temporary_types<InterfaceBinding>::type;

template <typename InterfaceBinding>
using binding_temporary_storage_traits =
    temporary_storage_traits<binding_temporary_types_t<InterfaceBinding>>;

// ---------------------------------------------------------------------------
// Numbering of the bindings of a graph.
// ---------------------------------------------------------------------------

template <typename LocalBindings> struct graph_local_bindings {
  using type = type_list<>;
};

template <typename... LocalRegistrations>
struct graph_local_bindings<static_bindings<LocalRegistrations...>> {
  using type = static_registry_bindings_t<LocalRegistrations...>;
};

template <typename... Bindings>
constexpr bool graph_any_local_bindings(type_list<Bindings...>) {
  return (
      !std::is_void_v<typename Bindings::binding_model_type::bindings_type> ||
      ...);
}

// The graph nodes are the interface bindings of the registry followed by the
// local bindings declared by its registrations, transitively. A local binding
// is only a node when some registration declares it.
template <typename Bindings,
          bool HasLocalBindings = graph_any_local_bindings(Bindings{})>
struct graph_universe {
  using type = Bindings;
};

template <typename... Bindings>
struct graph_universe<type_list<Bindings...>, true> {
  using type = type_list_cat_t<
      type_list<Bindings...>,
      typename graph_universe<type_list_cat_t<typename graph_local_bindings<
          typename Bindings::binding_model_type::bindings_type>::type...>>::
          type>;
};

// Returns the first position of Binding in Universe, or graph_npos for a
// binding that is not a node (void stands for a dependency without a binding).
template <typename Binding, typename... Universe>
constexpr std::size_t graph_type_index() {
  constexpr bool same[] = {std::is_same_v<Binding, Universe>..., false};
  for (std::size_t i = 0; i < sizeof...(Universe); ++i) {
    if (same[i]) {
      return i;
    }
  }
  return graph_npos;
}

template <typename Binding, typename Universe> struct graph_type_index_in;

template <typename Binding, typename... Universe>
struct graph_type_index_in<Binding, type_list<Universe...>>
    : std::integral_constant<std::size_t,
                             graph_type_index<Binding, Universe...>()> {};

// A dependency request that selects a unique binding of the host bindings by
// interface alone is looked up in the host binding index. Missing and
// ambiguous requests have no binding.
template <typename Lookup, typename Tag, typename = void>
struct graph_lookup_hit {
  static constexpr std::size_t index = graph_npos;
};

template <typename Lookup, typename Tag>
struct graph_lookup_hit<Lookup, Tag,
                        std::void_t<decltype(Lookup::select(Tag{}))>> {
  static constexpr std::size_t index = decltype(Lookup::select(Tag{}))::index;
};

// ---------------------------------------------------------------------------
// Structure: flags and dependency edges of every node.
// ---------------------------------------------------------------------------

// The edges of a node are a slice of the shared edge array. There is one edge
// per declared dependency request, graph_npos when the request has no binding,
// and one edge per matching binding for a collection request.
struct graph_vertex {
  // The binding uses shared_cyclical storage and may take part in a cycle.
  bool cyclical;
  // The binding model declares its dependency requests.
  bool requests_known;
  // The requests are declared or can be detected from the factory.
  bool dependencies_known;
  // The requests are declared and none is a collection, so every request owns
  // exactly one edge and its execution bound can be computed precisely.
  bool bounds_known;
  std::size_t edge_begin;
  std::size_t edge_count;
};

template <std::size_t Vertices, std::size_t Edges> struct graph_structure {
  std::array<graph_vertex, Vertices> vertices;
  std::array<std::size_t, Edges> edges;
};

// The requests of a binding are the declared dependencies, or the ones detected
// from its factory when none are declared.
template <typename Model,
          bool Declared = !std::is_void_v<binding_dependencies_t<Model>>>
struct graph_requests {
  using type = binding_dependencies_t<Model>;
};

template <typename Model> struct graph_requests<Model, false> {
  using type =
      typename factory_traits<typename Model::factory_type>::dependencies;
};

template <typename Request> constexpr bool graph_request_is_collection() {
  return collection_traits<
      binding_dependency_interface_t<Request>>::is_collection;
}

constexpr bool graph_requests_bounded(void *) { return false; }

template <typename... Requests>
constexpr bool graph_requests_bounded(type_list<Requests...> *) {
  return (!graph_request_is_collection<Requests>() && ...);
}

constexpr std::size_t graph_slot_count(void *) { return 0; }

template <typename... Dependencies>
constexpr std::size_t graph_slot_count(type_list<Dependencies...> *) {
  return sizeof...(Dependencies);
}

template <typename Host, typename Request>
constexpr std::size_t graph_request_slots() {
  if constexpr (graph_request_is_collection<Request>()) {
    using dependency = binding_dependency_interface_t<Request>;
    return binding_count_v<
        normalized_type_t<typename collection_traits<dependency>::resolve_type>,
        binding_dependency_key_t<Request>, Host>;
  } else {
    return 1;
  }
}

template <typename Host> constexpr std::size_t graph_requests_slots(void *) {
  return 0;
}

template <typename Host, typename... Requests>
constexpr std::size_t graph_requests_slots(type_list<Requests...> *) {
  return (std::size_t{0} + ... + graph_request_slots<Host, Requests>());
}

// Number of edges of a binding: one per matching binding of its requests,
// resolved against the host bindings and the local bindings of its model.
template <typename Host, typename Binding>
constexpr std::size_t graph_node_slots() {
  using model = typename Binding::binding_model_type;
  if constexpr (std::is_void_v<typename model::bindings_type>) {
    return graph_requests_slots<Host>(
        static_cast<typename graph_requests<model>::type *>(nullptr));
  } else {
    return graph_slot_count(
        static_cast<resolved_dependency_bindings_t<model, Host> *>(nullptr));
  }
}

template <typename Universe, std::size_t Edges>
constexpr void graph_write_bindings(void *, std::array<std::size_t, Edges> &,
                                    std::size_t) {}

template <typename Universe, typename... Dependencies, std::size_t Edges>
constexpr void graph_write_bindings(type_list<Dependencies...> *,
                                    std::array<std::size_t, Edges> &edges,
                                    std::size_t at) {
  ((edges[at++] = graph_type_index_in<Dependencies, Universe>::value), ...);
  (void)edges;
  (void)at;
}

template <typename Host, typename Universe, typename Request, std::size_t Edges>
constexpr void graph_write_request(std::array<std::size_t, Edges> &edges,
                                   std::size_t at) {
  using dependency = binding_dependency_interface_t<Request>;
  using key = binding_dependency_key_t<Request>;

  if constexpr (graph_request_is_collection<Request>()) {
    graph_write_bindings<Universe>(
        static_cast<bindings_t<normalized_type_t<typename collection_traits<
                                   dependency>::resolve_type>,
                               key, Host> *>(nullptr),
        edges, at);
  } else if constexpr (is_no_lookup_key_v<key>) {
    edges[at] = graph_lookup_hit<binding_lookup_index<Host>,
                                 binding_lookup_tag<dependency, key>>::index;
  } else {
    edges[at] =
        graph_type_index_in<binding_t<dependency, key, Host>, Universe>::value;
  }
}

template <typename Host, typename Universe, std::size_t Edges>
constexpr void graph_write_requests(void *, std::array<std::size_t, Edges> &,
                                    std::size_t) {}

template <typename Host, typename Universe, typename... Requests,
          std::size_t Edges>
constexpr void graph_write_requests(type_list<Requests...> *,
                                    std::array<std::size_t, Edges> &edges,
                                    std::size_t at) {
  ((graph_write_request<Host, Universe, Requests>(edges, at),
    at += graph_request_slots<Host, Requests>()),
   ...);
  (void)edges;
  (void)at;
}

template <typename Host, typename Universe, typename Binding,
          std::size_t Vertices, std::size_t Edges>
constexpr void graph_add_vertex(graph_structure<Vertices, Edges> &structure,
                                std::size_t &edge, std::size_t vertex) {
  using model = typename Binding::binding_model_type;
  using requests = typename graph_requests<model>::type;

  constexpr std::size_t slots = graph_node_slots<Host, Binding>();
  structure.vertices[vertex] =
      graph_vertex{std::is_same_v<typename model::storage_tag, shared_cyclical>,
                   !std::is_void_v<binding_dependencies_t<model>>,
                   !std::is_void_v<requests>,
                   graph_requests_bounded(
                       static_cast<binding_dependencies_t<model> *>(nullptr)),
                   edge,
                   slots};
  if constexpr (std::is_void_v<typename model::bindings_type>) {
    graph_write_requests<Host, Universe>(static_cast<requests *>(nullptr),
                                         structure.edges, edge);
  } else {
    graph_write_bindings<Universe>(
        static_cast<resolved_dependency_bindings_t<model, Host> *>(nullptr),
        structure.edges, edge);
  }
  edge += slots;
}

template <typename Host, typename... Bindings>
constexpr auto graph_make_structure(type_list<Bindings...>) {
  using universe = type_list<Bindings...>;
  constexpr std::size_t edges =
      (std::size_t{0} + ... + graph_node_slots<Host, Bindings>());

  graph_structure<sizeof...(Bindings), edges> structure{};
  std::size_t vertex = 0;
  std::size_t edge = 0;
  (graph_add_vertex<Host, universe, Bindings>(structure, edge, vertex++), ...);
  (void)vertex;
  (void)edge;
  return structure;
}

// ---------------------------------------------------------------------------
// Topology: resolvability, cycles and topological order.
// ---------------------------------------------------------------------------

template <std::size_t Vertices> struct graph_topology {
  // False when some cycle contains a binding that is not shared_cyclical.
  bool resolvable;
  // A legal (all shared_cyclical) cycle was reached before the traversal ended.
  bool contains_cycle;
  std::size_t order_size;
  // Reachable nodes with dependencies before dependents.
  std::array<std::size_t, Vertices> order;
};

constexpr unsigned char graph_unvisited = 0;
constexpr unsigned char graph_visiting = 1;
constexpr unsigned char graph_visited = 2;

// A dependency on the node at the given stack position closes a cycle made of
// the nodes visited since; it is legal only when all of them are
// shared_cyclical.
template <std::size_t Vertices, std::size_t Edges>
constexpr bool
graph_cycle_is_legal(const graph_structure<Vertices, Edges> &structure,
                     const std::array<std::size_t, Vertices> &stack,
                     std::size_t depth, std::size_t node) {
  std::size_t first = depth - 1;
  while (stack[first] != node) {
    --first;
  }
  for (std::size_t i = first; i < depth; ++i) {
    if (!structure.vertices[stack[i]].cyclical) {
      return false;
    }
  }
  return true;
}

// Depth-first traversal from the first `roots` nodes in order. An illegal cycle
// ends the traversal.
template <std::size_t Vertices, std::size_t Edges>
constexpr graph_topology<Vertices>
graph_sort(const graph_structure<Vertices, Edges> &structure,
           std::size_t roots) {
  graph_topology<Vertices> topology{true, false, 0, {}};
  std::array<unsigned char, Vertices> state{};
  std::array<std::size_t, Vertices> stack{};
  std::array<std::size_t, Vertices> cursor{};

  for (std::size_t root = 0; root < roots; ++root) {
    if (state[root] != graph_unvisited) {
      continue;
    }

    std::size_t depth = 1;
    state[root] = graph_visiting;
    stack[0] = root;
    cursor[0] = structure.vertices[root].edge_begin;
    while (depth != 0) {
      const std::size_t node = stack[depth - 1];
      const graph_vertex &vertex = structure.vertices[node];
      if (cursor[depth - 1] == vertex.edge_begin + vertex.edge_count) {
        state[node] = graph_visited;
        topology.order[topology.order_size++] = node;
        --depth;
        continue;
      }

      const std::size_t next = structure.edges[cursor[depth - 1]++];
      if (next == graph_npos || state[next] == graph_visited) {
        continue;
      }
      if (state[next] == graph_visiting) {
        if (!graph_cycle_is_legal(structure, stack, depth, next)) {
          topology.resolvable = false;
          return topology;
        }
        topology.contains_cycle = true;
        continue;
      }

      state[next] = graph_visiting;
      stack[depth] = next;
      cursor[depth] = structure.vertices[next].edge_begin;
      ++depth;
    }
  }
  return topology;
}

// Nodes reachable from the dependencies of `from`, and `from` itself when it
// is `included`.
template <std::size_t Vertices, std::size_t Edges>
constexpr std::array<bool, Vertices>
graph_reachable(const graph_structure<Vertices, Edges> &structure,
                std::size_t from, bool included) {
  std::array<bool, Vertices> reachable{};
  std::array<std::size_t, Vertices + 1> stack{};
  std::size_t depth = 1;
  reachable[from] = included;
  stack[0] = from;
  while (depth != 0) {
    const graph_vertex &vertex = structure.vertices[stack[--depth]];
    for (std::size_t i = 0; i < vertex.edge_count; ++i) {
      const std::size_t next = structure.edges[vertex.edge_begin + i];
      if (next != graph_npos && !reachable[next]) {
        reachable[next] = true;
        stack[depth++] = next;
      }
    }
  }
  return reachable;
}

template <std::size_t Vertices, std::size_t Edges>
constexpr bool
graph_dependencies_bound(const graph_structure<Vertices, Edges> &structure,
                         const graph_vertex &vertex) {
  for (std::size_t i = 0; i < vertex.edge_count; ++i) {
    if (structure.edges[vertex.edge_begin + i] == graph_npos) {
      return false;
    }
  }
  return vertex.dependencies_known;
}

// True when every binding reachable from `start` has its dependencies bound
// and no reachable cycle contains a binding that is not shared_cyclical.
template <std::size_t Vertices, std::size_t Edges>
constexpr bool
graph_binding_resolvable(const graph_structure<Vertices, Edges> &structure,
                         std::size_t start) {
  if (start == graph_npos) {
    return false;
  }

  const std::array<bool, Vertices> reachable =
      graph_reachable(structure, start, true);
  for (std::size_t node = 0; node < Vertices; ++node) {
    if (!reachable[node]) {
      continue;
    }
    // A binding lies on a cycle when it is reachable from its own dependencies.
    if (!graph_dependencies_bound(structure, structure.vertices[node]) ||
        (!structure.vertices[node].cyclical &&
         graph_reachable(structure, node, false)[node])) {
      return false;
    }
  }
  return true;
}

// ---------------------------------------------------------------------------
// Costs: what every binding adds to the static execution context.
// ---------------------------------------------------------------------------

// What one dependency request needs while its value is held by its dependent.
struct graph_request_cost {
  // The request is held by value, so it occupies a temporary slot.
  bool temporary;
  std::size_t destructible;
  std::size_t size;
  std::size_t align;
};

template <typename Request>
constexpr graph_request_cost graph_request_cost_of() {
  using type = typename annotated_traits<Request>::type;
  constexpr bool by_value =
      !std::is_reference_v<type> && !std::is_pointer_v<type>;

  graph_request_cost cost{};
  cost.temporary = by_value;
  cost.destructible = std::is_trivially_destructible_v<
                          std::remove_cv_t<std::remove_reference_t<Request>>>
                          ? 0
                          : 1;
  if constexpr (by_value) {
    cost.size = sizeof(std::remove_cv_t<std::remove_reference_t<type>>);
    cost.align = alignof(std::remove_cv_t<std::remove_reference_t<type>>);
  }
  return cost;
}

struct graph_request_summary {
  std::size_t destructible;
  std::size_t temporary;
  std::size_t size;
  std::size_t align;
};

constexpr graph_request_summary graph_summarize_requests(void *) {
  return graph_request_summary{};
}

template <typename... Requests>
constexpr graph_request_summary
graph_summarize_requests(type_list<Requests...> *) {
  graph_request_summary summary{};
  const graph_request_cost costs[] = {graph_request_cost_of<Requests>()...,
                                      graph_request_cost{}};
  for (std::size_t i = 0; i < sizeof...(Requests); ++i) {
    summary.destructible += costs[i].destructible;
    summary.temporary += costs[i].temporary ? 1 : 0;
    summary.size = std::max(summary.size, costs[i].size);
    summary.align = std::max(summary.align, costs[i].align);
  }
  return summary;
}

// The costs of a binding on its own, before its dependencies are added.
struct graph_cost {
  // Resolution operations of the binding never allocate conversion values.
  bool stable;
  // The binding retains a frame (shared storage).
  std::size_t frame;
  // Destructible and temporary slots including the by-value dependency
  // requests of the binding, used when dependency paths are summed.
  std::size_t destructible;
  std::size_t temporary;
  // Destructible and temporary slots of the binding itself, used when the
  // slots held by dependency requests are accounted for separately.
  std::size_t self_destructible;
  std::size_t self_temporary;
  std::size_t size;
  std::size_t align;
};

template <typename Binding> constexpr graph_cost graph_binding_cost() {
  using model = typename Binding::binding_model_type;
  using tag = typename model::storage_tag;
  using stored = typename model::storage_type::type;
  using temporaries = binding_temporary_storage_traits<Binding>;

  constexpr bool cyclical = std::is_same_v<tag, shared_cyclical>;
  constexpr std::size_t rollback = cyclical ? 1 : 0;
  constexpr std::size_t storage_slots =
      (std::is_same_v<tag, unique> || cyclical) ? 1 : 0;
  constexpr std::size_t stored_destructible =
      std::is_trivially_destructible_v<stored> ? 0 : 1;
  const graph_request_summary requests = graph_summarize_requests(
      static_cast<typename model::dependencies_type::type *>(nullptr));

  graph_cost cost{};
  cost.stable = model::storage_type::conversions::is_stable;
  cost.frame = std::is_same_v<tag, shared> ? 1 : 0;
  cost.self_temporary = std::max(temporaries::slots, storage_slots);
  cost.self_destructible =
      std::max({temporaries::destructible_slots,
                cost.self_temporary != 0 ? stored_destructible : std::size_t{0},
                rollback});
  cost.destructible = requests.destructible + temporaries::destructible_slots +
                      stored_destructible + rollback;
  cost.temporary = requests.temporary + temporaries::slots + storage_slots;
  cost.size = std::max({requests.size, temporaries::size, sizeof(stored),
                        cyclical ? sizeof(void *) : std::size_t{0}});
  cost.align = std::max({requests.align, temporaries::align, alignof(stored),
                         cyclical ? alignof(void *) : std::size_t{0}});
  return cost;
}

// The request costs are laid out like the edges of the nodes that know their
// bounds: the request at position k of a node owns the edge at position k.
template <std::size_t Vertices, std::size_t Edges> struct graph_costs {
  std::array<graph_cost, Vertices> nodes;
  std::array<graph_request_cost, Edges> requests;
};

template <std::size_t Edges>
constexpr void
graph_write_request_costs(void *, std::array<graph_request_cost, Edges> &,
                          std::size_t) {}

template <typename... Requests, std::size_t Edges>
constexpr void
graph_write_request_costs(type_list<Requests...> *,
                          std::array<graph_request_cost, Edges> &costs,
                          std::size_t at) {
  ((costs[at++] = graph_request_cost_of<Requests>()), ...);
  (void)costs;
  (void)at;
}

template <typename Binding, std::size_t Vertices, std::size_t Edges>
constexpr void graph_add_cost(graph_costs<Vertices, Edges> &costs,
                              const graph_structure<Vertices, Edges> &structure,
                              std::size_t index) {
  using requests = binding_dependencies_t<typename Binding::binding_model_type>;

  costs.nodes[index] = graph_binding_cost<Binding>();
  if (structure.vertices[index].bounds_known) {
    graph_write_request_costs(static_cast<requests *>(nullptr), costs.requests,
                              structure.vertices[index].edge_begin);
  }
}

template <std::size_t Vertices, std::size_t Edges, typename... Bindings>
constexpr graph_costs<Vertices, Edges>
graph_make_costs(const graph_structure<Vertices, Edges> &structure,
                 type_list<Bindings...>) {
  graph_costs<Vertices, Edges> costs{};
  std::size_t index = 0;
  (graph_add_cost<Bindings>(costs, structure, index++), ...);
  (void)index;
  return costs;
}

// ---------------------------------------------------------------------------
// Bounds: the execution context sizes of a graph.
// ---------------------------------------------------------------------------

struct graph_bounds {
  std::size_t retained_frame_depth;
  std::size_t destructible_slots;
  std::size_t temporary_slots;
  std::size_t temporary_size;
  std::size_t temporary_align;
};

template <std::size_t Vertices, std::size_t Edges>
constexpr bool
graph_roots_bounded(const graph_structure<Vertices, Edges> &structure,
                    std::size_t roots) {
  for (std::size_t i = 0; i < roots; ++i) {
    if (!structure.vertices[i].bounds_known) {
      return false;
    }
  }
  return true;
}

struct graph_slots {
  std::size_t destructible;
  std::size_t temporary;
};

// Largest value of the dependencies of a node.
template <std::size_t Vertices, std::size_t Edges>
constexpr std::size_t
graph_deepest(const graph_structure<Vertices, Edges> &structure,
              const graph_vertex &vertex,
              const std::array<std::size_t, Vertices> &values) {
  std::size_t deepest = 0;
  for (std::size_t i = 0; i < vertex.edge_count; ++i) {
    const std::size_t next = structure.edges[vertex.edge_begin + i];
    if (next != graph_npos) {
      deepest = std::max(deepest, values[next]);
    }
  }
  return deepest;
}

template <std::size_t Vertices> struct graph_peaks {
  std::array<std::size_t, Vertices> frame;
  std::array<std::size_t, Vertices> retained_destructible;
  std::array<std::size_t, Vertices> peak_destructible;
  std::array<std::size_t, Vertices> retained_temporary;
  std::array<std::size_t, Vertices> peak_temporary;
};

// The slots held after the request owning `slot` was resolved by `next`, and
// the largest amount held while it is being resolved.
template <std::size_t Vertices, std::size_t Edges>
constexpr void graph_request_hold(const graph_costs<Vertices, Edges> &costs,
                                  const graph_peaks<Vertices> &peaks,
                                  std::size_t slot, std::size_t next,
                                  graph_slots &retained, graph_slots &peak) {
  retained = graph_slots{};
  peak = graph_slots{};
  if (next == graph_npos) {
    return;
  }

  const graph_request_cost &request = costs.requests[slot];
  const bool held = request.temporary && costs.nodes[next].stable;
  retained.destructible = peaks.retained_destructible[next] +
                          (held ? request.destructible : std::size_t{0});
  retained.temporary = peaks.retained_temporary[next] + (held ? 1 : 0);
  peak.destructible =
      std::max(peaks.peak_destructible[next], retained.destructible);
  peak.temporary = std::max(peaks.peak_temporary[next], retained.temporary);
}

// Peaks of a node whose requests are matched to its edges, which are visited
// from the last request to the first while the slots held by the requests that
// follow are accumulated.
template <std::size_t Vertices, std::size_t Edges>
constexpr void
graph_request_peaks(const graph_structure<Vertices, Edges> &structure,
                    const graph_costs<Vertices, Edges> &costs, std::size_t node,
                    graph_peaks<Vertices> &peaks) {
  const graph_vertex &vertex = structure.vertices[node];
  graph_slots held{};
  graph_slots peak_dependencies{};
  for (std::size_t k = vertex.requests_known ? vertex.edge_count : 0;
       k-- > 0;) {
    const std::size_t slot = vertex.edge_begin + k;
    graph_slots retained{};
    graph_slots peak{};
    graph_request_hold(costs, peaks, slot, structure.edges[slot], retained,
                       peak);
    peak_dependencies.destructible =
        std::max(peak.destructible + held.destructible,
                 retained.destructible + peak_dependencies.destructible);
    peak_dependencies.temporary =
        std::max(peak.temporary + held.temporary,
                 retained.temporary + peak_dependencies.temporary);
    held.destructible += retained.destructible;
    held.temporary += retained.temporary;
  }

  const graph_cost &cost = costs.nodes[node];
  peaks.retained_destructible[node] =
      held.destructible + cost.self_destructible;
  peaks.peak_destructible[node] = std::max(peak_dependencies.destructible,
                                           peaks.retained_destructible[node]);
  peaks.retained_temporary[node] = held.temporary + cost.self_temporary;
  peaks.peak_temporary[node] =
      std::max(peak_dependencies.temporary, peaks.retained_temporary[node]);
}

// Peaks of a node as the largest total along its dependency paths.
template <std::size_t Vertices, std::size_t Edges>
constexpr void
graph_path_peaks(const graph_structure<Vertices, Edges> &structure,
                 const graph_costs<Vertices, Edges> &costs, std::size_t node,
                 graph_peaks<Vertices> &peaks) {
  const graph_vertex &vertex = structure.vertices[node];
  const graph_cost &cost = costs.nodes[node];
  peaks.peak_destructible[node] =
      cost.destructible +
      graph_deepest(structure, vertex, peaks.peak_destructible);
  peaks.peak_temporary[node] =
      cost.temporary + graph_deepest(structure, vertex, peaks.peak_temporary);
}

template <std::size_t Vertices, std::size_t Edges>
constexpr graph_bounds graph_totals(const graph_costs<Vertices, Edges> &costs,
                                    std::size_t roots) {
  graph_bounds totals{};
  for (std::size_t i = 0; i < roots; ++i) {
    totals.retained_frame_depth += costs.nodes[i].frame;
    totals.destructible_slots += costs.nodes[i].destructible;
    totals.temporary_slots += costs.nodes[i].temporary;
    totals.temporary_size =
        std::max(totals.temporary_size, costs.nodes[i].size);
    totals.temporary_align =
        std::max(totals.temporary_align, costs.nodes[i].align);
  }
  return totals;
}

template <std::size_t Vertices, std::size_t Edges>
constexpr graph_bounds
graph_path_bounds(const graph_structure<Vertices, Edges> &structure,
                  const graph_costs<Vertices, Edges> &costs,
                  const graph_topology<Vertices> &topology, std::size_t roots,
                  bool runtime_dependencies) {
  graph_peaks<Vertices> peaks{};
  for (std::size_t position = 0; position < topology.order_size; ++position) {
    const std::size_t node = topology.order[position];
    peaks.frame[node] =
        costs.nodes[node].frame +
        graph_deepest(structure, structure.vertices[node], peaks.frame);
    if (runtime_dependencies) {
      graph_path_peaks(structure, costs, node, peaks);
    } else {
      graph_request_peaks(structure, costs, node, peaks);
    }
  }

  graph_bounds bounds{};
  for (std::size_t i = 0; i < roots; ++i) {
    bounds.retained_frame_depth =
        std::max(bounds.retained_frame_depth, peaks.frame[i]);
    bounds.destructible_slots =
        std::max(bounds.destructible_slots, peaks.peak_destructible[i]);
    bounds.temporary_slots =
        std::max(bounds.temporary_slots, peaks.peak_temporary[i]);
  }
  return bounds;
}

// Bounds of the first `roots` nodes.
//
// Without precise bounds (a root has a collection or unknown requests) or when
// the graph contains a cycle, every binding of the roots is assumed to be
// active at once and the bounds are plain totals. A graph that is not
// resolvable has no slot bounds. Otherwise the bound is the largest amount
// held along any resolution path: a node retains the slots of its dependencies
// that were resolved before it completes, and its peak is the largest amount
// held at any moment while its dependencies are resolved in order. When
// `runtime_dependencies` is set the dependency requests are not matched to
// edges and the path totals are summed instead.
template <std::size_t Vertices, std::size_t Edges>
constexpr graph_bounds
graph_bound(const graph_structure<Vertices, Edges> &structure,
            const graph_costs<Vertices, Edges> &costs,
            const graph_topology<Vertices> &topology, std::size_t roots,
            bool runtime_dependencies) {
  graph_bounds bounds = graph_totals(costs, roots);
  if (!topology.resolvable) {
    bounds.temporary_size = 0;
    bounds.temporary_align = 0;
  }
  if (!graph_roots_bounded(structure, roots)) {
    return bounds;
  }
  if (!topology.resolvable) {
    return graph_bounds{};
  }
  if (topology.contains_cycle) {
    return bounds;
  }

  graph_bounds paths = graph_path_bounds(structure, costs, topology, roots,
                                         runtime_dependencies);
  paths.temporary_size = bounds.temporary_size;
  paths.temporary_align = bounds.temporary_align;
  return paths;
}

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

template <typename StaticRegistry, typename... Universe,
          std::size_t... Position>
auto graph_topological_list(type_list<Universe...>,
                            std::index_sequence<Position...>)
    -> type_list<std::tuple_element_t<
        graph_model<StaticRegistry>::topology.order[Position],
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
