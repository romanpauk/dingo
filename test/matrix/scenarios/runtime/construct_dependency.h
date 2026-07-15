//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include "matrix/scenarios/runtime/common.h"

namespace dingo::matrix {

struct runtime_construct_dependency_regression_scenario {
  template <typename Container> static void run(Container &container) {
    container.template register_type<
        dingo::scope<dingo::shared>,
        dingo::storage<std::shared_ptr<construct_dependency::a>>>();
    container.template register_type<
        dingo::scope<dingo::shared>,
        dingo::storage<std::shared_ptr<construct_dependency::b>>>();
    ASSERT_THROW(container.template construct<construct_dependency::foo>(),
                 dingo::type_not_found_exception);
    auto opted_in =
        container.template construct<construct_dependency::opted_in_foo>();
    EXPECT_TRUE(
        opted_in
            .template dependency<std::shared_ptr<construct_dependency::a>>());
    EXPECT_TRUE(
        opted_in
            .template dependency<std::shared_ptr<construct_dependency::b>>());
    auto aggregate =
        container.template construct<construct_dependency::aggregate_foo>();
    EXPECT_TRUE(
        aggregate
            .template dependency<std::shared_ptr<construct_dependency::a>>());
    EXPECT_TRUE(
        aggregate
            .template dependency<std::shared_ptr<construct_dependency::b>>());
    ASSERT_THROW(
        container
            .template construct<construct_dependency::ambiguous_opted_in_foo>(),
        dingo::type_not_found_exception);
  }
};

} // namespace dingo::matrix

namespace dingo {

template <typename... Deps>
struct is_auto_constructible<
    matrix::construct_dependency::auto_dependencies<Deps...>> : std::true_type {
};

template <>
struct is_auto_constructible<
    matrix::construct_dependency::ambiguous_auto_dependencies>
    : std::true_type {};

} // namespace dingo
