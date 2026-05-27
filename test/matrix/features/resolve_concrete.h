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
        value_type& instance = container.template resolve<value_type&>();
        value_type* pointer = container.template resolve<value_type*>();

        ASSERT_EQ(instance.marker(), 3);
        ASSERT_EQ(pointer->marker(), 3);
        ASSERT_EQ(&instance, pointer);
    });
}

} // namespace dingo::matrix
