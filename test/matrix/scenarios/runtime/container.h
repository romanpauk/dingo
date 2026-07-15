//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include "matrix/scenarios/runtime/common.h"

namespace dingo::matrix {

struct runtime_container_regression_scenario {
  template <typename Container> static void run(Container &container) {
    container.template register_type<
        dingo::scope<dingo::unique>,
        dingo::storage<std::shared_ptr<unique_hierarchy_s>>>();
    container.template register_type<
        dingo::scope<dingo::unique>,
        dingo::storage<std::unique_ptr<unique_hierarchy_u>>>();
    container.template register_type<dingo::scope<dingo::unique>,
                                     dingo::storage<unique_hierarchy_b>>();
    container.template resolve<unique_hierarchy_b>();

    container.template register_type<
        dingo::scope<dingo::shared>,
        dingo::storage<std::shared_ptr<std::unique_ptr<nested_wrapper_value>>>,
        dingo::interfaces<interface_type>>();
    container.template register_type<dingo::scope<dingo::unique>,
                                     dingo::storage<nested_wrapper_consumer>>();
    auto consumer = container.template resolve<nested_wrapper_consumer>();
    ASSERT_TRUE(is_constructed_value(consumer.value));
    ASSERT_EQ(consumer.pointer, std::addressof(consumer.value));

    clear_rollback_stats();
    container.template register_type<dingo::scope<dingo::shared>,
                                     dingo::storage<rollback_a>>();
    container.template register_type<dingo::scope<dingo::shared>,
                                     dingo::storage<rollback_b>>();
    container.template register_type<dingo::scope<dingo::shared>,
                                     dingo::storage<rollback_c>>();
    ASSERT_THROW(container.template resolve<rollback_c &>(),
                 rollback_exception);
    ASSERT_EQ(rollback_a::constructor_count, 1u);
    ASSERT_EQ(rollback_a::destructor_count, 1u);
    ASSERT_EQ(rollback_b::constructor_count, 1u);
    ASSERT_EQ(rollback_b::destructor_count, 1u);
    container.template resolve<rollback_a &>();
    ASSERT_THROW(container.template resolve<rollback_c>(), rollback_exception);
    ASSERT_EQ(rollback_a::constructor_count, 2u);
    ASSERT_EQ(rollback_a::destructor_count, 1u);
    ASSERT_EQ(rollback_b::constructor_count, 2u);
    ASSERT_EQ(rollback_b::destructor_count, 2u);

    clear_reentrant_resolution_stats();
    bool reentrant_should_throw = true;
    container.template register_type<
        dingo::scope<dingo::shared>,
        dingo::storage<reentrant_resolution_dependency>>();
    container
        .template register_type<dingo::scope<dingo::shared>,
                                dingo::storage<reentrant_resolution_service>>(
            dingo::callable([&container, &reentrant_should_throw] {
              auto &dependency =
                  container
                      .template resolve<reentrant_resolution_dependency &>();
              if (reentrant_should_throw) {
                throw rollback_exception();
              }
              return reentrant_resolution_service{std::addressof(dependency)};
            }));
    ASSERT_THROW(container.template resolve<reentrant_resolution_service &>(),
                 rollback_exception);
    ASSERT_EQ(reentrant_resolution_dependency::constructor_count, 1u);
    ASSERT_EQ(reentrant_resolution_dependency::destructor_count, 1u);
    reentrant_should_throw = false;
    auto &reentrant_service =
        container.template resolve<reentrant_resolution_service &>();
    auto &reentrant_dependency =
        container.template resolve<reentrant_resolution_dependency &>();
    ASSERT_EQ(reentrant_service.dependency,
              std::addressof(reentrant_dependency));
    ASSERT_EQ(reentrant_resolution_dependency::constructor_count, 2u);
    ASSERT_EQ(reentrant_resolution_dependency::destructor_count, 1u);

    ASSERT_THROW(
        (container.template register_type<dingo::scope<dingo::shared>,
                                          dingo::storage<rollback_a>>()),
        dingo::lookup_already_registered_exception);
  }
};

} // namespace dingo::matrix
