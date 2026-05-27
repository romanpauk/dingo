//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include "matrix/support/types.h"

#include <gtest/gtest.h>

#include <memory>

namespace dingo::matrix {

template <typename Case> void run_resolve_unique_wrapper() {
    Case::with_container([](auto& container) {
        auto instance =
            container.template resolve<std::unique_ptr<unique_interface>&&>();

        ASSERT_EQ(instance->value(), 7);
    });
}

} // namespace dingo::matrix
