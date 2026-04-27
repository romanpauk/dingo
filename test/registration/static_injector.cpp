//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#include <dingo/container.h>
#include <dingo/static/injector.h>
#include <dingo/static_container.h>
#include <dingo/storage/external.h>
#include <dingo/storage/shared.h>
#include <dingo/storage/unique.h>
#include <dingo/type/type_name.h>

#include <gtest/gtest.h>

#include <map>
#include <memory>
#include <type_traits>
#include <typeindex>
#include <utility>
#include <vector>

#include "support/assert.h"
#include "support/class.h"

using namespace dingo;

namespace {

struct injector_config {
    int value;
};

injector_config make_config() { return {7}; }

injector_config make_other_config() { return {11}; }

struct injector_service_interface {
    virtual ~injector_service_interface() = default;
    virtual int read() const = 0;
};

struct injector_service : injector_service_interface {
    explicit injector_service(injector_config& config) : config_(&config) {}

    int read() const override { return config_->value; }

    injector_config* config_;
};

struct constructed_consumer {
    explicit constructed_consumer(injector_config& config)
        : value(config.value) {}

    int value;
};

struct shared_counter {
    int value = 0;
};

shared_counter make_counter() { return {5}; }

struct inferred_service {
    inferred_service(injector_config& config, shared_counter& counter)
        : value(config.value + counter.value) {}

    int value;
};

struct first_key : std::integral_constant<int, 0> {};
struct second_key : std::integral_constant<int, 1> {};

struct keyed_consumer {
    explicit keyed_consumer(keyed<injector_config&, first_key> config)
        : value(static_cast<injector_config&>(config).value) {}

    int value;
};

struct keyed_service {
    explicit keyed_service(keyed<injector_config&, first_key> config)
        : value(static_cast<injector_config&>(config).value) {}

    int value;
};

struct keyed_collection_consumer {
    explicit keyed_collection_consumer(
        keyed<std::vector<std::shared_ptr<IClass>>, first_key> classes)
        : classes_(static_cast<std::vector<std::shared_ptr<IClass>>>(classes)) {
    }

    std::vector<std::shared_ptr<IClass>> classes_;
};

std::shared_ptr<Class> make_shared_class() { return std::make_shared<Class>(); }

} // namespace

TEST(static_injector_test, resolves_shared_interfaces_and_function_factories) {
    using source =
        dingo::bindings<dingo::bind<scope<shared>, storage<injector_config>,
                                    factory<function<make_config>>>,
                        dingo::bind<scope<shared>, storage<injector_service>,
                                    interfaces<injector_service_interface>,
                                    dependencies<injector_config&>>>;

    static_injector<source> injector;

    auto& config = injector.resolve<injector_config&>();
    auto& service = injector.resolve<injector_service_interface&>();

    ASSERT_EQ(config.value, 7);
    ASSERT_EQ(service.read(), 7);
    ASSERT_EQ(&config, &injector.resolve<injector_config&>());
    ASSERT_EQ(&service, &injector.resolve<injector_service_interface&>());
}

TEST(static_injector_test, shared_bindings_preserve_identity_across_resolves) {
    using source =
        dingo::bindings<dingo::bind<scope<shared>, storage<shared_counter>,
                                    factory<constructor<shared_counter()>>>>;

    static_injector<source> injector;

    auto& first = injector.resolve<shared_counter&>();
    auto& second = injector.resolve<shared_counter&>();
    first.value = 11;

    ASSERT_EQ(&first, &second);
    ASSERT_EQ(second.value, 11);
}

TEST(static_injector_test, construct_and_invoke_use_binding_dependencies) {
    using source =
        dingo::bindings<dingo::bind<scope<shared>, storage<injector_config>,
                                    factory<function<make_config>>>>;

    static_injector<source> injector;

    auto value = injector.construct<constructed_consumer>();

    ASSERT_EQ(value.value, 7);
    ASSERT_EQ(injector.invoke(
                  [](injector_config& config) { return config.value * 2; }),
              14);
}

TEST(static_injector_test, resolves_unique_rvalue_requests) {
    using source =
        dingo::bindings<dingo::bind<scope<unique>, storage<injector_config>,
                                    factory<function<make_config>>>>;

    static_injector<source> injector;

    auto moved = injector.resolve<injector_config&&>();

    ASSERT_EQ(moved.value, 7);
}

TEST(static_injector_test,
     hybrid_container_resolves_unique_rvalue_requests_from_static_bindings) {
    using source =
        dingo::bindings<dingo::bind<scope<unique>, storage<injector_config>,
                                    factory<function<make_config>>>>;

    container<source> hybrid;

    auto moved = hybrid.resolve<injector_config&&>();

    ASSERT_EQ(moved.value, 7);
}

TEST(static_injector_test, constructs_unique_rvalue_requests) {
    using source =
        dingo::bindings<dingo::bind<scope<unique>, storage<injector_config>,
                                    factory<function<make_config>>>>;

    static_injector<source> injector;

    auto moved = injector.construct<injector_config&&>();

    ASSERT_EQ(moved.value, 7);
}

TEST(static_injector_test,
     hybrid_container_constructs_unique_rvalue_requests_from_static_bindings) {
    using source =
        dingo::bindings<dingo::bind<scope<unique>, storage<injector_config>,
                                    factory<function<make_config>>>>;

    container<source> hybrid;

    auto moved = hybrid.construct<injector_config&&>();

    ASSERT_EQ(moved.value, 7);
}

TEST(static_injector_test, resolves_unique_wrapper_rvalue_requests) {
    using source = dingo::bindings<dingo::bind<
        scope<unique>, storage<std::unique_ptr<Class>>, interfaces<IClass>>>;

    static_injector<source> injector;

    auto unique = injector.resolve<std::unique_ptr<IClass>&&>();

    AssertClass(*unique);
}

TEST(static_injector_test, constructs_unique_wrapper_rvalue_requests) {
    using source = dingo::bindings<dingo::bind<
        scope<unique>, storage<std::unique_ptr<Class>>, interfaces<IClass>>>;

    static_injector<source> injector;

    auto unique = injector.construct<std::unique_ptr<IClass>&&>();

    AssertClass(*unique);
}

namespace {
using shared_config_source =
    dingo::bindings<dingo::bind<scope<shared>, storage<injector_config>,
                                factory<function<make_config>>>>;
using shared_interface_wrapper_source = dingo::bindings<
    dingo::bind<scope<shared>, storage<std::shared_ptr<Class>>,
                interfaces<IClass>, factory<function<make_shared_class>>>>;
using non_matching_static_source =
    dingo::bindings<dingo::bind<scope<shared>, storage<shared_counter>,
                                factory<function<make_counter>>>>;
using shared_config_static_source =
    static_bindings_source_t<shared_config_source>;
using shared_config_selection =
    detail::static_binding_t<typename shared_config_static_source::
                                 template bindings<injector_config, void>>;
using shared_config_binding = typename shared_config_selection::binding_type;
using shared_interface_wrapper_static_source =
    static_bindings_source_t<shared_interface_wrapper_source>;
using shared_interface_wrapper_selection = detail::static_binding_t<
    typename shared_interface_wrapper_static_source::template bindings<
        std::shared_ptr<IClass>, void>>;
using shared_interface_wrapper_binding =
    typename shared_interface_wrapper_selection::binding_type;
} // namespace

static_assert(!detail::binding_supports_request_v<injector_config&&,
                                                  shared_config_binding>);
static_assert(!detail::binding_supports_request_v<
              std::shared_ptr<injector_config>&&, shared_config_binding>);
static_assert(!detail::binding_supports_request_v<
              std::shared_ptr<IClass>&&, shared_interface_wrapper_binding>);

TEST(
    static_injector_test,
    static_injector_rejects_shared_wrapper_rvalue_construction_from_static_bindings) {
    static_injector<shared_config_source> injector;

    ASSERT_THROW((injector.construct<std::shared_ptr<injector_config>&&>()),
                 type_not_convertible_exception);
}

TEST(static_injector_test,
     static_injector_rejects_shared_rvalue_construction_from_static_bindings) {
    static_injector<shared_config_source> injector;

    ASSERT_THROW((injector.construct<injector_config&&>()),
                 type_not_convertible_exception);
}

TEST(static_injector_test,
     runtime_container_rejects_shared_wrapper_rvalue_resolution) {
    container<> runtime;
    runtime.template register_type<scope<shared>, storage<injector_config>,
                                   factory<function<make_config>>>();

    ASSERT_THROW((runtime.resolve<std::shared_ptr<injector_config>&&>()),
                 type_not_convertible_exception);
}

TEST(static_injector_test, runtime_container_rejects_shared_rvalue_resolution) {
    container<> runtime;
    runtime.template register_type<scope<shared>, storage<injector_config>,
                                   factory<function<make_config>>>();

    ASSERT_THROW((runtime.resolve<injector_config&&>()),
                 type_not_convertible_exception);
}

TEST(static_injector_test,
     runtime_container_rejects_shared_interface_wrapper_rvalue_resolution) {
    container<> runtime;
    runtime.template register_type<
        scope<shared>, storage<std::shared_ptr<Class>>, interfaces<IClass>,
        factory<function<make_shared_class>>>();

    ASSERT_THROW((runtime.resolve<std::shared_ptr<IClass>&&>()),
                 type_not_convertible_exception);
}

TEST(
    static_injector_test,
    hybrid_container_resolves_unique_wrapper_rvalue_requests_from_static_bindings) {
    using source = dingo::bindings<dingo::bind<
        scope<unique>, storage<std::unique_ptr<Class>>, interfaces<IClass>>>;

    container<source> hybrid;

    auto unique = hybrid.resolve<std::unique_ptr<IClass>&&>();

    AssertClass(*unique);
}

TEST(
    static_injector_test,
    hybrid_container_constructs_unique_wrapper_rvalue_requests_from_static_bindings) {
    using source = dingo::bindings<dingo::bind<
        scope<unique>, storage<std::unique_ptr<Class>>, interfaces<IClass>>>;

    container<source> hybrid;

    auto unique = hybrid.construct<std::unique_ptr<IClass>&&>();

    AssertClass(*unique);
}

TEST(
    static_injector_test,
    hybrid_container_rejects_shared_wrapper_rvalue_construction_from_static_bindings) {
    container<shared_config_source> hybrid;

    ASSERT_THROW((hybrid.construct<std::shared_ptr<injector_config>&&>()),
                 type_not_convertible_exception);
}

TEST(static_injector_test,
     hybrid_container_rejects_shared_rvalue_construction_from_static_bindings) {
    container<shared_config_source> hybrid;

    ASSERT_THROW((hybrid.construct<injector_config&&>()),
                 type_not_convertible_exception);
}

TEST(static_injector_test,
     hybrid_container_rejects_runtime_shared_wrapper_rvalue_resolution) {
    container<non_matching_static_source> hybrid;
    hybrid.template register_type<scope<shared>, storage<injector_config>,
                                  factory<function<make_config>>>();

    ASSERT_THROW((hybrid.resolve<std::shared_ptr<injector_config>&&>()),
                 type_not_convertible_exception);
}

TEST(static_injector_test,
     hybrid_container_rejects_runtime_shared_rvalue_resolution) {
    container<non_matching_static_source> hybrid;
    hybrid.template register_type<scope<shared>, storage<injector_config>,
                                  factory<function<make_config>>>();

    ASSERT_THROW((hybrid.resolve<injector_config&&>()),
                 type_not_convertible_exception);
}

TEST(static_injector_test,
     runtime_container_rejects_shared_wrapper_rvalue_construction) {
    container<> runtime;
    runtime.template register_type<scope<shared>, storage<injector_config>,
                                   factory<function<make_config>>>();

    ASSERT_THROW((runtime.construct<std::shared_ptr<injector_config>&&>()),
                 type_not_convertible_exception);
}

TEST(static_injector_test,
     runtime_container_rejects_shared_rvalue_construction) {
    container<> runtime;
    runtime.template register_type<scope<shared>, storage<injector_config>,
                                   factory<function<make_config>>>();

    ASSERT_THROW((runtime.construct<injector_config&&>()),
                 type_not_convertible_exception);
}

TEST(static_injector_test,
     runtime_container_rejects_shared_interface_wrapper_rvalue_construction) {
    container<> runtime;
    runtime.template register_type<
        scope<shared>, storage<std::shared_ptr<Class>>, interfaces<IClass>,
        factory<function<make_shared_class>>>();

    ASSERT_THROW((runtime.construct<std::shared_ptr<IClass>&&>()),
                 type_not_convertible_exception);
}

TEST(static_injector_test,
     runtime_container_rejects_external_wrapper_rvalue_construction) {
    container<> runtime;
    runtime.template register_type<scope<external>,
                                   storage<std::shared_ptr<injector_config>>>(
        std::make_shared<injector_config>(make_config()));

    ASSERT_THROW((runtime.construct<std::shared_ptr<injector_config>&&>()),
                 type_not_convertible_exception);
}

TEST(static_injector_test,
     hybrid_container_rejects_external_wrapper_rvalue_construction) {
    container<shared_config_source> hybrid;
    hybrid.template register_type<scope<external>,
                                  storage<std::shared_ptr<injector_config>>>(
        std::make_shared<injector_config>(make_config()));

    ASSERT_THROW((hybrid.construct<std::shared_ptr<injector_config>&&>()),
                 type_not_convertible_exception);
}

TEST(static_injector_test,
     runtime_container_rejects_external_rvalue_construction) {
    container<> runtime;
    auto config = make_config();
    runtime.template register_type<scope<external>, storage<injector_config>>(
        config);

    ASSERT_THROW((runtime.construct<injector_config&&>()),
                 type_not_convertible_exception);
}

TEST(static_injector_test,
     hybrid_container_rejects_external_rvalue_construction) {
    container<shared_config_source> hybrid;
    auto config = make_config();
    hybrid.template register_type<scope<external>, storage<injector_config>>(
        config);

    ASSERT_THROW((hybrid.construct<injector_config&&>()),
                 type_not_convertible_exception);
}

TEST(static_injector_test,
     runtime_container_rejects_external_wrapper_rvalue_resolution) {
    container<> runtime;
    runtime.template register_type<scope<external>,
                                   storage<std::shared_ptr<injector_config>>>(
        std::make_shared<injector_config>(make_config()));

    ASSERT_THROW((runtime.resolve<std::shared_ptr<injector_config>&&>()),
                 type_not_convertible_exception);
}

TEST(static_injector_test,
     hybrid_container_rejects_runtime_external_wrapper_rvalue_resolution) {
    container<non_matching_static_source> hybrid;
    hybrid.template register_type<scope<external>,
                                  storage<std::shared_ptr<injector_config>>>(
        std::make_shared<injector_config>(make_config()));

    ASSERT_THROW((hybrid.resolve<std::shared_ptr<injector_config>&&>()),
                 type_not_convertible_exception);
}

TEST(static_injector_test,
     runtime_container_rejects_external_rvalue_resolution) {
    container<> runtime;
    auto config = make_config();
    runtime.template register_type<scope<external>, storage<injector_config>>(
        config);

    ASSERT_THROW((runtime.resolve<injector_config&&>()),
                 type_not_convertible_exception);
}

TEST(static_injector_test,
     hybrid_container_rejects_runtime_external_rvalue_resolution) {
    container<non_matching_static_source> hybrid;
    auto config = make_config();
    hybrid.template register_type<scope<external>, storage<injector_config>>(
        config);

    ASSERT_THROW((hybrid.resolve<injector_config&&>()),
                 type_not_convertible_exception);
}

TEST(static_injector_test, wrapper_construction_reuses_registered_bindings) {
    using source =
        dingo::bindings<dingo::bind<scope<shared>, storage<injector_config>,
                                    factory<function<make_config>>>>;

    static_injector<source> injector;

    auto unique = injector.construct<std::unique_ptr<injector_config>>();
    auto constructed = injector.construct<std::shared_ptr<injector_config>>();
    auto optional = injector.construct<std::optional<injector_config>>();

    ASSERT_NE(unique, nullptr);
    ASSERT_EQ(unique->value, 7);
    ASSERT_EQ(constructed->value, 7);
    ASSERT_TRUE(optional.has_value());
    ASSERT_EQ(optional->value, 7);
}

TEST(static_injector_test,
     resolves_registered_services_with_inferred_binding_dependencies) {
    using source =
        dingo::bindings<dingo::bind<scope<shared>, storage<injector_config>,
                                    factory<function<make_config>>>,
                        dingo::bind<scope<shared>, storage<shared_counter>,
                                    factory<function<make_counter>>>,
                        dingo::bind<scope<shared>, storage<inferred_service>>>;

    static_injector<source> injector;

    auto& service = injector.resolve<inferred_service&>();

    ASSERT_EQ(service.value, 12);
    ASSERT_EQ(&service, &injector.resolve<inferred_service&>());
}

TEST(static_injector_test, static_container_wraps_compile_time_sources) {
    using source =
        dingo::bindings<dingo::bind<scope<shared>, storage<injector_config>,
                                    factory<function<make_config>>>>;

    static_container<source> binding_container;
    container<source> source_container;
    container<
        dingo::bindings<dingo::bind<scope<shared>, storage<injector_config>,
                                    factory<function<make_config>>>>>
        equivalent_container;

    ASSERT_EQ(binding_container.resolve<injector_config&>().value, 7);
    ASSERT_EQ(binding_container.construct<constructed_consumer>().value, 7);
    ASSERT_EQ(binding_container.injector().resolve<injector_config&>().value,
              7);
    ASSERT_EQ(source_container.resolve<injector_config&>().value, 7);
    ASSERT_EQ(source_container.construct<constructed_consumer>().value, 7);
    ASSERT_EQ(equivalent_container.resolve<injector_config&>().value, 7);
    ASSERT_EQ(equivalent_container.construct<constructed_consumer>().value, 7);
}

TEST(static_injector_test,
     hybrid_container_runtime_and_static_singular_conflicts_are_ambiguous) {
    using source =
        dingo::bindings<dingo::bind<scope<shared>, storage<injector_config>,
                                    factory<function<make_config>>>>;

    container<source> hybrid;
    hybrid.template register_type<scope<shared>, storage<injector_config>,
                                  factory<function<make_other_config>>>();

    ASSERT_THROW((hybrid.resolve<injector_config&>()),
                 type_ambiguous_exception);
    ASSERT_THROW((hybrid.construct<constructed_consumer>()),
                 type_ambiguous_exception);
}

TEST(static_injector_test,
     hybrid_container_merges_runtime_and_static_collection_bindings) {
    using source = dingo::bindings<
        dingo::bind<scope<shared>, storage<std::shared_ptr<ClassTag<0>>>,
                    interfaces<IClass>>>;

    container<source> hybrid;
    hybrid.template register_type<scope<shared>,
                                  storage<std::shared_ptr<ClassTag<1>>>,
                                  interfaces<IClass>>();

    auto classes =
        hybrid.construct_collection<std::vector<std::shared_ptr<IClass>>>();

    ASSERT_EQ(classes.size(), 2);
    std::vector<size_t> tags{classes[0]->GetTag(), classes[1]->GetTag()};
    std::sort(tags.begin(), tags.end());
    ASSERT_EQ(tags, (std::vector<size_t>{0, 1}));
}

TEST(static_injector_test,
     hybrid_container_runtime_bindings_can_depend_on_static_bindings) {
    using source =
        dingo::bindings<dingo::bind<scope<shared>, storage<injector_config>,
                                    factory<function<make_config>>>>;

    container<source> hybrid;
    hybrid.template register_type<scope<shared>, storage<injector_service>,
                                  interfaces<injector_service_interface>>();

    auto& service = hybrid.resolve<injector_service_interface&>();

    ASSERT_EQ(service.read(), 7);
}

TEST(static_injector_test,
     hybrid_container_static_bindings_can_depend_on_runtime_bindings) {
    using source =
        dingo::bindings<dingo::bind<scope<shared>, storage<injector_service>,
                                    interfaces<injector_service_interface>,
                                    dependencies<injector_config&>>>;

    container<source> hybrid;
    hybrid.template register_type<scope<shared>, storage<injector_config>,
                                  factory<function<make_other_config>>>();

    auto& service = hybrid.resolve<injector_service_interface&>();

    ASSERT_EQ(service.read(), 11);
}

TEST(static_injector_test,
     hybrid_container_runtime_bindings_can_use_static_host_bindings) {
    struct helper {
        helper() : value(5) {}

        int value;
    };

    struct service {
        service(injector_config& config, helper& helper)
            : value(config.value + helper.value) {}

        int value;
    };

    using static_bindings =
        dingo::bindings<dingo::bind<scope<shared>, storage<injector_config>,
                                    factory<function<make_config>>>>;

    container<static_bindings> hybrid;
    hybrid.template register_type<
        scope<shared>, storage<service>,
        dingo::bindings<dingo::bind<scope<shared>, storage<helper>>>>();

    auto& resolved = hybrid.resolve<service&>();

    ASSERT_EQ(resolved.value, 12);
}

TEST(
    static_injector_test,
    hybrid_container_runtime_bindings_override_conflicting_static_host_bindings) {
    struct helper {
        helper() : value(5) {}

        int value;
    };

    struct service {
        service(injector_config& config, helper& helper)
            : value(config.value + helper.value) {}

        int value;
    };

    using source =
        dingo::bindings<dingo::bind<scope<shared>, storage<injector_config>,
                                    factory<function<make_config>>>>;

    container<source> hybrid;
    hybrid.template register_type<
        scope<shared>, storage<service>,
        dingo::bindings<dingo::bind<scope<shared>, storage<injector_config>,
                                    factory<function<make_other_config>>>,
                        dingo::bind<scope<shared>, storage<helper>>>>();

    ASSERT_EQ(hybrid.resolve<service&>().value, 16);
    ASSERT_EQ(hybrid.construct<service>().value, 16);
    ASSERT_EQ(hybrid.construct<std::unique_ptr<service>>()->value, 16);
    ASSERT_EQ(hybrid.construct<std::shared_ptr<service>>()->value, 16);
    ASSERT_EQ(hybrid.invoke([](service resolved_service) {
        return resolved_service.value;
    }),
              16);
}

TEST(
    static_injector_test,
    hybrid_container_runtime_bindings_override_ambiguous_runtime_and_static_host_bindings) {
    struct helper {
        helper() : value(5) {}

        int value;
    };

    struct service {
        service(injector_config& config, helper& helper)
            : value(config.value + helper.value) {}

        int value;
    };

    using source =
        dingo::bindings<dingo::bind<scope<shared>, storage<injector_config>,
                                    factory<function<make_config>>>>;

    container<source> hybrid;
    hybrid.template register_type<scope<shared>, storage<injector_config>,
                                  factory<function<make_other_config>>>();
    hybrid.template register_type<
        scope<shared>, storage<service>,
        dingo::bindings<dingo::bind<scope<shared>, storage<injector_config>,
                                    factory<constructor<injector_config()>>>,
                        dingo::bind<scope<shared>, storage<helper>>>>();

    ASSERT_EQ(hybrid.resolve<service&>().value, 5);
    ASSERT_EQ(hybrid.construct<service>().value, 5);
    ASSERT_EQ(hybrid.invoke([](service resolved_service) {
        return resolved_service.value;
    }),
              5);
}

TEST(static_injector_test, shared_handle_conversions_are_cached_per_binding) {
    using source = dingo::bindings<
        dingo::bind<scope<shared>, storage<std::shared_ptr<Class>>,
                    interfaces<IClass>, factory<function<make_shared_class>>>>;

    static_injector<source> injector;

    auto& first = injector.resolve<std::shared_ptr<IClass>&>();
    auto& second = injector.resolve<std::shared_ptr<IClass>&>();
    auto* pointer = injector.resolve<std::shared_ptr<IClass>*>();
    auto* leaf = injector.resolve<IClass*>();

    AssertClass(*first);
    ASSERT_EQ(first.get(), leaf);
    ASSERT_EQ(&first, &second);
    ASSERT_EQ(pointer, &first);
}

TEST(static_injector_test,
     construct_exact_interface_wrapper_bindings_preserves_binding_identity) {
    using source = dingo::bindings<
        dingo::bind<scope<shared>, storage<std::shared_ptr<Class>>,
                    interfaces<IClass>, factory<function<make_shared_class>>>>;

    static_injector<source> injector;

    auto& resolved = injector.resolve<std::shared_ptr<IClass>&>();
    auto constructed = injector.construct<std::shared_ptr<IClass>>();

    AssertClass(*constructed);
    ASSERT_EQ(constructed.get(), resolved.get());
    ASSERT_EQ(constructed, resolved);
}

TEST(static_injector_test, matches_runtime_container_resolution_behavior) {
    using source = dingo::bindings<
        dingo::bind<scope<shared>, storage<injector_config>,
                    factory<function<make_config>>>,
        dingo::bind<scope<shared>, storage<std::shared_ptr<Class>>,
                    interfaces<IClass>, factory<function<make_shared_class>>>>;

    static_injector<source> injector;
    container<> runtime;

    runtime.register_type<scope<shared>, storage<injector_config>,
                          factory<function<make_config>>>();
    runtime.register_type<scope<shared>, storage<std::shared_ptr<Class>>,
                          interfaces<IClass>,
                          factory<function<make_shared_class>>>();

    auto& static_handle = injector.resolve<std::shared_ptr<IClass>&>();
    auto& runtime_handle = runtime.resolve<std::shared_ptr<IClass>&>();

    AssertClass(*static_handle);
    AssertClass(*runtime_handle);
    ASSERT_EQ(static_handle.get(), injector.resolve<IClass*>());
    ASSERT_EQ(runtime_handle.get(), runtime.resolve<IClass*>());
    ASSERT_EQ(&static_handle, &injector.resolve<std::shared_ptr<IClass>&>());
    ASSERT_EQ(&runtime_handle, &runtime.resolve<std::shared_ptr<IClass>&>());

    auto static_consumer = injector.construct<constructed_consumer>();
    auto runtime_consumer = runtime.construct<constructed_consumer>();

    ASSERT_EQ(static_consumer.value, 7);
    ASSERT_EQ(runtime_consumer.value, 7);
    ASSERT_EQ(injector.invoke(
                  [](injector_config& config) { return config.value * 3; }),
              runtime.invoke(
                  [](injector_config& config) { return config.value * 3; }));
}

TEST(static_injector_test, resolves_indexed_multibindings_with_typed_keys) {
    struct first_key : std::integral_constant<int, 0> {};
    struct second_key : std::integral_constant<int, 1> {};

    using source = dingo::bindings<
        dingo::bind<scope<shared>, storage<std::shared_ptr<ClassTag<0>>>,
                    interfaces<IClass>, key<first_key>>,
        dingo::bind<scope<shared>, storage<std::shared_ptr<ClassTag<1>>>,
                    interfaces<IClass>, key<second_key>>>;

    static_injector<source> injector;

    auto& first = injector.resolve<std::shared_ptr<IClass>&>(key<first_key>{});
    auto& second =
        injector.resolve<std::shared_ptr<IClass>&>(key<second_key>{});

    ASSERT_EQ(first->GetTag(), 0);
    ASSERT_EQ(second->GetTag(), 1);
    ASSERT_EQ(first.get(), injector.resolve<IClass*>(key<first_key>{}));
    ASSERT_EQ(second.get(), injector.resolve<IClass*>(key<second_key>{}));
}

TEST(static_injector_test, constructs_multibinding_collections) {
    using source = dingo::bindings<
        dingo::bind<scope<shared>, storage<std::shared_ptr<ClassTag<0>>>,
                    interfaces<IClass>>,
        dingo::bind<scope<shared>, storage<std::shared_ptr<ClassTag<1>>>,
                    interfaces<IClass>>>;

    static_injector<source> injector;

    auto classes = injector.resolve<std::vector<std::shared_ptr<IClass>>>();
    ASSERT_EQ(classes.size(), 2);
    ASSERT_EQ(classes[0]->GetTag(), 0);
    ASSERT_EQ(classes[1]->GetTag(), 1);

    auto mapped = injector.construct_collection<
        std::map<std::type_index, std::shared_ptr<IClass>>>(
        [](auto& collection, auto&& value) {
            collection.emplace(typeid(*value.get()), std::move(value));
        });
    ASSERT_EQ(mapped.size(), 2);
}

TEST(static_injector_test,
     resolves_keyed_collections_and_supports_keyed_collection_dependencies) {
    using source = dingo::bindings<
        dingo::bind<scope<shared>, storage<std::shared_ptr<ClassTag<0>>>,
                    interfaces<IClass>, key<first_key>>,
        dingo::bind<scope<shared>, storage<std::shared_ptr<ClassTag<1>>>,
                    interfaces<IClass>, key<first_key>>,
        dingo::bind<scope<shared>, storage<std::shared_ptr<ClassTag<2>>>,
                    interfaces<IClass>, key<second_key>>>;

    static_injector<source> injector;

    auto first = injector.resolve<std::vector<std::shared_ptr<IClass>>>(
        key<first_key>{});
    auto second = injector.resolve<std::vector<std::shared_ptr<IClass>>>(
        key<second_key>{});

    ASSERT_EQ(first.size(), 2);
    ASSERT_EQ(second.size(), 1);
    ASSERT_EQ(first[0]->GetTag(), 0);
    ASSERT_EQ(first[1]->GetTag(), 1);
    ASSERT_EQ(second.front()->GetTag(), 2);

    auto consumer = injector.construct<keyed_collection_consumer>();
    ASSERT_EQ(consumer.classes_.size(), 2);
    ASSERT_EQ(consumer.classes_[0]->GetTag(), 0);
    ASSERT_EQ(consumer.classes_[1]->GetTag(), 1);

    ASSERT_EQ(injector.invoke(
                  [](keyed<std::vector<std::shared_ptr<IClass>>, second_key>
                         classes) {
                      auto resolved =
                          static_cast<std::vector<std::shared_ptr<IClass>>>(
                              classes);
                      return resolved.front()->GetTag();
                  }),
              2);
}

TEST(static_injector_test, constructs_keyed_mapping_collections) {
    using source = dingo::bindings<
        dingo::bind<scope<shared>, storage<std::shared_ptr<ClassTag<0>>>,
                    interfaces<IClass>, key<first_key>>,
        dingo::bind<scope<shared>, storage<std::shared_ptr<ClassTag<1>>>,
                    interfaces<IClass>, key<first_key>>,
        dingo::bind<scope<shared>, storage<std::shared_ptr<ClassTag<2>>>,
                    interfaces<IClass>, key<second_key>>>;

    static_injector<source> injector;

    auto first = injector.construct_collection<
        std::map<std::type_index, std::shared_ptr<IClass>>>(
        [](auto& collection, auto&& value) {
            collection.emplace(typeid(*value.get()), std::move(value));
        },
        key<first_key>{});
    auto second = injector.construct_collection<
        std::map<std::type_index, std::shared_ptr<IClass>>>(
        [](auto& collection, auto&& value) {
            collection.emplace(typeid(*value.get()), std::move(value));
        },
        key<second_key>{});

    ASSERT_EQ(first.size(), 2);
    ASSERT_EQ(second.size(), 1);
    ASSERT_EQ(first.at(typeid(ClassTag<0>))->GetTag(), 0);
    ASSERT_EQ(first.at(typeid(ClassTag<1>))->GetTag(), 1);
    ASSERT_EQ(second.at(typeid(ClassTag<2>))->GetTag(), 2);
}

TEST(static_injector_test,
     construct_and_invoke_support_keyed_binding_dependencies) {
    using source = dingo::bindings<
        dingo::bind<scope<shared>, storage<injector_config>,
                    factory<function<make_config>>, key<first_key>>,
        dingo::bind<scope<shared>, storage<injector_config>,
                    factory<function<make_other_config>>, key<second_key>>>;

    static_injector<source> injector;

    auto consumer = injector.construct<keyed_consumer>();

    ASSERT_EQ(consumer.value, 7);
    ASSERT_EQ(injector.invoke([](keyed<injector_config&, second_key> config) {
        return static_cast<injector_config&>(config).value;
    }),
              11);
}

TEST(static_injector_test,
     resolves_registered_services_with_keyed_constructor_dependencies) {
    using source = dingo::bindings<
        dingo::bind<scope<shared>, storage<injector_config>,
                    factory<function<make_config>>, key<first_key>>,
        dingo::bind<scope<shared>, storage<injector_config>,
                    factory<function<make_other_config>>, key<second_key>>,
        dingo::bind<scope<shared>, storage<keyed_service>,
                    dependencies<keyed<injector_config&, first_key>>>>;

    static_injector<source> injector;

    auto& service = injector.resolve<keyed_service&>();

    ASSERT_EQ(service.value, 7);
}
