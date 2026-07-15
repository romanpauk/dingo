//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include "matrix/scenarios/runtime/common.h"

namespace dingo::matrix {

struct runtime_unique_reference_regression_scenario {
  template <typename Container> static void run(Container &container) {
    container
        .template register_type<dingo::scope<dingo::shared>,
                                dingo::storage<shared_from_unique_reference>>();
    container.template register_type<
        dingo::scope<dingo::unique>, dingo::storage<unique_reference_value>,
        dingo::factory<dingo::constructor<unique_reference_value()>>>();
    auto &shared = container.template resolve<shared_from_unique_reference &>();
    ASSERT_FALSE(shared.token.expired());
    ASSERT_FALSE(container.template resolve<shared_from_unique_reference &>()
                     .token.expired());

    int destructor_count = 0;
    container.template register_type<dingo::scope<dingo::external>,
                                     dingo::storage<int &>>(destructor_count);
    container.template register_type<
        dingo::scope<dingo::unique>,
        dingo::storage<unique_reference_exception_value>>();
    container.template register_type<
        dingo::scope<dingo::shared>,
        dingo::storage<shared_unique_reference_exception_value>>();
    ASSERT_THROW(
        container.template resolve<shared_unique_reference_exception_value>(),
        std::runtime_error);
    ASSERT_EQ(destructor_count, 1);
  }
};

} // namespace dingo::matrix
