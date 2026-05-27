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
        auto constructed = container.template construct<dependent_type>();
        ASSERT_EQ(constructed.value, 3);

        auto invoked = container.invoke(
            [](value_type& dependency) { return dependency.marker(); });
        ASSERT_EQ(invoked, 3);
    });
}

} // namespace dingo::matrix
