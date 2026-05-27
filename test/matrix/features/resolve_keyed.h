//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include "matrix/support/types.h"

#include <dingo/core/keyed.h>

#include <gtest/gtest.h>

#include <memory>

namespace dingo::matrix {

template <typename Case> void run_resolve_keyed() {
    Case::with_container([](auto& container) {
        auto& first =
            container.template resolve<std::shared_ptr<processor_interface>&>(
                dingo::key<first_key>{});
        auto& second =
            container.template resolve<std::shared_ptr<processor_interface>&>(
                dingo::key<second_key>{});

        ASSERT_EQ(first->id(), 0);
        ASSERT_EQ(second->id(), 1);
    });
}

} // namespace dingo::matrix
