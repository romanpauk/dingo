//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#include <dingo/container.h>
#include <dingo/index/array.h>
#include <dingo/storage/shared.h>
#include <dingo/storage/shared_cyclical.h>
#include <dingo/storage/unique.h>

#include <gtest/gtest.h>

#include "containers.h"
#include "test.h"

namespace dingo {
template <typename T> struct bindings_test : public test<T> {};
TYPED_TEST_SUITE(bindings_test, container_types, );

template <typename BaseTraits> struct indexed_bindings_container_traits : BaseTraits {
    using index_definition_type =
        std::tuple<std::tuple<size_t, index_type::array<3>>>;
};

TEST(bindings_static_check, autodetected_graph) {
    struct Logger {};
    struct Repository {
        explicit Repository(Logger& logger_ref) : logger(logger_ref) {}
        Logger& logger;
    };
    struct Service {
        Service(Repository& repository_ref, Logger& logger_ref)
            : repository(repository_ref), logger(logger_ref) {}
        Repository& repository;
        Logger& logger;
    };

    using application =
        bindings<registration<scope<shared>, storage<Logger>>,
                  registration<scope<shared>, storage<Repository>>,
                  registration<scope<unique>, storage<Service>>>;
    using service_binding = detail::find_binding_definition_t<
        detail::binding_definitions_t<application>, Service,
        detail::no_binding_id>;

    static_assert(bindings_constructible_v<application, Service>);
    static_assert(bindings_resolvable_v<application, Logger&>);
    static_assert(std::is_same_v<
                  detail::binding_factory_adapter_argument_types_t<
                      service_binding, application>,
                  std::tuple<Repository&, Logger&>>);
}

TEST(bindings_static_check,
     autodetected_graph_prefers_highest_arity_for_default_constructible_type) {
    struct Logger {
        int value = 7;
    };
    struct Repository {
        explicit Repository(Logger& logger_ref) : logger(logger_ref) {}
        Logger& logger;
    };
    struct Service {
        Service(Repository& repository_ref, Logger& logger_ref)
            : repository(repository_ref), logger(logger_ref) {}
        Repository& repository;
        Logger& logger;
    };

    using application =
        bindings<registration<scope<shared>, storage<Logger>>,
                  registration<scope<shared>, storage<Repository>>,
                  registration<scope<unique>, storage<Service>>>;

    static_assert(!bindings_constructible_v<application, Service>);
}

TEST(bindings_static_check, external_dependency) {
    struct ILogger {
        virtual ~ILogger() = default;
    };
    struct Logger : ILogger {};
    struct Service {
        explicit Service(ILogger&) {}
    };

    using application =
        bindings<registration<scope<external>, storage<Logger&>,
                               interfaces<ILogger>>,
                  registration<scope<unique>, storage<Service>>>;

    static_assert(bindings_constructible_v<application, Service>);
    static_assert(bindings_resolvable_v<application, ILogger&>);
}

TEST(bindings_static_check, missing_dependency) {
    struct Missing {};
    struct Service {
        explicit Service(Missing&) {}
    };

    using application =
        bindings<registration<scope<unique>, storage<Service>>>;

    static_assert(!bindings_constructible_v<application, Service>);
}

TEST(bindings_static_check, ambiguous_binding) {
    struct IValue {
        virtual ~IValue() = default;
    };
    struct ValueA : IValue {};
    struct ValueB : IValue {};

    using application =
        bindings<registration<scope<shared>, storage<ValueA>, interfaces<IValue>>,
                  registration<scope<shared>, storage<ValueB>, interfaces<IValue>>>;

    static_assert(!bindings_constructible_v<application, IValue&>);
}

TEST(bindings_static_check, ambiguous_dependency) {
    struct IValue {};
    struct ValueA : IValue {};
    struct ValueB : IValue {};
    struct Consumer {
        explicit Consumer(IValue&) {}
    };

    using application =
        bindings<registration<scope<shared>, storage<ValueA>, interfaces<IValue>>,
                  registration<scope<shared>, storage<ValueB>, interfaces<IValue>>,
                  registration<scope<unique>, storage<Consumer>>>;

    static_assert(!bindings_constructible_v<application, Consumer>);
}

TEST(bindings_static_check, cycle_policy) {
    struct B;

    struct A {
        explicit A(B&) {}
    };

    struct B {
        explicit B(A&) {}
    };

    using invalid_cycle =
        bindings<registration<scope<shared>, storage<A>>,
                  registration<scope<shared>, storage<B>>>;
    using valid_cycle =
        bindings<registration<scope<shared_cyclical>, storage<A>>,
                  registration<scope<shared_cyclical>, storage<B>>>;

    static_assert(!bindings_constructible_v<invalid_cycle, A&>);
    static_assert(bindings_constructible_v<valid_cycle, A&>);
}

TEST(bindings_static_check, indexed_request) {
    struct IAnimal {
        virtual ~IAnimal() = default;
    };
    struct Dog : IAnimal {};

    using animals =
        bindings<indexed_registration<value_id<size_t, 1>, scope<shared>,
                                       storage<Dog>, interfaces<IAnimal>>>;

    static_assert(bindings_resolvable_v<animals, IAnimal&,
                                         value_id<size_t, 1>>);
    static_assert(bindings_constructible_v<
                  animals,
                  indexed_request<IAnimal&, value_id<size_t, 1>>>);
}

TYPED_TEST(bindings_test, register_bindings_component) {
    using container_type = TypeParam;

    struct A {};
    struct B {
        explicit B(A& a) : a_(a) {}
        A& a_;
    };

    using app_component =
        bindings<registration<scope<shared>, storage<A>>,
                  registration<scope<unique>, storage<B>>>;

    container_type container;
    container.template register_bindings<app_component, B>();

    auto b1 = container.template resolve<B>();
    auto b2 = container.template resolve<B>();

    ASSERT_EQ(&b1.a_, &b2.a_);
}

TYPED_TEST(bindings_test, register_bindings_autodetected_graph) {
    using container_type = TypeParam;

    struct Logger {};
    struct Repository {
        explicit Repository(Logger& logger) : logger_(logger) {}
        Logger& logger_;
    };
    struct Service {
        Service(Repository& repository, Logger& logger)
            : repository_(repository), logger_(logger) {}
        Repository& repository_;
        Logger& logger_;
    };

    using application =
        bindings<registration<scope<shared>, storage<Logger>>,
                  registration<scope<shared>, storage<Repository>>,
                  registration<scope<unique>, storage<Service>>>;

    container_type container;
    container.template register_bindings<application, Service>();

    auto service = container.template resolve<Service>();

    ASSERT_EQ(&service.repository_.logger_, &service.logger_);
}

TYPED_TEST(bindings_test, register_bindings_supports_external_binds) {
    using container_type = TypeParam;

    struct ILogger {
        virtual ~ILogger() = default;
        virtual int value() const = 0;
    };
    struct Logger : ILogger {
        int value() const override { return 7; }
    };
    struct Service {
        explicit Service(ILogger& logger) : logger_(logger) {}
        ILogger& logger_;
    };

    using application =
        bindings<registration<scope<external>, storage<Logger&>,
                               interfaces<ILogger>>,
                  registration<scope<unique>, storage<Service>>>;

    Logger logger;
    container_type container;
    container.template register_bindings<application, Service>(
        bind<ILogger&>(logger));

    auto service = container.template resolve<Service>();

    ASSERT_EQ(&service.logger_, &logger);
    ASSERT_EQ(service.logger_.value(), 7);
}

TYPED_TEST(bindings_test, register_bindings_uses_parent_runtime_dependencies) {
    using container_type = TypeParam;

    struct Logger {};
    struct Repository {
        explicit Repository(Logger& logger) : logger_(logger) {}
        Logger& logger_;
    };
    struct Service {
        explicit Service(Repository& repository) : repository_(repository) {}
        Repository& repository_;
    };

    using application =
        bindings<registration<scope<shared>, storage<Repository>>>;

    container_type container;
    container.template register_type<scope<shared>, storage<Logger>>();
    container.template register_bindings<application, Service>();

    auto service = container.template resolve<Service>();
    auto& logger = container.template resolve<Logger&>();

    ASSERT_EQ(&service.repository_.logger_, &logger);
}

TYPED_TEST(bindings_test, register_bindings_root_supports_external_binds) {
    using container_type = TypeParam;

    struct ILogger {
        virtual ~ILogger() = default;
        virtual int value() const = 0;
    };
    struct Logger : ILogger {
        int value() const override { return 7; }
    };
    struct Service {
        explicit Service(ILogger& logger) : logger_(logger) {}
        ILogger& logger_;
    };

    using application =
        bindings<registration<scope<external>, storage<Logger&>,
                               interfaces<ILogger>>>;

    Logger logger;
    container_type container;
    container.template register_bindings<application, Service>(
        bind<ILogger&>(logger));

    auto service = container.template resolve<Service>();

    ASSERT_EQ(&service.logger_, &logger);
    ASSERT_EQ(service.logger_.value(), 7);
}

TYPED_TEST(bindings_test, register_bindings_preserves_bindings_shared_state) {
    using container_type = TypeParam;

    struct Repository {};
    struct Service {
        explicit Service(Repository& repository) : repository_(repository) {}
        Repository& repository_;
    };

    using application =
        bindings<registration<scope<shared>, storage<Repository>>>;

    container_type container;
    container.template register_bindings<application, Service>();

    auto service1 = container.template resolve<Service>();
    auto service2 = container.template resolve<Service>();

    ASSERT_EQ(&service1.repository_, &service2.repository_);
}

TYPED_TEST(bindings_test, register_bindings_nested_component) {
    using container_type = TypeParam;

    struct IValue {
        virtual ~IValue() = default;
        virtual int value() const = 0;
    };

    struct Value : IValue {
        int value() const override { return 7; }
    };

    struct Service {
        explicit Service(IValue& value) : value_(value) {}
        IValue& value_;
    };

    using infrastructure =
        bindings<registration<scope<shared>, storage<Value>, interfaces<IValue>>>;
    using application =
        bindings<infrastructure, registration<scope<unique>, storage<Service>>>;

    container_type container;
    container.template register_bindings<application, Service>();

    ASSERT_EQ(container.template resolve<Service>().value_.value(), 7);
}

TYPED_TEST(bindings_test, register_bindings_indexed_bindings) {
    struct IAnimal {
        virtual ~IAnimal() = default;
        virtual int sound() const = 0;
    };

    struct Dog : IAnimal {
        int sound() const override { return 1; }
    };

    struct Cat : IAnimal {
        int sound() const override { return 2; }
    };

    using dog_id = value_id<size_t, 1>;
    using cat_id = value_id<size_t, 2>;

    using container_traits = indexed_bindings_container_traits<
        typename TypeParam::container_traits_type>;

    using animal_component =
        bindings<indexed_registration<dog_id, scope<shared>, storage<Dog>,
                                      interfaces<Dog, IAnimal>>,
                  indexed_registration<cat_id, scope<shared>, storage<Cat>,
                                      interfaces<Cat, IAnimal>>>;

    container<container_traits> container;
    container.template register_bindings<animal_component,
                                         indexed_request<Dog, dog_id>>();

    ASSERT_EQ(container.template resolve<Dog>(size_t(1)).sound(), 1);
}

} // namespace dingo
