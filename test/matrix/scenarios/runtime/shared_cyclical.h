//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include "matrix/scenarios/runtime/common.h"

namespace dingo::matrix {

struct shared_cyclical_regression_scenario {
  template <typename Container> static void run(Container &container) {
    cyclical_rollback_b::fail = true;
    container.template register_type<
        dingo::scope<dingo::shared_cyclical>,
        dingo::storage<std::shared_ptr<cyclical_rollback_a>>>();
    container.template register_type<
        dingo::scope<dingo::shared_cyclical>,
        dingo::storage<std::shared_ptr<cyclical_rollback_b>>,
        dingo::interfaces<cyclical_rollback_b, interface_type>>();
    ASSERT_THROW(container.template resolve<cyclical_rollback_a &>(),
                 cyclical_rollback_exception);
    auto &rollback_a = container.template resolve<cyclical_rollback_a &>();
    auto &rollback_b = container.template resolve<cyclical_rollback_b &>();
    ASSERT_EQ(static_cast<interface_type *>(&rollback_b),
              rollback_a.dependency.get());

    container.template register_type<dingo::scope<dingo::shared>,
                                     dingo::storage<cyclical_commit_c>>();
    ASSERT_THROW(container.template resolve<cyclical_commit_c &>(),
                 cyclical_rollback_exception);
    ASSERT_EQ(static_cast<interface_type *>(&rollback_b),
              rollback_a.dependency.get());
  }
};

} // namespace dingo::matrix
