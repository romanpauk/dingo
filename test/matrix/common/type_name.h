//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

// The name of a type as the messages of the library spell it, by the
// implementation the matrix is built against; no public header names it.

#include <string>

#if !defined(DINGO_MATRIX_BASELINE)
#include <dingo/core/primitives/type_name.h>
#else
#include <dingo/base/type_name.h>
#endif

namespace dingo::matrix {

template <typename Type> std::string type_name() {
#if !defined(DINGO_MATRIX_BASELINE)
  return ::dingo::core::type_name_of<Type>();
#else
  return ::dingo::type_name<Type>();
#endif
}

} // namespace dingo::matrix
