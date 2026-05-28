//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include "matrix/support/values.h"

namespace dingo::matrix {

struct factory_function_value_type {
  private:
    explicit factory_function_value_type(int init) : value(init) {}

  public:
    static factory_function_value_type create() {
        return factory_function_value_type(9);
    }

    int value;
};

struct factory_constructor_value_type {
    explicit factory_constructor_value_type(value_type& dependency)
        : value(dependency.marker() + 6) {}

    int value;
};

} // namespace dingo::matrix
