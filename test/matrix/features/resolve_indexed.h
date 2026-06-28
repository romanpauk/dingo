//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include "matrix/support/values.h"

#include <gtest/gtest.h>

#include <memory>

namespace dingo::matrix {

template <typename Case> void run_resolve_indexed() {
  Case::with_container([](auto &container) {
    auto first =
        container.template resolve<std::shared_ptr<element_interface>>(0);
    auto second =
        container.template resolve<std::shared_ptr<element_interface>>(1);

    ASSERT_EQ(first->id(), 0);
    ASSERT_EQ(second->id(), 1);
  });
}

} // namespace dingo::matrix
