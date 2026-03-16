//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/static_container.h>

namespace dingo {

template <typename Bindings>
using bindings_container = static_container<Bindings>;

} // namespace dingo
