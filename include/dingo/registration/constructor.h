//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

// dingo/registration/constructor.h of the next implementation: the annotation
// that declares the constructor of a user type, intentionally without any
// includes.

namespace dingo {

template <typename...> struct constructor;

#define DINGO_CONSTRUCTOR(...)                                                 \
  using dingo_constructor_type [[maybe_unused]] =                              \
      ::dingo::constructor<__VA_ARGS__>;                                       \
  __VA_ARGS__

} // namespace dingo
