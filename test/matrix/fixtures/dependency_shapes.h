//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include "matrix/fixtures/variant_types.h"

#include <variant>

namespace dingo::matrix {

template <typename Type> using dependency_array = Type[2];

template <typename Type>
using dependency_variant = std::variant<Type, variant_b>;

} // namespace dingo::matrix
