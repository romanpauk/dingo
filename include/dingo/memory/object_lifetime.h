//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/core/config.h>

#include <cstddef>
#include <type_traits>

namespace dingo {
namespace detail {
template <typename Type> void destroy_object_value(Type& value) {
    if constexpr (std::is_array_v<Type>) {
        for (std::size_t i = std::extent_v<Type>; i > 0; --i) {
            destroy_object_value(value[i - 1]);
        }
    } else if constexpr (!std::is_trivially_destructible_v<Type>) {
        value.~Type();
    }
}
} // namespace detail
} // namespace dingo
