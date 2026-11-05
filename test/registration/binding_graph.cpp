//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#include <dingo/static/binding.h>
#include <dingo/static/context.h>
#include <dingo/static/graph.h>
#include <dingo/storage/shared.h>
#include <dingo/storage/shared_cyclical.h>
#include <dingo/storage/unique.h>

#include <gtest/gtest.h>

#include <memory>
#include <optional>

using namespace dingo;

TEST(static_graph_test, resolves_an_acyclic_graph) {
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

  static_assert(detail::graph_model<typename source::type>::resolvable);
}

TEST(static_graph_test,
     detects_cycles_without_invalidating_compile_time_bindings_source) {
  struct a {};
  struct b {};

  using a_binding = dingo::bind<scope<shared>, storage<a>, dependencies<b &>>;
  using b_binding = dingo::bind<scope<shared>, storage<b>, dependencies<a &>>;
  using source = dingo::bindings<a_binding, b_binding>;

  static_assert(source::type::valid);
  static_assert(!detail::graph_model<typename source::type>::resolvable);
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

  static_assert(source::type::valid);
  static_assert(detail::graph_model<typename source::type>::resolvable);
}

TEST(static_graph_test,
     rejects_cycles_when_any_cycle_binding_is_not_shared_cyclical) {
  struct a {};
  struct b {};

  using a_binding =
      dingo::bind<scope<shared_cyclical>, storage<a>, dependencies<b &>>;
  using b_binding = dingo::bind<scope<shared>, storage<b>, dependencies<a &>>;
  using source = dingo::bindings<a_binding, b_binding>;

  static_assert(source::type::valid);
  static_assert(!detail::graph_model<typename source::type>::resolvable);
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

  static_assert(source::type::valid);
  static_assert(!detail::graph_model<typename source::type>::resolvable);
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

TEST(static_execution_traits_test,
     default_stable_leaf_requests_select_their_category_route) {
  struct payload {};
  struct service_interface {
    virtual ~service_interface() = default;
  };
  struct service : service_interface {};

  using wrapped_registration =
      dingo::bind<scope<shared>, storage<std::shared_ptr<payload>>>;
  using wrapped_model = detail::binding_model<wrapped_registration>;
  using wrapped_binding = detail::binding<payload, wrapped_model>;
  using wrapped_resolutions = typename detail::request_binding_resolutions<
      payload &, payload, typename wrapped_model::storage_type>::type;
  using wrapped_match =
      typename detail::matching_binding_resolution<payload &,
                                                   wrapped_resolutions>::type;
  using const_wrapped_resolutions =
      typename detail::request_binding_resolutions<
          const payload &, payload, typename wrapped_model::storage_type>::type;
  using const_wrapped_match = typename detail::matching_binding_resolution<
      const payload &, const_wrapped_resolutions>::type;
  using const_pointer_wrapped_resolutions =
      typename detail::request_binding_resolutions<
          const payload *, payload, typename wrapped_model::storage_type>::type;
  using const_pointer_wrapped_match =
      typename detail::matching_binding_resolution<
          const payload *, const_pointer_wrapped_resolutions>::type;

  using converted_registration =
      dingo::bind<scope<shared>, storage<std::shared_ptr<service>>,
                  interfaces<service_interface>>;
  using converted_model = detail::binding_model<converted_registration>;
  using converted_binding = detail::binding<service_interface, converted_model>;
  using converted_resolutions = typename detail::request_binding_resolutions<
      service_interface *, service_interface,
      typename converted_model::storage_type>::type;
  using converted_match =
      typename detail::matching_binding_resolution<service_interface *,
                                                   converted_resolutions>::type;

  using array_registration = dingo::bind<scope<shared>, storage<payload[2]>>;
  using array_model = detail::binding_model<array_registration>;
  using array_binding = detail::binding<payload, array_model>;
  using array_pointer_resolutions =
      typename detail::request_binding_resolutions<
          payload *, payload, typename array_model::storage_type>::type;
  using array_pointer_match = typename detail::matching_binding_resolution<
      payload *, array_pointer_resolutions>::type;

  static_assert(std::is_same_v<typename detail::binding_supports_request<
                                   payload &, wrapped_binding>::type,
                               wrapped_match>);
  static_assert(std::is_same_v<typename detail::binding_supports_request<
                                   const payload &, wrapped_binding>::type,
                               const_wrapped_match>);
  static_assert(std::is_same_v<typename detail::binding_supports_request<
                                   const payload *, wrapped_binding>::type,
                               const_pointer_wrapped_match>);
  static_assert(
      std::is_same_v<typename detail::binding_supports_request<
                         service_interface *, converted_binding>::type,
                     converted_match>);
  static_assert(!detail::binding_supports_request_v<payload &, array_binding>);
  static_assert(std::is_same_v<typename detail::binding_supports_request<
                                   payload *, array_binding>::type,
                               array_pointer_match>);
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

  static_assert(detail::graph_model<typename source::type>::resolvable);
  static_assert(std::is_same_v<
                detail::resolved_dependency_bindings_t<
                    typename source::type::template binding<
                        annotated<service, service_tag>,
                        detail::no_lookup_key_t>::binding_model_type,
                    typename source::type::interface_bindings>,
                type_list<typename source::type::template binding<
                    annotated<config, config_tag>, detail::no_lookup_key_t>>>);
}
