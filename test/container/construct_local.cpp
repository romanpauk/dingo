//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#include "container/construct_common.h"

namespace dingo {

TYPED_TEST_SUITE(construct_local_test, container_types, );

TYPED_TEST(construct_local_test,
           keyed_dependencies_use_runtime_keyed_registrations) {
    using container_type = TypeParam;

    struct first_key : std::integral_constant<int, 0> {};
    struct second_key : std::integral_constant<int, 1> {};

    struct service {
        service(keyed<int&, first_key> first, keyed<int&, second_key> second)
            : sum(static_cast<int&>(first) + static_cast<int&>(second)) {}

        int sum;
    };

    container_type container;
    container
        .template register_type<scope<external>, storage<int>, key<first_key>>(
            2);
    container
        .template register_type<scope<external>, storage<int>, key<second_key>>(
            5);

    ASSERT_EQ(container.template resolve<int>(key<first_key>{}), 2);
    ASSERT_EQ(container.template resolve<int>(key<second_key>{}), 5);
    ASSERT_EQ(container.template construct<service>().sum, 7);
}

TYPED_TEST(construct_local_test,
           keyed_collection_dependencies_use_runtime_keyed_registrations) {
    using container_type = TypeParam;

    struct first_key : std::integral_constant<int, 0> {};

    struct service {
        explicit service(
            keyed<std::vector<std::shared_ptr<IClass>>, first_key> classes)
            : classes_(
                  static_cast<std::vector<std::shared_ptr<IClass>>>(classes)) {}

        std::vector<std::shared_ptr<IClass>> classes_;
    };

    container_type container;
    container.template register_type<scope<shared>,
                                     storage<std::shared_ptr<ClassTag<0>>>,
                                     interfaces<IClass>, key<first_key>>();
    container.template register_type<scope<shared>,
                                     storage<std::shared_ptr<ClassTag<1>>>,
                                     interfaces<IClass>, key<first_key>>();

    auto first =
        container.template resolve<std::vector<std::shared_ptr<IClass>>>(
            key<first_key>{});

    ASSERT_EQ(first.size(), 2);
    std::vector<size_t> first_tags{first[0]->GetTag(), first[1]->GetTag()};
    std::sort(first_tags.begin(), first_tags.end());
    ASSERT_EQ(first_tags, (std::vector<size_t>{0, 1}));
    ASSERT_THROW(
        (container.template resolve<std::shared_ptr<IClass>>(key<first_key>{})),
        type_ambiguous_exception);

    auto constructed = container.template construct<service>();
    ASSERT_EQ(constructed.classes_.size(), 2);
    std::vector<size_t> constructed_tags{constructed.classes_[0]->GetTag(),
                                         constructed.classes_[1]->GetTag()};
    std::sort(constructed_tags.begin(), constructed_tags.end());
    ASSERT_EQ(constructed_tags, (std::vector<size_t>{0, 1}));
}

TYPED_TEST(construct_local_test,
           local_bindings_resolve_local_and_runtime_dependencies) {
    using container_type = TypeParam;

    struct local_config {
        local_config() : value(7) {}
        int value;
    };

    struct local_helper {
        local_helper() : value(5) {}
        int value;
    };

    struct service {
        service(local_config& config, local_helper& helper, int runtime_value)
            : value(config.value + helper.value + runtime_value) {}

        int value;
    };

    container_type container;
    container.template register_type<scope<external>, storage<int>>(3);
    container.template register_type<
        scope<shared>, storage<service>,
        bindings<bind<scope<shared>, storage<local_config>>,
                 bind<scope<shared>, storage<local_helper>>>>();

    auto& resolved = container.template resolve<service&>();

    ASSERT_EQ(resolved.value, 15);
    ASSERT_EQ(container.template construct<service>().value, 15);
}

TYPED_TEST(construct_local_test, local_bindings_wrapper_remains_supported) {
    using container_type = TypeParam;

    struct local_config {
        local_config() : value(4) {}
        int value;
    };

    struct service {
        service(local_config& config, int runtime_value)
            : value(config.value + runtime_value) {}

        int value;
    };

    container_type container;
    container.template register_type<scope<external>, storage<int>>(3);
    container.template register_type<
        scope<shared>, storage<service>,
        bindings<bind<scope<shared>, storage<local_config>>>>();

    ASSERT_EQ(container.template construct<service>().value, 7);
}

TYPED_TEST(construct_local_test,
           local_bindings_override_conflicting_host_singular_dependencies) {
    using container_type = TypeParam;

    struct service {
        explicit service(int& resolved_value) : value(resolved_value) {}

        int value;
    };

    struct wrapper {
        explicit wrapper(service& resolved_service)
            : value(resolved_service.value) {}

        int value;
    };

    container_type container;
    container.template register_type<scope<external>, storage<int>>(5);
    container
        .template register_type<scope<shared>, storage<service>,
                                bindings<bind<scope<shared>, storage<int>,
                                              factory<constructor<int()>>>>>();

    ASSERT_EQ(container.template resolve<service&>().value, 0);
    ASSERT_EQ(container.template construct<service>().value, 0);
    ASSERT_EQ(container.template construct<std::unique_ptr<service>>()->value,
              0);
    ASSERT_EQ(container.template construct<std::shared_ptr<service>>()->value,
              0);
    ASSERT_EQ(container.template construct<std::optional<service>>()->value, 0);
    ASSERT_EQ(container.template construct<wrapper>().value, 0);
}

TYPED_TEST(
    construct_local_test,
    local_bindings_override_conflicting_keyed_host_singular_dependencies) {
    using container_type = TypeParam;

    struct first_key : std::integral_constant<int, 0> {};

    struct service {
        explicit service(keyed<int&, first_key> resolved_value)
            : value(static_cast<int&>(resolved_value)) {}

        int value;
    };

    container_type container;
    container
        .template register_type<scope<external>, storage<int>, key<first_key>>(
            5);
    container.template register_type<
        scope<shared>, storage<service>,
        bindings<bind<scope<shared>, storage<int>, key<first_key>,
                      factory<constructor<int()>>>>>();

    ASSERT_EQ(container.template resolve<service&>().value, 0);
    ASSERT_EQ(container.template construct<service>().value, 0);
}

TYPED_TEST(construct_local_test,
           local_bindings_merge_collections_with_host_bindings) {
    using container_type = TypeParam;

    struct service {
        explicit service(std::vector<std::shared_ptr<IClass>> classes)
            : classes_(std::move(classes)) {}

        std::vector<std::shared_ptr<IClass>> classes_;
    };

    container_type container;
    container.template register_type<scope<shared>,
                                     storage<std::shared_ptr<ClassTag<1>>>,
                                     interfaces<IClass>>();
    container.template register_type<
        scope<shared>, storage<service>,
        bindings<bind<scope<shared>, storage<std::shared_ptr<ClassTag<0>>>,
                      interfaces<IClass>>>>();

    auto& resolved = container.template resolve<service&>();

    ASSERT_EQ(resolved.classes_.size(), 2);
    std::vector<size_t> tags{resolved.classes_[0]->GetTag(),
                             resolved.classes_[1]->GetTag()};
    std::sort(tags.begin(), tags.end());
    ASSERT_EQ(tags, (std::vector<size_t>{0, 1}));

    auto constructed = container.template construct<service>();
    ASSERT_EQ(constructed.classes_.size(), 2);
    std::vector<size_t> constructed_tags{constructed.classes_[0]->GetTag(),
                                         constructed.classes_[1]->GetTag()};
    std::sort(constructed_tags.begin(), constructed_tags.end());
    ASSERT_EQ(constructed_tags, (std::vector<size_t>{0, 1}));
}

TYPED_TEST(construct_local_test,
           runtime_injector_facade_uses_container_registrations) {
    using container_type = TypeParam;

    struct service {
        explicit service(int resolved_value) : value(resolved_value) {}
        int value;
    };

    container_type container;
    container.template register_type<scope<external>, storage<int>>(11);

    auto injector = container.injector();

    ASSERT_EQ(injector.template resolve<int>(), 11);
    ASSERT_EQ(injector.template construct<service>().value, 11);
}

TEST(construct_local_test,
     runtime_registry_and_runtime_container_split_runtime_source_and_facade) {
    struct service {
        explicit service(int resolved_value) : value(resolved_value) {}
        int value;
    };

    runtime_registry<> registry;
    registry.template register_type<scope<external>, storage<int>>(13);

    auto registry_injector = registry.injector();
    ASSERT_EQ(registry_injector.template resolve<int>(), 13);
    ASSERT_EQ(registry_injector.template construct<service>().value, 13);

    runtime_container<> named_container;
    named_container.template register_type<scope<external>, storage<int>>(17);

    ASSERT_EQ(named_container.template resolve<int>(), 17);
    ASSERT_EQ(named_container.template construct<service>().value, 17);
}

} // namespace dingo
