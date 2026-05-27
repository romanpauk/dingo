//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#include <dingo/static/graph.h>
#include <dingo/core/static_activation_set.h>
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
        explicit service(config&) {}
    };
    struct controller {
        explicit controller(service_interface&) {}
    };

    using config_binding = dingo::bind<scope<shared>, storage<config>>;
    using service_binding =
        dingo::bind<scope<shared>, storage<service>, interfaces<service_interface>,
             dependencies<config&>>;
    using controller_binding =
        dingo::bind<scope<unique>, storage<controller>,
             dependencies<service_interface&>>;
    using source = dingo::bindings<config_binding, service_binding, controller_binding>;
    using graph = static_graph<source>;
    using static_traits = detail::static_execution_traits<typename source::type>;

    static_assert(graph::acyclic);
    static_assert(std::is_same_v<typename graph::dependency_nodes<config>,
                                 type_list<>>);
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
    static_assert(std::is_same_v<
                  typename graph::topological_nodes,
                  type_list<typename graph::node<config>,
                            typename graph::node<service_interface>,
                            typename graph::node<controller>>>);
    static_assert(static_traits::max_preserved_closure_depth == 2);
    static_assert(static_traits::max_destructible_slots == 0);
}

TEST(static_graph_test, detects_cycles_without_invalidating_compile_time_bindings_source) {
    struct a {};
    struct b {};

    using a_binding =
        dingo::bind<scope<shared>, storage<a>, dependencies<b&>>;
    using b_binding =
        dingo::bind<scope<shared>, storage<b>, dependencies<a&>>;
    using source = dingo::bindings<a_binding, b_binding>;
    using graph = static_graph<source>;

    static_assert(source::type::valid);
    static_assert(!graph::acyclic);
    static_assert(std::is_same_v<typename graph::topological_bindings, void>);
    static_assert(std::is_same_v<typename graph::topological_nodes, void>);
}

TEST(static_graph_test,
     partial_analysis_detects_static_cycles_even_with_runtime_dependencies) {
    struct a {};
    struct b {};
    struct runtime_only {};

    using a_binding =
        dingo::bind<scope<shared>, storage<a>, dependencies<b&, runtime_only&>>;
    using b_binding =
        dingo::bind<scope<shared>, storage<b>, dependencies<a&>>;
    using source = dingo::bindings<a_binding, b_binding>;
    using traits = detail::execution_traits<typename source::type, true>;

    static_assert(source::type::valid);
    static_assert(
        !detail::graph_analysis<typename source::type, true>::acyclic);
    static_assert(!traits::acyclic);
}

TEST(static_execution_traits_test,
     unique_only_paths_use_one_temporary_slot_per_unique_binding) {
    struct leaf {};
    struct middle {
        explicit middle(leaf&) {}
    };
    struct root {
        explicit root(middle&) {}
    };

    using source = dingo::bindings<
        dingo::bind<scope<unique>, storage<leaf>>,
        dingo::bind<scope<unique>, storage<middle>, dependencies<leaf&>>,
        dingo::bind<scope<unique>, storage<root>, dependencies<middle&>>>;
    using static_traits = detail::static_execution_traits<typename source::type>;

    static_assert(static_traits::acyclic);
    static_assert(static_traits::max_preserved_closure_depth == 0);
    static_assert(static_traits::max_temporary_slots == 3);
    static_assert(static_traits::max_destructible_slots == 0);
    static_assert(static_traits::max_temporary_size >= sizeof(root));
}

TEST(static_execution_traits_test,
     sibling_unique_dependencies_do_not_accumulate_nested_peaks) {
    struct left {};
    struct right {};
    struct root {
        root(left&, right&) {}
    };

    using source = dingo::bindings<
        dingo::bind<scope<unique>, storage<left>>,
        dingo::bind<scope<unique>, storage<right>>,
        dingo::bind<scope<unique>, storage<root>, dependencies<left&, right&>>>;
    using static_traits = detail::static_execution_traits<typename source::type>;

    static_assert(static_traits::acyclic);
    static_assert(static_traits::max_temporary_slots == 3);
    static_assert(static_traits::max_destructible_slots == 0);
}

TEST(static_execution_traits_test,
     static_conversion_cost_traits_live_with_storage_types) {
    struct payload {
        ~payload() {}
    };

    using unique_value_storage = detail::conversions<unique, payload, payload>;
    using unique_handle_storage =
        detail::conversions<unique, std::shared_ptr<payload>, payload>;
    using shared_handle_storage =
        detail::conversions<shared, std::shared_ptr<payload>, payload>;

    static_assert(
        unique_value_storage::static_conversion_temporary_slots == 1);
    static_assert(
        unique_value_storage::static_conversion_destructible_slots == 1);
    static_assert(
        unique_value_storage::static_conversion_temporary_size ==
        sizeof(std::optional<payload>));
    static_assert(
        unique_value_storage::static_conversion_temporary_align ==
        alignof(std::optional<payload>));
    static_assert(
        unique_handle_storage::static_conversion_temporary_slots == 0);
    static_assert(
        unique_handle_storage::static_conversion_destructible_slots == 0);
    static_assert(
        unique_handle_storage::static_conversion_temporary_size == 0);
    static_assert(
        unique_handle_storage::static_conversion_temporary_align == 0);
    static_assert(
        shared_handle_storage::static_conversion_temporary_slots == 0);
    static_assert(
        shared_handle_storage::static_conversion_destructible_slots == 0);
    static_assert(
        shared_handle_storage::static_conversion_temporary_size == 0);
    static_assert(
        shared_handle_storage::static_conversion_temporary_align == 0);
}

TEST(static_execution_traits_test,
     pure_static_closures_do_not_use_runtime_closure_base) {
    struct payload {
        ~payload() {}
    };

    using registration = dingo::bind<scope<unique>, storage<payload>>;
    using pure_closure =
        detail::static_activation_closure_t<false, registration>;
    using partial_closure =
        detail::static_activation_closure_t<true, registration>;

    static_assert(
        !std::is_base_of_v<detail::context_closure_base, pure_closure>);
    static_assert(
        std::is_base_of_v<detail::context_closure_base, partial_closure>);
}

TEST(static_execution_traits_test,
     preserved_closure_depth_counts_preserving_bindings_across_non_preserving_edges) {
    struct config {};
    struct transient_service {
        explicit transient_service(config&) {}
    };
    struct leaf {
        explicit leaf(transient_service&) {}
    };
    struct shared_leaf {
        explicit shared_leaf(leaf&) {}
    };

    using source = dingo::bindings<
        dingo::bind<scope<shared>, storage<config>>,
        dingo::bind<scope<unique>, storage<transient_service>,
             dependencies<config&>>,
        dingo::bind<scope<unique>, storage<leaf>, dependencies<transient_service&>>,
        dingo::bind<scope<shared>, storage<shared_leaf>, dependencies<leaf&>>>;
    using static_traits = detail::static_execution_traits<typename source::type>;

    static_assert(static_traits::acyclic);
    static_assert(static_traits::max_preserved_closure_depth == 2);
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
    static_assert(static_traits::max_preserved_closure_depth == 0);
    static_assert(static_traits::max_destructible_slots == 1);
    static_assert(static_traits::max_temporary_slots == 1);
    static_assert(static_traits::max_temporary_size >= sizeof(std::shared_ptr<payload>));
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
    static_assert(static_traits::max_preserved_closure_depth == 0);
    static_assert(static_traits::max_temporary_slots == 1);
    static_assert(static_traits::max_destructible_slots == 1);
    static_assert(static_traits::max_temporary_size >= sizeof(std::optional<payload>));
    static_assert(static_traits::max_temporary_align >=
                  alignof(std::optional<payload>));
}

TEST(static_execution_traits_test,
     shared_cyclical_paths_account_for_rollback_temporary_slots) {
    struct node {};

    using source = dingo::bindings<dingo::bind<scope<shared_cyclical>, storage<node>>>;
    using static_traits = detail::static_execution_traits<typename source::type>;

    static_assert(static_traits::acyclic);
    static_assert(static_traits::max_preserved_closure_depth == 0);
    static_assert(static_traits::max_destructible_slots == 1);
    static_assert(static_traits::max_temporary_slots == 1);
    static_assert(static_traits::max_temporary_size >= sizeof(void*));
    static_assert(static_traits::max_temporary_align >= alignof(void*));
}

TEST(static_execution_traits_test,
     destructible_slots_follow_non_trivial_dependency_requests_along_static_paths) {
    struct config {
        virtual ~config() = default;
    };
    struct payload {
        explicit payload(config&) {}
        virtual ~payload() = default;
    };
    struct service {
        explicit service(std::shared_ptr<payload>, std::optional<payload>) {}
        virtual ~service() = default;
    };

    using source = dingo::bindings<
        dingo::bind<scope<shared>, storage<config>>,
        dingo::bind<scope<unique>, storage<payload>, dependencies<config&>>,
        dingo::bind<scope<unique>, storage<service>,
             dependencies<std::shared_ptr<payload>, std::optional<payload>>>>;
    using static_traits = detail::static_execution_traits<typename source::type>;

    static_assert(static_traits::acyclic);
    static_assert(static_traits::max_destructible_slots == 3);
    static_assert(static_traits::max_temporary_slots == 3);
    static_assert(static_traits::max_temporary_size >= sizeof(std::optional<payload>));
    static_assert(static_traits::max_temporary_align >=
                  alignof(std::optional<payload>));
}

TEST(static_graph_test, annotated_bindings_reuse_compile_time_bindings_lookup) {
    struct service_tag {};
    struct config_tag {};
    struct config {};
    struct service {
        explicit service(annotated<config&, config_tag>) {}
    };

    using config_binding =
        dingo::bind<scope<shared>, storage<config>,
             interfaces<annotated<config, config_tag>>>;
    using service_binding =
        dingo::bind<scope<unique>, storage<service>,
             interfaces<annotated<service, service_tag>>,
             dependencies<annotated<config&, config_tag>>>;
    using source = dingo::bindings<config_binding, service_binding>;
    using graph = static_graph<source>;

    static_assert(graph::acyclic);
    static_assert(
        std::is_same_v<
            typename graph::dependency_nodes<annotated<service, service_tag>>,
            type_list<typename graph::node<annotated<config, config_tag>>>>);
}
