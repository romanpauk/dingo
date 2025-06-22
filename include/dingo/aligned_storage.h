//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <algorithm>
#include <cstdint>

namespace dingo {
template <std::size_t Len, std::size_t Alignment> struct aligned_storage {
    struct type {
        alignas(Alignment) std::uint8_t data[Len];
    };
};

template <std::size_t Len, std::size_t Alignment>
using aligned_storage_t = typename aligned_storage<Len, Alignment>::type;

template <std::size_t MinLen, typename... Ts> struct aligned_union {
    static constexpr std::size_t length = std::max({MinLen, sizeof(Ts)...});
    static constexpr std::size_t alignment = std::max({alignof(Ts)...});

    using type = typename aligned_storage<length, alignment>::type;
};

template <std::size_t MinLen, typename... Ts>
using aligned_union_t = typename aligned_union<MinLen, Ts...>::type;
} // namespace dingo
