//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#include <dingo/static/registry.h>
#include <dingo/registration/constructor.h>
#include <dingo/storage/shared.h>
#include <dingo/storage/unique.h>

#include <gtest/gtest.h>

using namespace dingo;

TEST(static_bindings_source_test, exposes_models_and_dependency_bindings) {
    struct config {};
    struct service_interface {
        virtual ~service_interface() = default;
    };
    struct service : service_interface {
        explicit service(config&) {}
    };

    using config_binding = dingo::bind<scope<shared>, storage<config>>;
    using service_binding =
        dingo::bind<scope<unique>, storage<service>, interfaces<service_interface>,
                    dependencies<config&>>;
    using source = dingo::bindings<config_binding, service_binding>;
    using registry_type = typename source::type;

    static_assert(registry_type::valid);
    static_assert(std::is_same_v<typename registry_type::registration_types,
                                 type_list<config_binding, service_binding>>);
    static_assert(std::is_same_v<typename registry_type::model<config>::registration_type,
                                 config_binding>);
    static_assert(
        std::is_same_v<typename registry_type::model<service_interface>::registration_type,
                       service_binding>);
    static_assert(std::is_same_v<typename registry_type::dependencies<service_interface>,
                                 type_list<config&>>);
    static_assert(
        std::is_same_v<typename registry_type::dependency_bindings<service_interface>,
                       type_list<typename registry_type::binding<config>>>);
}

TEST(static_bindings_source_test, zero_argument_detected_constructor_is_known) {
    struct config {};

    using source = dingo::bindings<dingo::bind<scope<unique>, storage<config>>>;
    using registry_type = typename source::type;

    static_assert(registry_type::valid);
    static_assert(std::is_same_v<typename registry_type::dependencies<config>,
                                 type_list<>>);
    static_assert(std::is_same_v<typename registry_type::dependency_bindings<config>,
                                 type_list<>>);
}

TEST(static_bindings_source_test, constructor_typedef_bindings_reuse_detected_dependencies) {
    struct config {};
    struct service {
        DINGO_CONSTRUCTOR(service(config&)) {}
    };

    using config_binding = dingo::bind<scope<shared>, storage<config>>;
    using service_binding = dingo::bind<scope<unique>, storage<service>>;
    using source = dingo::bindings<config_binding, service_binding>;
    using registry_type = typename source::type;

    static_assert(registry_type::valid);
    static_assert(std::is_same_v<typename registry_type::dependencies<service>,
                                 type_list<config&>>);
    static_assert(
        std::is_same_v<typename registry_type::dependency_bindings<service>,
                       type_list<typename registry_type::binding<config>>>);
}

TEST(static_bindings_source_test,
     plain_constructor_dependencies_are_inferred_from_interface_bindings) {
    struct config {};
    struct logger {};
    struct service {
        service(config&, logger&) {}
    };

    using source = dingo::bindings<dingo::bind<scope<shared>, storage<config>>,
                                   dingo::bind<scope<shared>, storage<logger>>,
                                   dingo::bind<scope<unique>, storage<service>>>;
    using registry_type = typename source::type;

    static_assert(registry_type::valid);
    static_assert(std::is_same_v<typename registry_type::dependencies<service>, void>);
    static_assert(
        std::is_same_v<typename registry_type::dependency_bindings<service>,
                       type_list<typename registry_type::binding<config>,
                                 typename registry_type::binding<logger>>>);
}

TEST(static_bindings_source_test, annotated_dependencies_normalize_to_annotated_interfaces) {
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
    using registry_type = typename source::type;

    static_assert(registry_type::valid);
    static_assert(
        std::is_same_v<typename registry_type::dependencies<annotated<service, service_tag>>,
                       type_list<annotated<config&, config_tag>>>);
    static_assert(
        std::is_same_v<typename registry_type::dependency_bindings<
                           annotated<service&, service_tag>>,
                       type_list<typename registry_type::binding<
                           annotated<config, config_tag>>>>);
}

TEST(static_bindings_source_test, supports_multibindings_and_keyed_multibindings) {
    struct first_key : std::integral_constant<int, 0> {};
    struct second_key : std::integral_constant<int, 1> {};
    struct service_interface {
        virtual ~service_interface() = default;
    };
    struct service_a : service_interface {};
    struct service_b : service_interface {};
    struct service_c : service_interface {};

    using first_binding =
        dingo::bind<scope<shared>, storage<service_a>, interfaces<service_interface>,
                    key<first_key>>;
    using second_binding =
        dingo::bind<scope<shared>, storage<service_b>, interfaces<service_interface>,
                    key<first_key>>;
    using third_binding =
        dingo::bind<scope<shared>, storage<service_c>, interfaces<service_interface>,
                    key<second_key>>;
    using source = dingo::bindings<first_binding, second_binding, third_binding>;
    using registry_type = typename source::type;

    static_assert(registry_type::valid);
    static_assert(type_list_size_v<
                      typename registry_type::template bindings<service_interface>> == 3);
    static_assert(std::is_void_v<
                  typename registry_type::template binding<service_interface>>);
    static_assert(type_list_size_v<
                      typename registry_type::template bindings<service_interface,
                                                               first_key>> == 2);
    static_assert(type_list_size_v<typename registry_type::template bindings<
                      service_interface, second_key>> == 1);
    static_assert(std::is_void_v<
                  typename registry_type::template binding<service_interface, first_key>>);
    static_assert(!std::is_void_v<
                  typename registry_type::template binding<service_interface, second_key>>);
}

TEST(static_bindings_source_test, keyed_dependencies_resolve_against_matching_keyed_bindings) {
    struct first_key : std::integral_constant<int, 0> {};
    struct second_key : std::integral_constant<int, 1> {};
    struct config {};
    struct service {
        explicit service(keyed<config&, first_key>) {}
    };

    using first_config_binding =
        dingo::bind<scope<shared>, storage<config>, key<first_key>>;
    using second_config_binding =
        dingo::bind<scope<shared>, storage<config>, key<second_key>>;
    using service_binding =
        dingo::bind<scope<unique>, storage<service>,
                    dependencies<keyed<config&, first_key>>>;
    using source = dingo::bindings<first_config_binding, second_config_binding,
                                   service_binding>;
    using registry_type = typename source::type;

    static_assert(registry_type::valid);
    static_assert(std::is_same_v<typename registry_type::dependencies<service>,
                                 type_list<keyed<config&, first_key>>>);
    static_assert(
        std::is_same_v<typename registry_type::dependency_bindings<service>,
                       type_list<typename registry_type::template binding<
                           keyed<config, first_key>>>>);
    static_assert(std::is_same_v<
                  typename registry_type::template binding<keyed<config, first_key>>,
                  typename registry_type::template binding<config, first_key>>);
}
