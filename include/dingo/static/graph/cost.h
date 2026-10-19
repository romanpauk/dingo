//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/base/type_list.h>
#include <dingo/exposure/type_storage_traits.h>
#include <dingo/key/annotated.h>
#include <dingo/resolution/resolution.h>
#include <dingo/static/bindings.h>
#include <dingo/static/graph/structure.h>
#include <dingo/static/graph/topology.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <type_traits>

namespace dingo {

struct shared;
struct unique;
struct shared_cyclical;

namespace detail {

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

} // namespace detail
} // namespace dingo
