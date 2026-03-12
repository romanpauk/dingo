//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#include <dingo/container.h>
#include <dingo/index/array.h>
#include <dingo/storage/shared.h>
#include <dingo/storage/unique.h>

#include <gtest/gtest.h>

namespace dingo {

TEST(component_container_test, shared_unique_graph) {
    struct Logger {};
    struct Service {
        explicit Service(Logger& logger) : logger_(logger) {}
        Logger& logger_;
    };

    using application =
        component<registration<scope<shared>, storage<Logger>>,
                  registration<scope<unique>, storage<Service>>>;

    component_container<application> container;
    auto service = container.resolve<Service>();
    auto& logger = container.resolve<Logger&>();

    ASSERT_EQ(&service.logger_, &logger);
}

TEST(component_container_test, nested_autodetected_graph) {
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

    component_container<application> container;
    auto service = container.resolve<Service>();
    auto& logger = container.resolve<Logger&>();
    auto& repository = container.resolve<Repository&>();

    ASSERT_EQ(&service.logger_, &logger);
    ASSERT_EQ(&service.repository_, &repository);
    ASSERT_EQ(&repository.logger_, &logger);
}

TEST(component_container_test, shared_interface_conversion_cache) {
    struct IService {
        virtual ~IService() = default;
    };

    struct Service : IService {};

    using application =
        component<registration<scope<shared>, storage<std::shared_ptr<Service>>,
                               interfaces<IService>>>;

    component_container<application> container;
    auto& service1 = container.resolve<std::shared_ptr<IService>&>();
    auto& service2 = container.resolve<std::shared_ptr<IService>&>();

    ASSERT_EQ(&service1, &service2);
    ASSERT_EQ(service1.get(), service2.get());
}

TEST(component_container_test, indexed_resolution) {
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

    using animals =
        component<indexed_registration<value_id<size_t, 1>, scope<shared>,
                                       storage<Dog>, interfaces<IAnimal>>,
                  indexed_registration<value_id<size_t, 2>, scope<shared>,
                                       storage<Cat>, interfaces<IAnimal>>>;

    component_container<animals> container;

    ASSERT_EQ((container.resolve<indexed_request<IAnimal&, value_id<size_t, 1>>>()
                   .sound()),
              1);
    ASSERT_EQ((container.resolve<indexed_request<IAnimal&, value_id<size_t, 2>>>()
                   .sound()),
              2);
}

TEST(component_container_test, unmanaged_construct) {
    struct Logger {};
    struct Service {
        explicit Service(Logger& logger) : logger_(logger) {}
        Logger& logger_;
    };
    struct Unmanaged {
        explicit Unmanaged(Service service) : service_(std::move(service)) {}
        Service service_;
    };

    using application =
        component<registration<scope<shared>, storage<Logger>>,
                  registration<scope<unique>, storage<Service>>>;

    component_container<application> container;
    auto unmanaged = container.construct<Unmanaged>();
    auto& logger = container.resolve<Logger&>();

    ASSERT_EQ(&unmanaged.service_.logger_, &logger);
}

TEST(component_container_test, explicit_constructor_factory) {
    struct Logger {};
    struct Service {
        explicit Service(Logger& logger) : logger_(logger) {}
        Logger& logger_;
    };

    static_assert(std::is_same_v<
                  constructor<Service(Logger&)>::direct_argument_types,
                  std::tuple<Logger&>>);

    using application =
        component<registration<scope<shared>, storage<Logger>>,
                  registration<scope<unique>, storage<Service>,
                               factory<constructor<Service(Logger&)>>>>;

    component_container<application> container;
    auto service = container.resolve<Service>();
    auto& logger = container.resolve<Logger&>();

    ASSERT_EQ(&service.logger_, &logger);
}

} // namespace dingo
