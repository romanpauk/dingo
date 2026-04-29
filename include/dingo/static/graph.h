//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/core/container_common.h>
#include <dingo/registration/type_registration.h>
#include <dingo/static/registry.h>
#include <dingo/storage/type_storage_traits.h>
#include <dingo/type/type_list.h>

#include <algorithm>
#include <type_traits>

namespace dingo {

struct shared;
struct unique;
struct shared_cyclical;

namespace detail {

template <typename Binding, typename DependencyBindings> struct graph_node {
    using binding_type = Binding;
    using dependency_bindings = DependencyBindings;
};

template <typename DependencyBindings>
struct filter_resolved_dependency_bindings;

template <> struct filter_resolved_dependency_bindings<void> {
    using type = type_list<>;
};

template <> struct filter_resolved_dependency_bindings<type_list<>> {
    using type = type_list<>;
};

template <typename Head, typename... Tail>
struct filter_resolved_dependency_bindings<type_list<Head, Tail...>> {
  private:
    using tail_type =
        typename filter_resolved_dependency_bindings<type_list<Tail...>>::type;

  public:
    using type =
        std::conditional_t<std::is_void_v<Head>, tail_type,
                           type_list_cat_t<type_list<Head>, tail_type>>;
};

template <typename DependencyBindings>
using filter_resolved_dependency_bindings_t =
    typename filter_resolved_dependency_bindings<DependencyBindings>::type;

template <typename T, typename List> struct type_list_contains;

template <typename T>
struct type_list_contains<T, type_list<>> : std::false_type {};

template <typename T, typename Head, typename... Tail>
struct type_list_contains<T, type_list<Head, Tail...>>
    : std::bool_constant<std::is_same_v<T, Head> ||
                         type_list_contains<T, type_list<Tail...>>::value> {};

template <typename T, typename List>
inline constexpr bool type_list_contains_v = type_list_contains<T, List>::value;

template <typename InterfaceBinding, typename StaticRegistry,
          bool RuntimeDependencies = false>
struct static_graph_node {
    using binding_model_type = typename InterfaceBinding::binding_model_type;
    using resolved_dependency_bindings = resolved_dependency_bindings_t<
        binding_model_type, typename StaticRegistry::interface_bindings>;
    using type = graph_node<
        InterfaceBinding,
        resolved_dependency_bindings_t<
            binding_model_type, typename StaticRegistry::interface_bindings>>;
};

template <typename StaticRegistry, bool RuntimeDependencies>
struct static_graph_node<void, StaticRegistry, RuntimeDependencies> {
    using type = void;
};

template <typename InterfaceBinding, typename StaticRegistry>
struct static_graph_node<InterfaceBinding, StaticRegistry, true> {
    using binding_model_type = typename InterfaceBinding::binding_model_type;
    using resolved_dependency_bindings = resolved_dependency_bindings_t<
        binding_model_type, typename StaticRegistry::interface_bindings>;
    using type = graph_node<
        InterfaceBinding,
        filter_resolved_dependency_bindings_t<resolved_dependency_bindings>>;
};

template <typename InterfaceBinding, typename StaticRegistry,
          bool RuntimeDependencies = false>
using static_graph_node_t =
    typename static_graph_node<InterfaceBinding, StaticRegistry,
                               RuntimeDependencies>::type;

template <typename Bindings, typename StaticRegistry,
          typename Visiting = type_list<>>
struct static_bindings_resolvable;

template <typename Binding, typename StaticRegistry,
          typename Visiting = type_list<>,
          bool InVisiting = type_list_contains_v<Binding, Visiting>>
struct static_binding_resolvable;

template <typename StaticRegistry, typename Visiting>
struct static_binding_resolvable<void, StaticRegistry, Visiting, false>
    : std::false_type {};

template <typename Binding, typename StaticRegistry, typename Visiting>
struct static_binding_resolvable<Binding, StaticRegistry, Visiting, true>
    : std::false_type {};

template <typename Binding, typename StaticRegistry, typename Visiting>
struct static_binding_resolvable<Binding, StaticRegistry, Visiting, false>
    : static_bindings_resolvable<
          typename static_graph_node_t<Binding, StaticRegistry,
                                       false>::dependency_bindings,
          StaticRegistry, type_list_cat_t<Visiting, type_list<Binding>>> {};

template <typename StaticRegistry, typename Visiting>
struct static_bindings_resolvable<type_list<>, StaticRegistry, Visiting>
    : std::true_type {};

template <typename StaticRegistry, typename Visiting>
struct static_bindings_resolvable<void, StaticRegistry, Visiting>
    : std::false_type {};

template <typename... Bindings, typename StaticRegistry, typename Visiting>
struct static_bindings_resolvable<type_list<Bindings...>, StaticRegistry,
                                  Visiting>
    : std::bool_constant<(static_binding_resolvable<Bindings, StaticRegistry,
                                                    Visiting>::value &&
                          ...)> {};

template <typename Binding, typename StaticRegistry>
inline constexpr bool static_binding_resolvable_v =
    static_binding_resolvable<Binding, StaticRegistry>::value;

template <typename Bindings, typename StaticRegistry>
inline constexpr bool static_bindings_resolvable_v =
    static_bindings_resolvable<Bindings, StaticRegistry>::value;

template <typename InterfaceBindings, typename StaticRegistry,
          bool RuntimeDependencies = false>
struct static_graph_nodes;

template <typename StaticRegistry, bool RuntimeDependencies>
struct static_graph_nodes<type_list<>, StaticRegistry, RuntimeDependencies> {
    using type = type_list<>;
};

template <typename StaticRegistry, bool RuntimeDependencies>
struct static_graph_nodes<void, StaticRegistry, RuntimeDependencies> {
    using type = void;
};

template <typename... InterfaceBindings, typename StaticRegistry,
          bool RuntimeDependencies>
struct static_graph_nodes<type_list<InterfaceBindings...>, StaticRegistry,
                          RuntimeDependencies> {
    using type =
        type_list<static_graph_node_t<InterfaceBindings, StaticRegistry,
                                      RuntimeDependencies>...>;
};

template <typename InterfaceBindings, typename StaticRegistry,
          bool RuntimeDependencies = false>
using static_graph_nodes_t =
    typename static_graph_nodes<InterfaceBindings, StaticRegistry,
                                RuntimeDependencies>::type;

template <typename DependencyBindings, typename StaticRegistry>
struct dependency_graph_nodes;

template <typename StaticRegistry>
struct dependency_graph_nodes<void, StaticRegistry> {
    using type = void;
};

template <typename StaticRegistry>
struct dependency_graph_nodes<type_list<>, StaticRegistry> {
    using type = type_list<>;
};

template <typename... DependencyBindings, typename StaticRegistry>
struct dependency_graph_nodes<type_list<DependencyBindings...>,
                              StaticRegistry> {
    using type =
        type_list<static_graph_node_t<DependencyBindings, StaticRegistry>...>;
};

template <typename DependencyBindings, typename StaticRegistry>
using dependency_graph_nodes_t =
    typename dependency_graph_nodes<DependencyBindings, StaticRegistry>::type;

template <typename StorageTag>
struct static_preserves_closure : std::false_type {};

template <> struct static_preserves_closure<shared> : std::true_type {};

template <typename StorageTag>
inline constexpr bool static_preserves_closure_v =
    static_preserves_closure<StorageTag>::value;

template <typename StorageTag>
struct static_storage_rollback_cost : std::integral_constant<std::size_t, 0> {};

template <>
struct static_storage_rollback_cost<shared_cyclical>
    : std::integral_constant<std::size_t, 1> {};

template <typename StorageTag>
inline constexpr std::size_t static_storage_rollback_cost_v =
    static_storage_rollback_cost<StorageTag>::value;

template <typename StorageTag>
struct static_storage_temporary_slot_cost
    : std::integral_constant<std::size_t, 0> {};

template <>
struct static_storage_temporary_slot_cost<unique>
    : std::integral_constant<std::size_t, 1> {};

template <>
struct static_storage_temporary_slot_cost<shared_cyclical>
    : std::integral_constant<std::size_t, 1> {};

template <typename StorageTag>
inline constexpr std::size_t static_storage_temporary_slot_cost_v =
    static_storage_temporary_slot_cost<StorageTag>::value;

template <typename StorageTag>
struct static_storage_temporary_size : std::integral_constant<std::size_t, 0> {
};

template <>
struct static_storage_temporary_size<shared_cyclical>
    : std::integral_constant<std::size_t, sizeof(void*)> {};

template <typename StorageTag>
inline constexpr std::size_t static_storage_temporary_size_v =
    static_storage_temporary_size<StorageTag>::value;

template <typename StorageTag>
struct static_storage_temporary_align : std::integral_constant<std::size_t, 0> {
};

template <>
struct static_storage_temporary_align<shared_cyclical>
    : std::integral_constant<std::size_t, alignof(void*)> {};

template <typename StorageTag>
inline constexpr std::size_t static_storage_temporary_align_v =
    static_storage_temporary_align<StorageTag>::value;

template <typename Request>
using static_request_type_t = std::remove_cv_t<
    std::remove_reference_t<typename annotated_traits<Request>::type>>;

template <typename Request>
inline constexpr bool static_request_uses_temporary_slot_v =
    !std::is_reference_v<typename annotated_traits<Request>::type> &&
    !std::is_pointer_v<typename annotated_traits<Request>::type>;

template <typename Request>
inline constexpr std::size_t static_request_temporary_slot_cost_v =
    static_request_uses_temporary_slot_v<Request> ? 1 : 0;

template <typename Request>
inline constexpr std::size_t static_request_temporary_size_v =
    static_request_uses_temporary_slot_v<Request>
        ? sizeof(static_request_type_t<Request>)
        : 0;

template <typename Request>
inline constexpr std::size_t static_request_temporary_align_v =
    static_request_uses_temporary_slot_v<Request>
        ? alignof(static_request_type_t<Request>)
        : 0;

template <typename Request>
inline constexpr std::size_t static_request_destructible_cost_v =
    !std::is_trivially_destructible_v<
        std::remove_cv_t<std::remove_reference_t<Request>>>
        ? 1
        : 0;

template <typename InterfaceBinding>
inline constexpr bool static_binding_is_stable_v =
    InterfaceBinding::binding_model_type::storage_type::conversions::is_stable;

template <typename InterfaceBinding>
inline constexpr std::size_t static_binding_self_temporary_slot_cost_v =
    std::max({static_conversion_temporary_slots_v<
                  typename InterfaceBinding::binding_model_type::storage_type>,
              static_storage_temporary_slot_cost_v<
                  typename InterfaceBinding::binding_model_type::storage_tag>,
              std::size_t{0}});

template <typename InterfaceBinding>
inline constexpr std::size_t static_binding_self_destructible_slot_cost_v =
    std::max(
        {static_conversion_destructible_slots_v<
             typename InterfaceBinding::binding_model_type::storage_type>,
         static_binding_self_temporary_slot_cost_v<InterfaceBinding> != 0 &&
                 !std::is_trivially_destructible_v<
                     typename InterfaceBinding::binding_model_type::
                         storage_type::type>
             ? std::size_t{1}
             : std::size_t{0},
         static_storage_rollback_cost_v<
             typename InterfaceBinding::binding_model_type::storage_tag>,
         std::size_t{0}});

template <typename Dependencies> struct static_dependency_destructible_cost;

template <typename Dependencies> struct static_dependency_temporary_slot_cost;

template <typename Dependencies> struct static_dependency_temporary_size;

template <typename Dependencies> struct static_dependency_temporary_align;

template <typename Requests, typename Bindings, typename StaticRegistry>
struct static_dependency_retained_destructible_slots;

template <typename Requests, typename Bindings, typename StaticRegistry>
struct static_dependency_peak_destructible_slots;

template <typename Request, typename Binding, typename StaticRegistry>
struct static_request_retained_destructible_slots;

template <typename Request, typename Binding, typename StaticRegistry>
struct static_request_peak_destructible_slots;

template <typename Binding, typename StaticRegistry>
struct static_binding_retained_destructible_slots;

template <typename Binding, typename StaticRegistry>
struct static_binding_peak_destructible_slots;

template <typename Requests, typename Bindings, typename StaticRegistry>
struct static_dependency_retained_temporary_slots;

template <typename Requests, typename Bindings, typename StaticRegistry>
struct static_dependency_peak_temporary_slots;

template <typename Request, typename Binding, typename StaticRegistry>
struct static_request_retained_temporary_slots;

template <typename Request, typename Binding, typename StaticRegistry>
struct static_request_peak_temporary_slots;

template <typename Binding, typename StaticRegistry>
struct static_binding_retained_temporary_slots;

template <typename Binding, typename StaticRegistry>
struct static_binding_peak_temporary_slots;

template <typename Request, typename StaticRegistry>
struct static_request_retained_destructible_slots<Request, void, StaticRegistry>
    : std::integral_constant<std::size_t, 0> {};

template <typename Request, typename StaticRegistry>
struct static_request_peak_destructible_slots<Request, void, StaticRegistry>
    : std::integral_constant<std::size_t, 0> {};

template <typename StaticRegistry>
struct static_binding_retained_destructible_slots<void, StaticRegistry>
    : std::integral_constant<std::size_t, 0> {};

template <typename StaticRegistry>
struct static_binding_peak_destructible_slots<void, StaticRegistry>
    : std::integral_constant<std::size_t, 0> {};

template <typename Request, typename StaticRegistry>
struct static_request_retained_temporary_slots<Request, void, StaticRegistry>
    : std::integral_constant<std::size_t, 0> {};

template <typename Request, typename StaticRegistry>
struct static_request_peak_temporary_slots<Request, void, StaticRegistry>
    : std::integral_constant<std::size_t, 0> {};

template <typename StaticRegistry>
struct static_binding_retained_temporary_slots<void, StaticRegistry>
    : std::integral_constant<std::size_t, 0> {};

template <typename StaticRegistry>
struct static_binding_peak_temporary_slots<void, StaticRegistry>
    : std::integral_constant<std::size_t, 0> {};

template <>
struct static_dependency_destructible_cost<void>
    : std::integral_constant<std::size_t, 0> {};

template <>
struct static_dependency_destructible_cost<type_list<>>
    : std::integral_constant<std::size_t, 0> {};

template <typename... Dependencies>
struct static_dependency_destructible_cost<type_list<Dependencies...>>
    : std::integral_constant<std::size_t,
                             (static_request_destructible_cost_v<Dependencies> +
                              ... + std::size_t{0})> {};

template <>
struct static_dependency_temporary_slot_cost<void>
    : std::integral_constant<std::size_t, 0> {};

template <>
struct static_dependency_temporary_slot_cost<type_list<>>
    : std::integral_constant<std::size_t, 0> {};

template <typename... Dependencies>
struct static_dependency_temporary_slot_cost<type_list<Dependencies...>>
    : std::integral_constant<
          std::size_t, (static_request_temporary_slot_cost_v<Dependencies> +
                        ... + std::size_t{0})> {};

template <>
struct static_dependency_temporary_size<void>
    : std::integral_constant<std::size_t, 0> {};

template <>
struct static_dependency_temporary_size<type_list<>>
    : std::integral_constant<std::size_t, 0> {};

template <typename... Dependencies>
struct static_dependency_temporary_size<type_list<Dependencies...>>
    : std::integral_constant<
          std::size_t,
          std::max({static_request_temporary_size_v<Dependencies>...,
                    std::size_t{0}})> {};

template <>
struct static_dependency_temporary_align<void>
    : std::integral_constant<std::size_t, 0> {};

template <>
struct static_dependency_temporary_align<type_list<>>
    : std::integral_constant<std::size_t, 0> {};

template <typename... Dependencies>
struct static_dependency_temporary_align<type_list<Dependencies...>>
    : std::integral_constant<
          std::size_t,
          std::max({static_request_temporary_align_v<Dependencies>...,
                    std::size_t{0}})> {};

template <typename InterfaceBinding>
inline constexpr std::size_t static_binding_preserved_closure_cost_v =
    static_preserves_closure_v<
        typename InterfaceBinding::binding_model_type::storage_tag>
        ? 1
        : 0;

template <typename InterfaceBinding>
inline constexpr std::size_t static_binding_destructible_slot_cost_v =
    static_dependency_destructible_cost<
        typename InterfaceBinding::binding_model_type::dependencies_type::
            type>::value +
    static_conversion_destructible_slots_v<
        typename InterfaceBinding::binding_model_type::storage_type> +
    (!std::is_trivially_destructible_v<
         typename InterfaceBinding::binding_model_type::storage_type::type>
         ? 1
         : 0) +
    static_storage_rollback_cost_v<
        typename InterfaceBinding::binding_model_type::storage_tag>;

template <typename InterfaceBinding>
inline constexpr std::size_t static_binding_temporary_slot_cost_v =
    static_dependency_temporary_slot_cost<
        typename InterfaceBinding::binding_model_type::dependencies_type::
            type>::value +
    static_conversion_temporary_slots_v<
        typename InterfaceBinding::binding_model_type::storage_type> +
    static_storage_temporary_slot_cost_v<
        typename InterfaceBinding::binding_model_type::storage_tag>;

template <typename InterfaceBinding>
inline constexpr std::size_t static_binding_max_temporary_size_v = std::max(
    {static_dependency_temporary_size<
         typename InterfaceBinding::binding_model_type::dependencies_type::
             type>::value,
     static_conversion_temporary_size_v<
         typename InterfaceBinding::binding_model_type::storage_type>,
     sizeof(typename InterfaceBinding::binding_model_type::storage_type::type),
     static_storage_temporary_size_v<
         typename InterfaceBinding::binding_model_type::storage_tag>,
     std::size_t{0}});

template <typename InterfaceBinding>
inline constexpr std::size_t static_binding_max_temporary_align_v = std::max(
    {static_dependency_temporary_align<
         typename InterfaceBinding::binding_model_type::dependencies_type::
             type>::value,
     static_conversion_temporary_align_v<
         typename InterfaceBinding::binding_model_type::storage_type>,
     alignof(typename InterfaceBinding::binding_model_type::storage_type::type),
     static_storage_temporary_align_v<
         typename InterfaceBinding::binding_model_type::storage_tag>,
     std::size_t{0}});

template <typename StaticRegistry>
struct static_dependency_retained_destructible_slots<type_list<>, type_list<>,
                                                     StaticRegistry>
    : std::integral_constant<std::size_t, 0> {};

template <typename Bindings, typename StaticRegistry>
struct static_dependency_retained_destructible_slots<void, Bindings,
                                                     StaticRegistry>
    : std::integral_constant<std::size_t, 0> {};

template <typename RequestHead, typename... RequestTail, typename BindingHead,
          typename... BindingTail, typename StaticRegistry>
struct static_dependency_retained_destructible_slots<
    type_list<RequestHead, RequestTail...>,
    type_list<BindingHead, BindingTail...>, StaticRegistry>
    : std::integral_constant<
          std::size_t,
          static_request_retained_destructible_slots<RequestHead, BindingHead,
                                                     StaticRegistry>::value +
              static_dependency_retained_destructible_slots<
                  type_list<RequestTail...>, type_list<BindingTail...>,
                  StaticRegistry>::value> {};

template <typename StaticRegistry>
struct static_dependency_peak_destructible_slots<type_list<>, type_list<>,
                                                 StaticRegistry>
    : std::integral_constant<std::size_t, 0> {};

template <typename Bindings, typename StaticRegistry>
struct static_dependency_peak_destructible_slots<void, Bindings, StaticRegistry>
    : std::integral_constant<std::size_t, 0> {};

template <typename RequestHead, typename... RequestTail, typename BindingHead,
          typename... BindingTail, typename StaticRegistry>
struct static_dependency_peak_destructible_slots<
    type_list<RequestHead, RequestTail...>,
    type_list<BindingHead, BindingTail...>, StaticRegistry>
    : std::integral_constant<
          std::size_t,
          std::max(static_request_peak_destructible_slots<
                       RequestHead, BindingHead, StaticRegistry>::value +
                       static_dependency_retained_destructible_slots<
                           type_list<RequestTail...>, type_list<BindingTail...>,
                           StaticRegistry>::value,
                   static_request_retained_destructible_slots<
                       RequestHead, BindingHead, StaticRegistry>::value +
                       static_dependency_peak_destructible_slots<
                           type_list<RequestTail...>, type_list<BindingTail...>,
                           StaticRegistry>::value)> {};

template <typename Request, typename Binding, typename StaticRegistry>
struct static_request_retained_destructible_slots
    : std::integral_constant<
          std::size_t, static_binding_retained_destructible_slots<
                           Binding, StaticRegistry>::value +
                           (static_request_uses_temporary_slot_v<Request> &&
                                    static_binding_is_stable_v<Binding>
                                ? static_request_destructible_cost_v<Request>
                                : std::size_t{0})> {};

template <typename Request, typename Binding, typename StaticRegistry>
struct static_request_peak_destructible_slots
    : std::integral_constant<
          std::size_t, std::max(static_binding_peak_destructible_slots<
                                    Binding, StaticRegistry>::value,
                                static_request_retained_destructible_slots<
                                    Request, Binding, StaticRegistry>::value)> {
};

template <typename Binding, typename StaticRegistry>
struct static_binding_retained_destructible_slots
    : std::integral_constant<
          std::size_t,
          static_dependency_retained_destructible_slots<
              typename Binding::binding_model_type::dependencies_type::type,
              typename static_graph_node_t<Binding, StaticRegistry,
                                           false>::dependency_bindings,
              StaticRegistry>::value +
              static_binding_self_destructible_slot_cost_v<Binding>> {};

template <typename Binding, typename StaticRegistry>
struct static_binding_peak_destructible_slots
    : std::integral_constant<
          std::size_t,
          std::max(
              static_dependency_peak_destructible_slots<
                  typename Binding::binding_model_type::dependencies_type::type,
                  typename static_graph_node_t<Binding, StaticRegistry,
                                               false>::dependency_bindings,
                  StaticRegistry>::value,
              static_binding_retained_destructible_slots<
                  Binding, StaticRegistry>::value)> {};

template <typename StaticRegistry>
struct static_dependency_retained_temporary_slots<type_list<>, type_list<>,
                                                  StaticRegistry>
    : std::integral_constant<std::size_t, 0> {};

template <typename Bindings, typename StaticRegistry>
struct static_dependency_retained_temporary_slots<void, Bindings,
                                                  StaticRegistry>
    : std::integral_constant<std::size_t, 0> {};

template <typename RequestHead, typename... RequestTail, typename BindingHead,
          typename... BindingTail, typename StaticRegistry>
struct static_dependency_retained_temporary_slots<
    type_list<RequestHead, RequestTail...>,
    type_list<BindingHead, BindingTail...>, StaticRegistry>
    : std::integral_constant<
          std::size_t,
          static_request_retained_temporary_slots<RequestHead, BindingHead,
                                                  StaticRegistry>::value +
              static_dependency_retained_temporary_slots<
                  type_list<RequestTail...>, type_list<BindingTail...>,
                  StaticRegistry>::value> {};

template <typename StaticRegistry>
struct static_dependency_peak_temporary_slots<type_list<>, type_list<>,
                                              StaticRegistry>
    : std::integral_constant<std::size_t, 0> {};

template <typename Bindings, typename StaticRegistry>
struct static_dependency_peak_temporary_slots<void, Bindings, StaticRegistry>
    : std::integral_constant<std::size_t, 0> {};

template <typename RequestHead, typename... RequestTail, typename BindingHead,
          typename... BindingTail, typename StaticRegistry>
struct static_dependency_peak_temporary_slots<
    type_list<RequestHead, RequestTail...>,
    type_list<BindingHead, BindingTail...>, StaticRegistry>
    : std::integral_constant<
          std::size_t,
          std::max(static_request_peak_temporary_slots<RequestHead, BindingHead,
                                                       StaticRegistry>::value +
                       static_dependency_retained_temporary_slots<
                           type_list<RequestTail...>, type_list<BindingTail...>,
                           StaticRegistry>::value,
                   static_request_retained_temporary_slots<
                       RequestHead, BindingHead, StaticRegistry>::value +
                       static_dependency_peak_temporary_slots<
                           type_list<RequestTail...>, type_list<BindingTail...>,
                           StaticRegistry>::value)> {};

template <typename Request, typename Binding, typename StaticRegistry>
struct static_request_retained_temporary_slots
    : std::integral_constant<
          std::size_t, static_binding_retained_temporary_slots<
                           Binding, StaticRegistry>::value +
                           (static_request_uses_temporary_slot_v<Request> &&
                                    static_binding_is_stable_v<Binding>
                                ? std::size_t{1}
                                : std::size_t{0})> {};

template <typename Request, typename Binding, typename StaticRegistry>
struct static_request_peak_temporary_slots
    : std::integral_constant<
          std::size_t, std::max(static_binding_peak_temporary_slots<
                                    Binding, StaticRegistry>::value,
                                static_request_retained_temporary_slots<
                                    Request, Binding, StaticRegistry>::value)> {
};

template <typename Binding, typename StaticRegistry>
struct static_binding_retained_temporary_slots
    : std::integral_constant<
          std::size_t,
          static_dependency_retained_temporary_slots<
              typename Binding::binding_model_type::dependencies_type::type,
              typename static_graph_node_t<Binding, StaticRegistry,
                                           false>::dependency_bindings,
              StaticRegistry>::value +
              static_binding_self_temporary_slot_cost_v<Binding>> {};

template <typename Binding, typename StaticRegistry>
struct static_binding_peak_temporary_slots
    : std::integral_constant<
          std::size_t,
          std::max(
              static_dependency_peak_temporary_slots<
                  typename Binding::binding_model_type::dependencies_type::type,
                  typename static_graph_node_t<Binding, StaticRegistry,
                                               false>::dependency_bindings,
                  StaticRegistry>::value,
              static_binding_retained_temporary_slots<Binding,
                                                      StaticRegistry>::value)> {
};

template <typename Bindings, typename StaticRegistry,
          bool RuntimeDependencies = false>
struct static_graph_max_preserved_closure_depth_all;

template <typename StaticRegistry, bool RuntimeDependencies, bool Acyclic>
struct static_graph_max_preserved_closure_depth;

template <typename Binding, typename StaticRegistry,
          bool RuntimeDependencies = false>
struct static_binding_graph_preserved_closure_depth;

template <typename Bindings, typename StaticRegistry,
          bool RuntimeDependencies = false>
struct static_graph_max_destructible_slots_all;

template <typename StaticRegistry, bool RuntimeDependencies, bool Acyclic>
struct static_graph_max_destructible_slots;

template <typename Binding, typename StaticRegistry,
          bool RuntimeDependencies = false>
struct static_binding_graph_destructible_slots;

template <typename Bindings> struct static_graph_max_temporary_size_all;

template <typename StaticRegistry, bool Acyclic>
struct static_graph_max_temporary_size;

template <typename Bindings> struct static_graph_max_temporary_align_all;

template <typename StaticRegistry, bool Acyclic>
struct static_graph_max_temporary_align;

template <typename Bindings, typename StaticRegistry,
          bool RuntimeDependencies = false>
struct static_graph_max_temporary_slots_all;

template <typename StaticRegistry, bool RuntimeDependencies, bool Acyclic>
struct static_graph_max_temporary_slots;

template <typename Binding, typename StaticRegistry,
          bool RuntimeDependencies = false>
struct static_binding_graph_temporary_slots;

template <typename StaticRegistry, bool RuntimeDependencies>
struct static_graph_max_preserved_closure_depth_all<type_list<>, StaticRegistry,
                                                    RuntimeDependencies>
    : std::integral_constant<std::size_t, 0> {};

template <typename StaticRegistry, bool RuntimeDependencies>
struct static_graph_max_preserved_closure_depth_all<void, StaticRegistry,
                                                    RuntimeDependencies>
    : std::integral_constant<std::size_t, 0> {};

template <typename... Bindings, typename StaticRegistry,
          bool RuntimeDependencies>
struct static_graph_max_preserved_closure_depth_all<
    type_list<Bindings...>, StaticRegistry, RuntimeDependencies>
    : std::integral_constant<
          std::size_t, std::max({static_binding_graph_preserved_closure_depth<
                                     Bindings, StaticRegistry,
                                     RuntimeDependencies>::value...,
                                 std::size_t{0}})> {};

template <typename Binding, typename StaticRegistry, bool RuntimeDependencies>
struct static_binding_graph_preserved_closure_depth
    : std::integral_constant<
          std::size_t, static_binding_preserved_closure_cost_v<Binding> +
                           static_graph_max_preserved_closure_depth_all<
                               typename static_graph_node_t<
                                   Binding, StaticRegistry,
                                   RuntimeDependencies>::dependency_bindings,
                               StaticRegistry, RuntimeDependencies>::value> {};

template <typename StaticRegistry, bool RuntimeDependencies>
struct static_binding_graph_preserved_closure_depth<void, StaticRegistry,
                                                    RuntimeDependencies>
    : std::integral_constant<std::size_t, 0> {};

template <typename StaticRegistry, bool RuntimeDependencies>
struct static_graph_max_destructible_slots_all<type_list<>, StaticRegistry,
                                               RuntimeDependencies>
    : std::integral_constant<std::size_t, 0> {};

template <typename StaticRegistry, bool RuntimeDependencies>
struct static_graph_max_destructible_slots_all<void, StaticRegistry,
                                               RuntimeDependencies>
    : std::integral_constant<std::size_t, 0> {};

template <typename... Bindings, typename StaticRegistry,
          bool RuntimeDependencies>
struct static_graph_max_destructible_slots_all<
    type_list<Bindings...>, StaticRegistry, RuntimeDependencies>
    : std::integral_constant<std::size_t,
                             std::max({static_binding_graph_destructible_slots<
                                           Bindings, StaticRegistry,
                                           RuntimeDependencies>::value...,
                                       std::size_t{0}})> {};

template <typename Binding, typename StaticRegistry, bool RuntimeDependencies>
struct static_binding_graph_destructible_slots
    : std::integral_constant<
          std::size_t, static_binding_destructible_slot_cost_v<Binding> +
                           static_graph_max_destructible_slots_all<
                               typename static_graph_node_t<
                                   Binding, StaticRegistry,
                                   RuntimeDependencies>::dependency_bindings,
                               StaticRegistry, RuntimeDependencies>::value> {};

template <typename StaticRegistry, bool RuntimeDependencies>
struct static_binding_graph_destructible_slots<void, StaticRegistry,
                                               RuntimeDependencies>
    : std::integral_constant<std::size_t, 0> {};

template <typename Binding, typename StaticRegistry>
struct static_binding_graph_destructible_slots<Binding, StaticRegistry, false>
    : static_binding_peak_destructible_slots<Binding, StaticRegistry> {};

template <typename StaticRegistry, bool RuntimeDependencies>
struct static_graph_max_preserved_closure_depth<StaticRegistry,
                                                RuntimeDependencies, false>
    : std::integral_constant<std::size_t, 0> {};

template <typename StaticRegistry, bool RuntimeDependencies>
struct static_graph_max_preserved_closure_depth<StaticRegistry,
                                                RuntimeDependencies, true>
    : static_graph_max_preserved_closure_depth_all<
          typename StaticRegistry::interface_bindings, StaticRegistry,
          RuntimeDependencies> {};

template <typename StaticRegistry, bool RuntimeDependencies>
struct static_graph_max_destructible_slots<StaticRegistry, RuntimeDependencies,
                                           false>
    : std::integral_constant<std::size_t, 0> {};

template <typename StaticRegistry, bool RuntimeDependencies>
struct static_graph_max_destructible_slots<StaticRegistry, RuntimeDependencies,
                                           true>
    : static_graph_max_destructible_slots_all<
          typename StaticRegistry::interface_bindings, StaticRegistry,
          RuntimeDependencies> {};

template <>
struct static_graph_max_temporary_size_all<type_list<>>
    : std::integral_constant<std::size_t, 0> {};

template <typename... Bindings>
struct static_graph_max_temporary_size_all<type_list<Bindings...>>
    : std::integral_constant<
          std::size_t,
          std::max({static_binding_max_temporary_size_v<Bindings>...,
                    std::size_t{0}})> {};

template <typename StaticRegistry>
struct static_graph_max_temporary_size<StaticRegistry, false>
    : std::integral_constant<std::size_t, 0> {};

template <typename StaticRegistry>
struct static_graph_max_temporary_size<StaticRegistry, true>
    : static_graph_max_temporary_size_all<
          typename StaticRegistry::interface_bindings> {};

template <>
struct static_graph_max_temporary_align_all<type_list<>>
    : std::integral_constant<std::size_t, 0> {};

template <typename... Bindings>
struct static_graph_max_temporary_align_all<type_list<Bindings...>>
    : std::integral_constant<
          std::size_t,
          std::max({static_binding_max_temporary_align_v<Bindings>...,
                    std::size_t{0}})> {};

template <typename StaticRegistry>
struct static_graph_max_temporary_align<StaticRegistry, false>
    : std::integral_constant<std::size_t, 0> {};

template <typename StaticRegistry>
struct static_graph_max_temporary_align<StaticRegistry, true>
    : static_graph_max_temporary_align_all<
          typename StaticRegistry::interface_bindings> {};

template <typename StaticRegistry, bool RuntimeDependencies>
struct static_graph_max_temporary_slots_all<type_list<>, StaticRegistry,
                                            RuntimeDependencies>
    : std::integral_constant<std::size_t, 0> {};

template <typename StaticRegistry, bool RuntimeDependencies>
struct static_graph_max_temporary_slots_all<void, StaticRegistry,
                                            RuntimeDependencies>
    : std::integral_constant<std::size_t, 0> {};

template <typename... Bindings, typename StaticRegistry,
          bool RuntimeDependencies>
struct static_graph_max_temporary_slots_all<type_list<Bindings...>,
                                            StaticRegistry, RuntimeDependencies>
    : std::integral_constant<std::size_t,
                             std::max({static_binding_graph_temporary_slots<
                                           Bindings, StaticRegistry,
                                           RuntimeDependencies>::value...,
                                       std::size_t{0}})> {};

template <typename Binding, typename StaticRegistry, bool RuntimeDependencies>
struct static_binding_graph_temporary_slots
    : std::integral_constant<
          std::size_t, static_binding_temporary_slot_cost_v<Binding> +
                           static_graph_max_temporary_slots_all<
                               typename static_graph_node_t<
                                   Binding, StaticRegistry,
                                   RuntimeDependencies>::dependency_bindings,
                               StaticRegistry, RuntimeDependencies>::value> {};

template <typename StaticRegistry, bool RuntimeDependencies>
struct static_binding_graph_temporary_slots<void, StaticRegistry,
                                            RuntimeDependencies>
    : std::integral_constant<std::size_t, 0> {};

template <typename Binding, typename StaticRegistry>
struct static_binding_graph_temporary_slots<Binding, StaticRegistry, false>
    : static_binding_peak_temporary_slots<Binding, StaticRegistry> {};

template <typename StaticRegistry, bool RuntimeDependencies>
struct static_graph_max_temporary_slots<StaticRegistry, RuntimeDependencies,
                                        false>
    : std::integral_constant<std::size_t, 0> {};

template <typename StaticRegistry, bool RuntimeDependencies>
struct static_graph_max_temporary_slots<StaticRegistry, RuntimeDependencies,
                                        true>
    : static_graph_max_temporary_slots_all<
          typename StaticRegistry::interface_bindings, StaticRegistry,
          RuntimeDependencies> {};

template <bool Acyclic, typename Visited, typename Order>
struct graph_visit_result {
    static constexpr bool acyclic = Acyclic;
    using visited = Visited;
    using order = Order;
};

template <typename HeadResult, typename TailResult, bool Acyclic>
struct graph_visit_merge;

template <typename HeadResult, typename TailResult>
struct graph_visit_merge<HeadResult, TailResult, true> {
    using type =
        graph_visit_result<true, typename TailResult::visited,
                           type_list_cat_t<typename HeadResult::order,
                                           typename TailResult::order>>;
};

template <typename HeadResult, typename TailResult>
struct graph_visit_merge<HeadResult, TailResult, false> {
    using type = graph_visit_result<false, typename TailResult::visited, void>;
};

template <typename RecurseResult, typename Binding, bool Acyclic>
struct graph_visit_append;

template <typename RecurseResult, typename Binding>
struct graph_visit_append<RecurseResult, Binding, true> {
    using type = graph_visit_result<
        true,
        type_list_cat_t<typename RecurseResult::visited, type_list<Binding>>,
        type_list_cat_t<typename RecurseResult::order, type_list<Binding>>>;
};

template <typename RecurseResult, typename Binding>
struct graph_visit_append<RecurseResult, Binding, false> {
    using type =
        graph_visit_result<false, typename RecurseResult::visited, void>;
};

template <typename Bindings, typename StaticRegistry, typename Visiting,
          typename Visited, bool RuntimeDependencies>
struct graph_visit_all;

template <typename Binding, typename StaticRegistry, typename Visiting,
          typename Visited, bool RuntimeDependencies>
struct graph_visit_one;

template <typename StaticRegistry, typename Visiting, typename Visited,
          bool RuntimeDependencies>
struct graph_visit_all<type_list<>, StaticRegistry, Visiting, Visited,
                       RuntimeDependencies> {
    using type = graph_visit_result<true, Visited, type_list<>>;
};

template <typename StaticRegistry, typename Visiting, typename Visited,
          bool RuntimeDependencies>
struct graph_visit_all<void, StaticRegistry, Visiting, Visited,
                       RuntimeDependencies> {
    using type = graph_visit_result<true, Visited, type_list<>>;
};

template <typename Head, typename... Tail, typename StaticRegistry,
          typename Visiting, typename Visited, bool RuntimeDependencies>
struct graph_visit_all<type_list<Head, Tail...>, StaticRegistry, Visiting,
                       Visited, RuntimeDependencies> {
  private:
    using head_result =
        typename graph_visit_one<Head, StaticRegistry, Visiting, Visited,
                                 RuntimeDependencies>::type;

    using tail_result = std::conditional_t<
        head_result::acyclic,
        typename graph_visit_all<type_list<Tail...>, StaticRegistry, Visiting,
                                 typename head_result::visited,
                                 RuntimeDependencies>::type,
        graph_visit_result<false, typename head_result::visited, void>>;

  public:
    using type = typename graph_visit_merge<head_result, tail_result,
                                            head_result::acyclic &&
                                                tail_result::acyclic>::type;
};

template <typename Binding, typename StaticRegistry, typename Visiting,
          typename Visited, bool RuntimeDependencies, bool InVisiting,
          bool InVisited>
struct graph_visit_one_impl;

template <typename Binding, typename StaticRegistry, typename Visiting,
          typename Visited, bool RuntimeDependencies>
struct graph_visit_one_impl<Binding, StaticRegistry, Visiting, Visited,
                            RuntimeDependencies, true, false> {
    using type = graph_visit_result<false, Visited, void>;
};

template <typename Binding, typename StaticRegistry, typename Visiting,
          typename Visited, bool RuntimeDependencies>
struct graph_visit_one_impl<Binding, StaticRegistry, Visiting, Visited,
                            RuntimeDependencies, false, true> {
    using type = graph_visit_result<true, Visited, type_list<>>;
};

template <typename Binding, typename StaticRegistry, typename Visiting,
          typename Visited, bool RuntimeDependencies>
struct graph_visit_one_impl<Binding, StaticRegistry, Visiting, Visited,
                            RuntimeDependencies, false, false> {
  private:
    using node =
        static_graph_node_t<Binding, StaticRegistry, RuntimeDependencies>;

    using recurse_result =
        typename graph_visit_all<typename node::dependency_bindings,
                                 StaticRegistry,
                                 type_list_cat_t<Visiting, type_list<Binding>>,
                                 Visited, RuntimeDependencies>::type;

  public:
    using type = typename graph_visit_append<recurse_result, Binding,
                                             recurse_result::acyclic>::type;
};

template <typename Binding, typename StaticRegistry, typename Visiting,
          typename Visited, bool RuntimeDependencies>
struct graph_visit_one {
    using type = typename graph_visit_one_impl<
        Binding, StaticRegistry, Visiting, Visited, RuntimeDependencies,
        type_list_contains_v<Binding, Visiting>,
        type_list_contains_v<Binding, Visited>>::type;
};

template <typename StaticRegistry, typename Visiting, typename Visited,
          bool RuntimeDependencies>
struct graph_visit_one<void, StaticRegistry, Visiting, Visited,
                       RuntimeDependencies> {
    using type = graph_visit_result<true, Visited, type_list<>>;
};

template <typename StaticRegistry, bool RuntimeDependencies = false>
struct basic_static_graph_topology_analysis {
  private:
    using traversal =
        typename graph_visit_all<typename StaticRegistry::interface_bindings,
                                 StaticRegistry, type_list<>, type_list<>,
                                 RuntimeDependencies>::type;

  public:
    static constexpr bool acyclic = traversal::acyclic;
    using topological_bindings =
        std::conditional_t<acyclic, typename traversal::order, void>;
};

template <typename StaticRegistry, bool RuntimeDependencies = false>
struct basic_static_execution_traits {
  private:
    using topology = basic_static_graph_topology_analysis<StaticRegistry,
                                                          RuntimeDependencies>;

  public:
    static constexpr bool acyclic = topology::acyclic;
    static constexpr std::size_t max_preserved_closure_depth =
        static_graph_max_preserved_closure_depth<
            StaticRegistry, RuntimeDependencies, acyclic>::value;
    static constexpr std::size_t max_destructible_slots =
        static_graph_max_destructible_slots<StaticRegistry, RuntimeDependencies,
                                            acyclic>::value;
    static constexpr std::size_t max_temporary_slots =
        static_graph_max_temporary_slots<StaticRegistry, RuntimeDependencies,
                                         acyclic>::value;
    static constexpr std::size_t max_temporary_size =
        static_graph_max_temporary_size<StaticRegistry, acyclic>::value;
    static constexpr std::size_t max_temporary_align =
        static_graph_max_temporary_align<StaticRegistry, acyclic>::value;
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
struct static_graph<static_registry<Registrations...>, void>
    : private detail::static_registry_dependency_diagnostics<
          typename static_registry<Registrations...>::interface_bindings,
          detail::binding_model<Registrations>...> {
    using static_registry_type = static_registry<Registrations...>;
    using interface_bindings =
        typename static_registry_type::interface_bindings;
    using nodes =
        detail::static_graph_nodes_t<interface_bindings, static_registry_type>;

    static_assert(static_registry_type::valid,
                  "static_graph requires a valid compile-time bindings source");

    static constexpr bool acyclic =
        detail::graph_analysis<static_registry_type>::acyclic;
    using topological_bindings = typename detail::graph_analysis<
        static_registry_type>::topological_bindings;
    using topological_nodes =
        detail::static_graph_nodes_t<topological_bindings,
                                     static_registry_type>;

    template <typename Interface>
    using binding = typename static_registry_type::template binding<Interface>;

    template <typename Interface>
    using node =
        detail::static_graph_node_t<binding<Interface>, static_registry_type>;

    template <typename Interface>
    using dependency_bindings =
        typename static_registry_type::template dependency_bindings<Interface>;

    template <typename Interface>
    using dependency_nodes =
        detail::dependency_graph_nodes_t<dependency_bindings<Interface>,
                                         static_registry_type>;
};

} // namespace dingo
