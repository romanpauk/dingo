//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

// dingo/runtime_container.h of the next implementation: the runtime-only
// spelling of the container.

#include <dingo/core/api/container.h> // IWYU pragma: export

namespace dingo {

template <typename Traits = dynamic_container_traits, typename... Rest>
using runtime_container = container<Traits, Rest...>;

} // namespace dingo
