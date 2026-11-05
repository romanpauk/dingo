//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/base/type_list.h>
#include <dingo/registration/type_registration.h>
#include <dingo/resolution/resolution_operation.h>
#include <dingo/static/registry.h>
#include <dingo/storage/storage.h>
#include <dingo/storage/type_storage_traits.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <type_traits>
#include <utility>

namespace dingo {

namespace detail {

// A static binding graph is analysed as constexpr data. The bindings reachable
// from a registry (its interface bindings followed by the local bindings its
// registrations declare) are numbered once, every dependency becomes an index
// into that numbering, and the topology is computed by plain constexpr
// functions over std::array tables. Only the per-binding leaves that read types
// (dependency lists) are templates.

inline constexpr std::size_t graph_npos = static_cast<std::size_t>(-1);

// ---------------------------------------------------------------------------
// Numbering of the bindings of a graph.
// ---------------------------------------------------------------------------

template <typename LocalBindings> struct graph_local_bindings {
  using type = type_list<>;
};

template <typename... LocalRegistrations>
struct graph_local_bindings<static_registry<LocalRegistrations...>> {
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

// ---------------------------------------------------------------------------
// Structure: flags and dependency edges of every node.
// ---------------------------------------------------------------------------

// The edges of a node are a slice of the shared edge array. There is one edge
// per declared dependency request, graph_npos when the request has no binding,
// and one edge per matching binding for a collection request.
struct graph_vertex {
  // The scope of the binding allows it to take part in a cycle.
  bool cyclical;
  std::size_t edge_begin;
  std::size_t edge_count;
};

template <std::size_t Vertices, std::size_t Edges> struct graph_structure {
  std::array<graph_vertex, Vertices> vertices;
  std::array<std::size_t, Edges> edges;
};

// The edges of a binding are the bindings of its dependencies, resolved
// against the host bindings and the local bindings of its model: void, which is
// no node, for a dependency without exactly one binding.
template <typename Host, typename Binding>
using graph_edges_t =
    resolved_dependency_bindings_t<typename Binding::binding_model_type, Host>;

constexpr std::size_t graph_slot_count(void *) { return 0; }

template <typename... Dependencies>
constexpr std::size_t graph_slot_count(type_list<Dependencies...> *) {
  return sizeof...(Dependencies);
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

template <typename Host, typename Universe, typename Binding,
          std::size_t Vertices, std::size_t Edges>
constexpr void graph_add_vertex(graph_structure<Vertices, Edges> &structure,
                                std::size_t &edge, std::size_t vertex) {
  using edges_type = graph_edges_t<Host, Binding>;
  constexpr std::size_t slots =
      graph_slot_count(static_cast<edges_type *>(nullptr));
  structure.vertices[vertex] = graph_vertex{
      scope_allows_cycles_v<typename Binding::binding_model_type::storage_tag>,
      edge, slots};
  graph_write_bindings<Universe>(static_cast<edges_type *>(nullptr),
                                 structure.edges, edge);
  edge += slots;
}

template <typename Host, typename... Bindings>
constexpr auto graph_make_structure(type_list<Bindings...>) {
  using universe = type_list<Bindings...>;
  constexpr std::size_t edges =
      (std::size_t{0} + ... +
       graph_slot_count(static_cast<graph_edges_t<Host, Bindings> *>(nullptr)));

  graph_structure<sizeof...(Bindings), edges> structure{};
  std::size_t vertex = 0;
  std::size_t edge = 0;
  (graph_add_vertex<Host, universe, Bindings>(structure, edge, vertex++), ...);
  (void)vertex;
  (void)edge;
  return structure;
}

// ---------------------------------------------------------------------------
// Resolvability: every cycle is made of bindings whose scopes allow it.
// ---------------------------------------------------------------------------

constexpr unsigned char graph_unvisited = 0;
constexpr unsigned char graph_visiting = 1;
constexpr unsigned char graph_visited = 2;

// A dependency on the node at the given stack position closes a cycle made of
// the nodes visited since; it is legal only when the scopes of all of them
// allow cycles.
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

// Depth-first traversal from the first `roots` nodes, which ends at the first
// cycle containing a binding whose scope does not allow it.
template <std::size_t Vertices, std::size_t Edges>
constexpr bool
graph_resolvable(const graph_structure<Vertices, Edges> &structure,
                 std::size_t roots) {
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
        --depth;
        continue;
      }
      const std::size_t next = structure.edges[cursor[depth - 1]++];
      if (next == graph_npos || state[next] == graph_visited) {
        continue;
      }
      if (state[next] == graph_visiting) {
        if (!graph_cycle_is_legal(structure, stack, depth, next)) {
          return false;
        }
        continue;
      }
      state[next] = graph_visiting;
      stack[depth] = next;
      cursor[depth] = structure.vertices[next].edge_begin;
      ++depth;
    }
  }
  return true;
}

// ---------------------------------------------------------------------------
// Graph of a static registry.
// ---------------------------------------------------------------------------

// The graph of a registry and whether it is resolvable.
template <typename StaticRegistry> struct graph_model {
  using host_bindings = typename StaticRegistry::interface_bindings;
  using universe = typename graph_universe<host_bindings>::type;
  static constexpr auto structure =
      graph_make_structure<host_bindings>(universe{});
  static constexpr bool resolvable =
      graph_resolvable(structure, type_list_size_v<host_bindings>);
};
} // namespace detail
} // namespace dingo
