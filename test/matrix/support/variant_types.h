//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include "matrix/support/values.h"

#include <variant>

namespace dingo::matrix {

struct variant_a {
  explicit variant_a(value_type &dependency) : value(dependency.marker()) {}

  int value;
};

struct variant_b {
  explicit variant_b(int init) : value(init) {}

  int value;
};

} // namespace dingo::matrix
