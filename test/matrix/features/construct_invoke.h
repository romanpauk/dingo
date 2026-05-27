//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include "matrix/support/types.h"

#include <gtest/gtest.h>

namespace dingo::matrix {

template <typename Case> void run_construct_invoke() {
    Case::with_container([](auto& container) {
        auto constructed = container.template construct<consumer>();
        ASSERT_EQ(constructed.value, 3);

        auto invoked = container.template invoke(
            [](config& cfg) { return cfg.retries(); });
        ASSERT_EQ(invoked, 3);
    });
}

} // namespace dingo::matrix
