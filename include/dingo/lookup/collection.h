//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/core/key.h>
#include <dingo/lookup/tags.h>

namespace dingo {
template <typename Interface>
using collection =
    detail::lookup_definition<Interface, detail::no_lookup_key_t, many>;

} // namespace dingo
