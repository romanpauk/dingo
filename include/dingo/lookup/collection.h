//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/lookup/tags.h>

namespace dingo {
template <typename Interface>
using collection = detail::lookup_definition<Interface, no_key, many>;

} // namespace dingo
