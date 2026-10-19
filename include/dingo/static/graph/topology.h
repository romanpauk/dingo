//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/static/graph/structure.h>

#include <array>
#include <cstddef>

namespace dingo {

namespace detail {

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

} // namespace detail
} // namespace dingo
