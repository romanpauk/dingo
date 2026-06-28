//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include "matrix/support/values.h"

#include <gtest/gtest.h>

namespace dingo::matrix {

template <typename Case> void run_resolve_concrete() {
  Case::with_container([](auto &container) {
    value_type &instance = container.template resolve<value_type &>();
    value_type *pointer = container.template resolve<value_type *>();

    ASSERT_TRUE(is_constructed_value(instance));
    ASSERT_TRUE(is_constructed_value(*pointer));
    ASSERT_EQ(&instance, pointer);
  });
}

} // namespace dingo::matrix
