//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#include <dingo/container.h>
#include <dingo/registration/constructor.h>
#include <dingo/static/registry.h>
#include <dingo/static_container.h>
#include <dingo/storage/shared.h>
#include <dingo/storage/shared_cyclical.h>
#include <dingo/storage/unique.h>

#include <gtest/gtest.h>

#include <memory>

using namespace dingo;

TEST(static_bindings_source_test, exposes_models_and_dependency_bindings) {
  struct config {};
  struct service_interface {
    virtual ~service_interface() = default;
  };
  struct service : service_interface {
    explicit service(config &) {}
  };

  using config_binding = dingo::bind<scope<shared>, storage<config>>;
  using service_binding =
      dingo::bind<scope<unique>, storage<service>,
                  interfaces<service_interface>, dependencies<config &>>;
  using source = dingo::bindings<config_binding, service_binding>;
  using registry_type = typename source::type;

  static_assert(registry_type::valid);
  static_assert(std::is_same_v<dependency<config &>, config &>);
  static_assert(std::is_same_v<typename registry_type::registration_types,
                               type_list<config_binding, service_binding>>);
  static_assert(
      std::is_same_v<typename registry_type::model<config>::registration_type,
                     config_binding>);
  static_assert(
      std::is_same_v<
          typename registry_type::model<service_interface>::registration_type,
          service_binding>);
  static_assert(
      std::is_same_v<typename registry_type::dependencies<service_interface>,
                     type_list<config &>>);
  static_assert(
      std::is_same_v<
          typename registry_type::dependency_bindings<service_interface>,
          type_list<typename registry_type::binding<config,
                                                    detail::no_lookup_key_t>>>);
}

TEST(static_bindings_source_test, zero_argument_detected_constructor_is_known) {
  struct config {};

  using source = dingo::bindings<dingo::bind<scope<unique>, storage<config>>>;
  using registry_type = typename source::type;

  static_assert(registry_type::valid);
  static_assert(std::is_same_v<typename registry_type::dependencies<config>,
                               type_list<>>);
  static_assert(
      std::is_same_v<typename registry_type::dependency_bindings<config>,
                     type_list<>>);
}

TEST(static_bindings_source_test,
     constructor_typedef_bindings_reuse_detected_dependencies) {
  struct config {};
  struct service {
    DINGO_CONSTRUCTOR(service(config &)) {}
  };

  using config_binding = dingo::bind<scope<shared>, storage<config>>;
  using service_binding = dingo::bind<scope<unique>, storage<service>>;
  using source = dingo::bindings<config_binding, service_binding>;
  using registry_type = typename source::type;

  static_assert(registry_type::valid);
  static_assert(std::is_same_v<typename registry_type::dependencies<service>,
                               type_list<config &>>);
  static_assert(
      std::is_same_v<typename registry_type::dependency_bindings<service>,
                     type_list<typename registry_type::binding<
                         config, detail::no_lookup_key_t>>>);
}

TEST(static_bindings_source_test,
     plain_constructor_dependencies_follow_default_detection_metadata) {
  struct config {};
  struct logger {};
  struct service {
    service(config &, logger &) {}
  };

  using source = dingo::bindings<dingo::bind<scope<shared>, storage<config>>,
                                 dingo::bind<scope<shared>, storage<logger>>,
                                 dingo::bind<scope<unique>, storage<service>>>;
  using registry_type = typename source::type;
  using dependencies = typename constructor_detection<service>::arguments;
  using expected_dependency_bindings = std::conditional_t<
      std::is_void_v<dependencies>, void,
      type_list<
          typename registry_type::binding<config, detail::no_lookup_key_t>,
          typename registry_type::binding<logger, detail::no_lookup_key_t>>>;

  static_assert(registry_type::valid);
  static_assert(std::is_same_v<typename registry_type::dependencies<service>,
                               dependencies>);
  static_assert(
      std::is_same_v<typename registry_type::dependency_bindings<service>,
                     expected_dependency_bindings>);
}

TEST(static_bindings_source_test,
     plain_constructor_resolves_default_detected_dependencies) {
  struct config {};
  struct service {
    explicit service(config &init_dependency) : dependency(&init_dependency) {}
    config *dependency;
  };

  using source = dingo::bindings<dingo::bind<scope<shared>, storage<config>>,
                                 dingo::bind<scope<unique>, storage<service>>>;
  using registry_type = typename source::type;
  using dependencies = typename constructor_detection<service>::arguments;
  using expected_dependency_bindings =
      std::conditional_t<std::is_void_v<dependencies>, void,
                         type_list<typename registry_type::binding<
                             config, detail::no_lookup_key_t>>>;

  static_assert(registry_type::valid);
  static_assert(std::is_same_v<typename registry_type::dependencies<service>,
                               dependencies>);
  static_assert(
      std::is_same_v<typename registry_type::dependency_bindings<service>,
                     expected_dependency_bindings>);

  dingo::static_container<source> container;
  auto instance = container.resolve<service>();

  EXPECT_EQ(instance.dependency, &container.resolve<config &>());
}

TEST(static_bindings_source_test,
     annotated_dependencies_normalize_to_annotated_interfaces) {
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
  using registry_type = typename source::type;

  static_assert(registry_type::valid);
  static_assert(
      std::is_same_v<
          typename registry_type::dependencies<annotated<service, service_tag>>,
          type_list<annotated<config &, config_tag>>>);
  static_assert(std::is_same_v<
                typename registry_type::dependency_bindings<
                    annotated<service &, service_tag>>,
                type_list<typename registry_type::binding<
                    annotated<config, config_tag>, detail::no_lookup_key_t>>>);
}

TEST(static_bindings_source_test,
     supports_multibindings_and_keyed_multibindings) {
  struct first_key : std::integral_constant<int, 0> {};
  struct second_key : std::integral_constant<int, 1> {};
  struct service_interface {
    virtual ~service_interface() = default;
  };
  struct service_a : service_interface {};
  struct service_b : service_interface {};
  struct service_c : service_interface {};

  using first_binding =
      dingo::bind<scope<shared>, storage<service_a>,
                  interfaces<service_interface>, key_type<first_key>>;
  using second_binding =
      dingo::bind<scope<shared>, storage<service_b>,
                  interfaces<service_interface>, key_type<first_key>>;
  using third_binding =
      dingo::bind<scope<shared>, storage<service_c>,
                  interfaces<service_interface>, key_type<second_key>>;
  using source = dingo::bindings<first_binding, second_binding, third_binding>;
  using registry_type = typename source::type;

  static_assert(registry_type::valid);
  static_assert(type_list_size_v<typename registry_type::template bindings<
                    service_interface, detail::no_lookup_key_t>> == 0);
  static_assert(std::is_void_v<typename registry_type::template binding<
                    service_interface, detail::no_lookup_key_t>>);
  static_assert(
      type_list_size_v<typename registry_type::template bindings<
          service_interface, detail::lookup_key<key_type<first_key>>>> == 2);
  static_assert(
      type_list_size_v<typename registry_type::template bindings<
          service_interface, detail::lookup_key<key_type<second_key>>>> == 1);
  static_assert(
      std::is_void_v<typename registry_type::template binding<
          service_interface, detail::lookup_key<key_type<first_key>>>>);
  static_assert(
      !std::is_void_v<typename registry_type::template binding<
          service_interface, detail::lookup_key<key_type<second_key>>>>);
}

TEST(static_bindings_source_test,
     keyed_dependencies_resolve_against_matching_keyed_bindings) {
  struct first_key : std::integral_constant<int, 0> {};
  struct second_key : std::integral_constant<int, 1> {};
  struct config {};
  struct service {
    explicit service(dependency<config &, key_type<first_key>>) {}
  };

  using first_config_binding =
      dingo::bind<scope<shared>, storage<config>, key_type<first_key>>;
  using second_config_binding =
      dingo::bind<scope<shared>, storage<config>, key_type<second_key>>;
  using service_binding =
      dingo::bind<scope<unique>, storage<service>,
                  dependencies<dependency<config &, key_type<first_key>>>>;
  using source = dingo::bindings<first_config_binding, second_config_binding,
                                 service_binding>;
  using registry_type = typename source::type;

  static_assert(registry_type::valid);
  static_assert(
      std::is_same_v<typename registry_type::dependencies<service>,
                     type_list<dependency<config &, key_type<first_key>>>>);
  static_assert(
      std::is_same_v<typename registry_type::dependency_bindings<service>,
                     type_list<typename registry_type::template binding<
                         dependency<config, key_type<first_key>>,
                         detail::no_lookup_key_t>>>);
  static_assert(
      std::is_same_v<
          typename registry_type::template binding<
              dependency<config, key_type<first_key>>, detail::no_lookup_key_t>,
          typename registry_type::template binding<
              config, detail::lookup_key<key_type<first_key>>>>);
}

TEST(static_bindings_source_test,
     static_container_resolves_shared_cyclical_bindings) {
  struct b;
  struct a {
    explicit a(b &init_dependency) : dependency(&init_dependency) {}
    b *dependency;
  };
  struct b {
    explicit b(a &init_dependency) : dependency(&init_dependency) {}
    a *dependency;
  };

  using source = dingo::bindings<
      dingo::bind<scope<shared_cyclical>, storage<a>, dependencies<b &>>,
      dingo::bind<scope<shared_cyclical>, storage<b>, dependencies<a &>>>;

  dingo::static_container<source> container;
  auto &instance = container.resolve<a &>();

  EXPECT_EQ(instance.dependency->dependency, &instance);
  EXPECT_EQ(&container.resolve<a &>(), &instance);
}

TEST(static_bindings_source_test,
     container_resolves_static_shared_cyclical_interface_handles) {
  struct b_interface {
    virtual ~b_interface() = default;
  };
  struct b;
  struct a {
    explicit a(b_interface &init_dependency) : dependency(&init_dependency) {}
    b_interface *dependency;
  };
  struct b : b_interface {
    explicit b(a &init_dependency) : dependency(&init_dependency) {}
    a *dependency;
  };

  using source = dingo::bindings<
      dingo::bind<scope<shared_cyclical>, storage<a>,
                  dependencies<b_interface &>>,
      dingo::bind<scope<shared_cyclical>, storage<std::shared_ptr<b>>,
                  interfaces<b_interface>, dependencies<a &>>>;

  dingo::container<source> container;
  auto &instance = container.resolve<a &>();
  auto &b_view = container.resolve<b_interface &>();
  auto &b_instance = static_cast<b &>(b_view);

  EXPECT_EQ(instance.dependency, &b_view);
  EXPECT_EQ(b_instance.dependency, &instance);
  EXPECT_EQ(&container.resolve<a &>(), &instance);
  EXPECT_EQ(&container.resolve<b_interface &>(), &b_view);
}
