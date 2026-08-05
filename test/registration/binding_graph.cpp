//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#include <dingo/static/activation_set.h>
#include <dingo/static/graph.h>
#include <dingo/storage/shared.h>
#include <dingo/storage/shared_cyclical.h>
#include <dingo/storage/unique.h>

#include <gtest/gtest.h>

#include <memory>
#include <optional>

using namespace dingo;

TEST(static_graph_test, exposes_dependency_nodes_and_topological_order) {
  struct config {};
  struct service_interface {
    virtual ~service_interface() = default;
  };
  struct service : service_interface {
    explicit service(config &) {}
  };
  struct controller {
    explicit controller(service_interface &) {}
  };

  using config_binding = dingo::bind<scope<shared>, storage<config>>;
  using service_binding =
      dingo::bind<scope<shared>, storage<service>,
                  interfaces<service_interface>, dependencies<config &>>;
  using controller_binding = dingo::bind<scope<unique>, storage<controller>,
                                         dependencies<service_interface &>>;
  using source =
      dingo::bindings<config_binding, service_binding, controller_binding>;
  using graph = static_graph<source>;
  using static_traits = detail::static_execution_traits<typename source::type>;

  static_assert(graph::resolvable);
  static_assert(graph::acyclic);
  static_assert(
      std::is_same_v<typename graph::dependency_nodes<config>, type_list<>>);
  static_assert(
      std::is_same_v<typename graph::dependency_nodes<service_interface>,
                     type_list<typename graph::node<config>>>);
  static_assert(
      std::is_same_v<typename graph::dependency_nodes<controller>,
                     type_list<typename graph::node<service_interface>>>);
  static_assert(std::is_same_v<
                typename graph::topological_bindings,
                type_list<typename graph::template binding<config>,
                          typename graph::template binding<service_interface>,
                          typename graph::template binding<controller>>>);
  static_assert(
      std::is_same_v<typename graph::topological_nodes,
                     type_list<typename graph::node<config>,
                               typename graph::node<service_interface>,
                               typename graph::node<controller>>>);
  static_assert(static_traits::max_retained_frame_depth == 2);
  static_assert(static_traits::max_destructible_slots == 0);
}

TEST(static_graph_test,
     detects_cycles_without_invalidating_compile_time_bindings_source) {
  struct a {};
  struct b {};

  using a_binding = dingo::bind<scope<shared>, storage<a>, dependencies<b &>>;
  using b_binding = dingo::bind<scope<shared>, storage<b>, dependencies<a &>>;
  using source = dingo::bindings<a_binding, b_binding>;
  using graph = static_graph<source>;

  static_assert(source::type::valid);
  static_assert(!graph::resolvable);
  static_assert(!graph::acyclic);
  static_assert(std::is_same_v<typename graph::topological_bindings, void>);
  static_assert(std::is_same_v<typename graph::topological_nodes, void>);
}

TEST(static_graph_test,
     allows_cycles_when_every_cycle_binding_is_shared_cyclical) {
  struct a {};
  struct b {};
  struct owner {};

  using a_binding =
      dingo::bind<scope<shared_cyclical>, storage<a>, dependencies<b &>>;
  using b_binding =
      dingo::bind<scope<shared_cyclical>, storage<b>, dependencies<a &>>;
  using owner_binding =
      dingo::bind<scope<shared>, storage<owner>, dependencies<a &>>;
  using source = dingo::bindings<a_binding, b_binding, owner_binding>;
  using graph = static_graph<source>;

  static_assert(source::type::valid);
  static_assert(graph::resolvable);
  static_assert(!graph::acyclic);
  static_assert(graph::contains_cycle);
  static_assert(std::is_void_v<typename graph::topological_bindings>);
  static_assert(
      detail::static_binding_resolvable_v<
          typename source::type::template binding<owner,
                                                  detail::no_lookup_key_t>,
          typename source::type>);
}

TEST(static_graph_test,
     rejects_cycles_when_any_cycle_binding_is_not_shared_cyclical) {
  struct a {};
  struct b {};

  using a_binding =
      dingo::bind<scope<shared_cyclical>, storage<a>, dependencies<b &>>;
  using b_binding = dingo::bind<scope<shared>, storage<b>, dependencies<a &>>;
  using source = dingo::bindings<a_binding, b_binding>;
  using graph = static_graph<source>;

  static_assert(source::type::valid);
  static_assert(!graph::resolvable);
  static_assert(!graph::acyclic);
  static_assert(
      !detail::static_binding_resolvable_v<
          typename source::type::template binding<a, detail::no_lookup_key_t>,
          typename source::type>);
}

TEST(static_graph_test,
     partial_analysis_detects_static_cycles_even_with_runtime_dependencies) {
  struct a {};
  struct b {};
  struct runtime_only {};

  using a_binding =
      dingo::bind<scope<shared>, storage<a>, dependencies<b &, runtime_only &>>;
  using b_binding = dingo::bind<scope<shared>, storage<b>, dependencies<a &>>;
  using source = dingo::bindings<a_binding, b_binding>;
  using traits = detail::execution_traits<typename source::type, true>;

  static_assert(source::type::valid);
  static_assert(!detail::graph_analysis<typename source::type, true>::acyclic);
  static_assert(
      !detail::graph_analysis<typename source::type, true>::resolvable);
  static_assert(!traits::resolvable);
  static_assert(!traits::acyclic);
}

TEST(static_execution_traits_test,
     unique_only_paths_use_one_temporary_slot_per_unique_binding) {
  struct leaf {};
  struct middle {
    explicit middle(leaf &) {}
  };
  struct root {
    explicit root(middle &) {}
  };

  using source = dingo::bindings<
      dingo::bind<scope<unique>, storage<leaf>>,
      dingo::bind<scope<unique>, storage<middle>, dependencies<leaf &>>,
      dingo::bind<scope<unique>, storage<root>, dependencies<middle &>>>;
  using static_traits = detail::static_execution_traits<typename source::type>;

  static_assert(static_traits::acyclic);
  static_assert(static_traits::max_retained_frame_depth == 0);
  static_assert(static_traits::max_temporary_slots == 3);
  static_assert(static_traits::max_destructible_slots == 0);
  static_assert(static_traits::max_temporary_size >= sizeof(root));
}

TEST(static_execution_traits_test,
     sibling_unique_dependencies_do_not_accumulate_nested_peaks) {
  struct left {};
  struct right {};
  struct root {
    root(left &, right &) {}
  };

  using source = dingo::bindings<
      dingo::bind<scope<unique>, storage<left>>,
      dingo::bind<scope<unique>, storage<right>>,
      dingo::bind<scope<unique>, storage<root>, dependencies<left &, right &>>>;
  using static_traits = detail::static_execution_traits<typename source::type>;

  static_assert(static_traits::acyclic);
  static_assert(static_traits::max_temporary_slots == 3);
  static_assert(static_traits::max_destructible_slots == 0);
}

TEST(static_execution_traits_test,
     detected_auto_constructor_dependencies_control_context_bounds) {
  struct left {};
  struct right {};
  struct root {
    explicit root(left &) {}
  };

  using source = dingo::bindings<dingo::bind<scope<shared>, storage<left>>,
                                 dingo::bind<scope<shared>, storage<right>>,
                                 dingo::bind<scope<unique>, storage<root>>>;
  using static_traits = detail::static_execution_traits<typename source::type>;
  using detection = constructor_detection<root>;

  static_assert(static_traits::acyclic);
  static_assert(static_traits::static_context_eligible ==
                !std::is_void_v<typename detection::arguments>);
  static_assert(static_traits::max_retained_frame_depth ==
                (static_traits::static_context_eligible ? 1 : 2));
}

TEST(static_execution_traits_test,
     resolution_operations_publish_static_temporary_requirements) {
  struct payload {
    ~payload() {}
  };
  struct larger_payload {
    alignas(16) char bytes[32];
  };

  using registration = dingo::bind<scope<unique>, storage<payload>>;
  using model = detail::binding_model<registration>;
  using storage_type = typename model::storage_type;
  using resolutions =
      typename detail::binding_resolutions<payload, storage_type>::type;
  using temporary_types =
      detail::resolution_temporary_types_t<resolutions, storage_type>;
  using temporary_traits = detail::temporary_storage_traits<temporary_types>;

  static_assert(
      std::is_same_v<temporary_types, type_list<std::optional<payload>>>);
  static_assert(temporary_traits::slots == 1);
  static_assert(temporary_traits::destructible_slots == 1);
  static_assert(temporary_traits::size == sizeof(std::optional<payload>));
  static_assert(temporary_traits::align == alignof(std::optional<payload>));

  using exact_types = detail::resolution_temporary_types_t<
      type_list<detail::conversion_resolution<payload, payload &&>>,
      storage_type>;
  using larger_types = detail::resolution_temporary_types_t<
      type_list<detail::conversion_resolution<larger_payload, payload &&>>,
      storage_type>;
  static_assert(std::is_same_v<exact_types, type_list<>>);
  static_assert(std::is_same_v<larger_types, type_list<larger_payload>>);
}

TEST(static_execution_traits_test,
     pure_static_frames_do_not_use_runtime_frame_base) {
  struct payload {
    ~payload() {}
  };

  using registration = dingo::bind<scope<unique>, storage<payload>>;
  using pure_frame = detail::static_activation_frame_t<false, registration>;
  using partial_frame = detail::static_activation_frame_t<true, registration>;

  static_assert(
      !std::is_base_of_v<detail::static_context_frame_base, pure_frame>);
  static_assert(
      std::is_base_of_v<detail::static_context_frame_base, partial_frame>);
}

TEST(static_execution_traits_test,
     default_stable_leaf_bindings_skip_conversion_cache_discovery) {
  struct payload {};
  struct service_interface {
    virtual ~service_interface() = default;
  };
  struct service : service_interface {};
  struct custom_conversions
      : detail::conversions<shared, std::shared_ptr<payload>, runtime_type> {};

  using direct_registration = dingo::bind<scope<shared>, storage<payload>>;
  using direct_model = detail::binding_model<direct_registration>;
  using wrapped_registration =
      dingo::bind<scope<shared>, storage<std::shared_ptr<payload>>>;
  using wrapped_model = detail::binding_model<wrapped_registration>;
  using converted_registration =
      dingo::bind<scope<shared>, storage<std::shared_ptr<service>>,
                  interfaces<service_interface>>;
  using converted_model = detail::binding_model<converted_registration>;
  using custom_registration =
      dingo::bind<scope<shared>, storage<std::shared_ptr<payload>>,
                  conversions<custom_conversions>>;
  using custom_model = detail::binding_model<custom_registration>;
  using unstable_registration = dingo::bind<scope<unique>, storage<payload>>;
  using unstable_model = detail::binding_model<unstable_registration>;

  static_assert(detail::binding_has_cacheless_conversion_shape_v<direct_model>);
  static_assert(
      std::is_same_v<detail::binding_cache_types_t<direct_model>, type_list<>>);
  static_assert(detail::binding_uses_default_conversions_v<wrapped_model>);
  static_assert(
      detail::binding_has_cacheless_default_leaf_v<payload, wrapped_model>);
  static_assert(std::is_same_v<detail::binding_cache_types_t<wrapped_model>,
                               type_list<>>);
  static_assert(
      !detail::binding_has_cacheless_conversion_shape_v<converted_model>);
  static_assert(detail::binding_has_cacheless_default_leaf_v<service_interface,
                                                             converted_model>);
  static_assert(std::is_same_v<detail::binding_cache_types_t<converted_model>,
                               type_list<>>);
  static_assert(!detail::binding_uses_default_conversions_v<custom_model>);
  static_assert(
      !detail::binding_has_cacheless_default_leaf_v<payload, custom_model>);
  static_assert(
      !detail::binding_has_cacheless_default_leaf_v<payload, unstable_model>);
}

TEST(
    static_execution_traits_test,
    retained_frame_depth_counts_retaining_bindings_across_non_retaining_edges) {
  struct config {};
  struct transient_service {
    explicit transient_service(config &) {}
  };
  struct leaf {
    explicit leaf(transient_service &) {}
  };
  struct shared_leaf {
    explicit shared_leaf(leaf &) {}
  };

  using source = dingo::bindings<
      dingo::bind<scope<shared>, storage<config>>,
      dingo::bind<scope<unique>, storage<transient_service>,
                  dependencies<config &>>,
      dingo::bind<scope<unique>, storage<leaf>,
                  dependencies<transient_service &>>,
      dingo::bind<scope<shared>, storage<shared_leaf>, dependencies<leaf &>>>;
  using static_traits = detail::static_execution_traits<typename source::type>;

  static_assert(static_traits::acyclic);
  static_assert(static_traits::max_retained_frame_depth == 2);
  static_assert(static_traits::max_temporary_slots == 2);
  static_assert(static_traits::max_destructible_slots == 0);
}

TEST(static_execution_traits_test,
     unique_wrapper_storage_paths_account_for_wrapper_materialization_slots) {
  struct payload {};

  using source = dingo::bindings<
      dingo::bind<scope<unique>, storage<std::shared_ptr<payload>>>>;
  using static_traits = detail::static_execution_traits<typename source::type>;

  static_assert(static_traits::acyclic);
  static_assert(static_traits::max_retained_frame_depth == 0);
  static_assert(static_traits::max_destructible_slots == 1);
  static_assert(static_traits::max_temporary_slots == 1);
  static_assert(static_traits::max_temporary_size >=
                sizeof(std::shared_ptr<payload>));
  static_assert(static_traits::max_temporary_align >=
                alignof(std::shared_ptr<payload>));
}

TEST(static_execution_traits_test,
     unique_value_storage_paths_account_for_optional_conversion_shape) {
  struct payload {
    ~payload() {}
  };

  using source = dingo::bindings<dingo::bind<scope<unique>, storage<payload>>>;
  using static_traits = detail::static_execution_traits<typename source::type>;

  static_assert(static_traits::acyclic);
  static_assert(static_traits::max_retained_frame_depth == 0);
  static_assert(static_traits::max_temporary_slots == 1);
  static_assert(static_traits::max_destructible_slots == 1);
  static_assert(static_traits::max_temporary_size >=
                sizeof(std::optional<payload>));
  static_assert(static_traits::max_temporary_align >=
                alignof(std::optional<payload>));
}

TEST(static_execution_traits_test,
     shared_cyclical_paths_account_for_rollback_temporary_slots) {
  struct node {};

  using source =
      dingo::bindings<dingo::bind<scope<shared_cyclical>, storage<node>>>;
  using static_traits = detail::static_execution_traits<typename source::type>;

  static_assert(static_traits::acyclic);
  static_assert(static_traits::max_retained_frame_depth == 0);
  static_assert(static_traits::max_destructible_slots == 1);
  static_assert(static_traits::max_temporary_slots == 1);
  static_assert(static_traits::max_temporary_size >= sizeof(void *));
  static_assert(static_traits::max_temporary_align >= alignof(void *));
}

TEST(
    static_execution_traits_test,
    destructible_slots_follow_non_trivial_dependency_requests_along_static_paths) {
  struct config {
    virtual ~config() = default;
  };
  struct payload {
    explicit payload(config &) {}
    virtual ~payload() = default;
  };
  struct service {
    explicit service(std::shared_ptr<payload>, std::optional<payload>) {}
    virtual ~service() = default;
  };

  using source = dingo::bindings<
      dingo::bind<scope<shared>, storage<config>>,
      dingo::bind<scope<unique>, storage<payload>, dependencies<config &>>,
      dingo::bind<
          scope<unique>, storage<service>,
          dependencies<std::shared_ptr<payload>, std::optional<payload>>>>;
  using static_traits = detail::static_execution_traits<typename source::type>;

  static_assert(static_traits::acyclic);
  static_assert(static_traits::max_destructible_slots == 3);
  static_assert(static_traits::max_temporary_slots == 3);
  static_assert(static_traits::max_temporary_size >=
                sizeof(std::optional<payload>));
  static_assert(static_traits::max_temporary_align >=
                alignof(std::optional<payload>));
}

TEST(static_graph_test, annotated_bindings_reuse_compile_time_bindings_lookup) {
  struct service_tag {};
  struct config_tag {};
  struct config {};
  struct service {
    explicit service(annotated<config &, config_tag>) {}
  };

  using config_binding = dingo::bind<scope<shared>, storage<config>,
                                     interfaces<annotated<config, config_tag>>>;
  using service_binding =
      dingo::bind<scope<unique>, storage<service>,
                  interfaces<annotated<service, service_tag>>,
                  dependencies<annotated<config &, config_tag>>>;
  using source = dingo::bindings<config_binding, service_binding>;
  using graph = static_graph<source>;

  static_assert(graph::acyclic);
  static_assert(
      std::is_same_v<
          typename graph::dependency_nodes<annotated<service, service_tag>>,
          type_list<typename graph::node<annotated<config, config_tag>>>>);
}
