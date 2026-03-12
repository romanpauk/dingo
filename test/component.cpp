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
template <typename T> struct component_test : public test<T> {};
TYPED_TEST_SUITE(component_test, container_types, );

template <typename BaseTraits> struct indexed_component_container_traits : BaseTraits {
    using index_definition_type =
        std::tuple<std::tuple<size_t, index_type::array<3>>>;
};

TEST(component_static_check, autodetected_graph) {
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
        component<registration<scope<shared>, storage<Logger>>,
                  registration<scope<shared>, storage<Repository>>,
                  registration<scope<unique>, storage<Service>>>;
    using service_binding = detail::find_component_binding_t<
        detail::component_bindings_t<application>, Service,
        detail::component_no_id>;

    static_assert(component_constructible_v<application, Service>);
    static_assert(component_resolvable_v<application, Logger&>);
    static_assert(std::is_same_v<
                  detail::component_binding_factory_argument_types_t<
                      service_binding, application>,
                  std::tuple<Repository&, Logger&>>);
}

TEST(component_static_check,
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
        component<registration<scope<shared>, storage<Logger>>,
                  registration<scope<shared>, storage<Repository>>,
                  registration<scope<unique>, storage<Service>>>;

    static_assert(!component_constructible_v<application, Service>);
}

TEST(component_static_check, missing_dependency) {
    struct Missing {};
    struct Service {
        explicit Service(Missing&) {}
    };

    using application =
        component<registration<scope<unique>, storage<Service>>>;

    static_assert(!component_constructible_v<application, Service>);
}

TEST(component_static_check, ambiguous_binding) {
    struct IValue {
        virtual ~IValue() = default;
    };
    struct ValueA : IValue {};
    struct ValueB : IValue {};

    using application =
        component<registration<scope<shared>, storage<ValueA>, interfaces<IValue>>,
                  registration<scope<shared>, storage<ValueB>, interfaces<IValue>>>;

    static_assert(!component_constructible_v<application, IValue&>);
}

TEST(component_static_check, cycle_policy) {
    struct B;

    struct A {
        explicit A(B&) {}
    };

    struct B {
        explicit B(A&) {}
    };

    using invalid_cycle =
        component<registration<scope<shared>, storage<A>>,
                  registration<scope<shared>, storage<B>>>;
    using valid_cycle =
        component<registration<scope<shared_cyclical>, storage<A>>,
                  registration<scope<shared_cyclical>, storage<B>>>;

    static_assert(!component_constructible_v<invalid_cycle, A&>);
    static_assert(component_constructible_v<valid_cycle, A&>);
}

TEST(component_static_check, indexed_request) {
    struct IAnimal {
        virtual ~IAnimal() = default;
    };
    struct Dog : IAnimal {};

    using animals =
        component<indexed_registration<value_id<size_t, 1>, scope<shared>,
                                       storage<Dog>, interfaces<IAnimal>>>;

    static_assert(component_resolvable_v<animals, IAnimal&,
                                         value_id<size_t, 1>>);
    static_assert(component_constructible_v<
                  animals,
                  indexed_request<IAnimal&, value_id<size_t, 1>>>);
}

TYPED_TEST(component_test, install_component) {
    using container_type = TypeParam;

    struct A {};
    struct B {
        explicit B(A& a) : a_(a) {}
        A& a_;
    };

    using app_component =
        component<registration<scope<shared>, storage<A>>,
                  registration<scope<unique>, storage<B>>>;

    container_type container;
    install<app_component>(container);

    auto b = container.template resolve<B>();
    auto& a = container.template resolve<A&>();

    ASSERT_EQ(&b.a_, &a);
}

TYPED_TEST(component_test, install_autodetected_graph) {
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
        component<registration<scope<shared>, storage<Logger>>,
                  registration<scope<shared>, storage<Repository>>,
                  registration<scope<unique>, storage<Service>>>;

    container_type container;
    install<application>(container);

    auto service = container.template resolve<Service>();
    auto& logger = container.template resolve<Logger&>();
    auto& repository = container.template resolve<Repository&>();

    ASSERT_EQ(&service.logger_, &logger);
    ASSERT_EQ(&service.repository_, &repository);
    ASSERT_EQ(&repository.logger_, &logger);
}

TYPED_TEST(component_test, install_nested_component) {
    using container_type = TypeParam;

    struct IValue {
        virtual ~IValue() = default;
        virtual int value() const = 0;
    };

    struct Value : IValue {
        int value() const override { return 7; }
    };

    using infrastructure =
        component<registration<scope<shared>, storage<Value>, interfaces<IValue>>>;
    using application = component<infrastructure>;

    container_type container;
    install<application>(container);

    ASSERT_EQ(container.template resolve<IValue&>().value(), 7);
}

TYPED_TEST(component_test, install_indexed_component) {
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

    using container_traits = indexed_component_container_traits<
        typename TypeParam::container_traits_type>;

    using animal_component =
        component<indexed_registration<value_id<size_t, 1>, scope<shared>,
                                       storage<Dog>, interfaces<IAnimal>>,
                  indexed_registration<value_id<size_t, 2>, scope<shared>,
                                       storage<Cat>, interfaces<IAnimal>>>;

    container<container_traits> container;
    install<animal_component>(container);

    ASSERT_EQ(container.template resolve<IAnimal&>(size_t(1)).sound(), 1);
    ASSERT_EQ(container.template resolve<IAnimal&>(size_t(2)).sound(), 2);
}

} // namespace dingo
