//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

// Intentionally without any includes

namespace dingo {

template <typename...> struct constructor;
#define DINGO_CONSTRUCTOR(...)                                                 \
    using dingo_constructor_type = ::dingo::constructor<__VA_ARGS__>;          \
    __VA_ARGS__

} // namespace dingo