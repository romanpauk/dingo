//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include "matrix/support/values.h"

#include <dingo/core/keyed.h>

#include <gtest/gtest.h>

#include <memory>

namespace dingo::matrix {

template <typename Case> void run_resolve_keyed() {
    Case::with_container([](auto& container) {
        auto& first =
            container.template resolve<std::shared_ptr<element_interface>&>(
                dingo::key<key_a>{});
        auto& second =
            container.template resolve<std::shared_ptr<element_interface>&>(
                dingo::key<key_b>{});

        ASSERT_EQ(first->id(), 0);
        ASSERT_EQ(second->id(), 1);
    });
}

} // namespace dingo::matrix
