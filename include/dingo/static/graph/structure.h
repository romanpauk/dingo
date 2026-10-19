//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/base/type_list.h>
#include <dingo/dependency/collection.h>
#include <dingo/dependency/normalized_type.h>
#include <dingo/factory/traits.h>
#include <dingo/key/key.h>
#include <dingo/static/bindings.h>

#include <array>
#include <cstddef>
#include <type_traits>

namespace dingo {

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

} // namespace detail
} // namespace dingo
