//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include "matrix/common/assertions.h"
#include "matrix/fixtures/factory.h"

#include <dingo/factory/callable.h>

namespace dingo::matrix {

struct explicit_callable_construct_scenario {
  template <typename Container> static void run(Container &container) {
    auto constructed =
        container.template construct<factory_callable_value_type>(
            dingo::callable<factory_callable_value_type(value_type &)>(
                factory_overloaded_callable{}));
    ASSERT_EQ(constructed.value, 21);
  }
};

} // namespace dingo::matrix
