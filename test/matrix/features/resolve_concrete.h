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

template <typename Case> void run_resolve_concrete() {
    Case::with_container([](auto& container) {
        config& instance = container.template resolve<config&>();
        config* pointer = container.template resolve<config*>();

        ASSERT_EQ(instance.retries(), 3);
        ASSERT_EQ(pointer->retries(), 3);
        ASSERT_EQ(&instance, pointer);
    });
}

} // namespace dingo::matrix
