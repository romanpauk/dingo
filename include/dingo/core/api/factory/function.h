//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

// dingo/factory/function.h of the next implementation: the function factory,
// which calls a function whose result is the stored object.

#include <dingo/core/projection/construct.h>

namespace dingo {

template <auto Function> struct function {};

} // namespace dingo

namespace dingo::core {

template <auto Function> struct factory_of<::dingo::function<Function>> {
  using type = function_factory<Function>;
};

} // namespace dingo::core
