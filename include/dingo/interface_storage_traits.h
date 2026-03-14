//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/config.h>
#include <dingo/type_traits.h>

namespace dingo {
template <typename T>
inline constexpr bool is_interface_storage_rebindable_v =
    has_type_traits_v<T> && type_traits<T>::is_pointer_like;
} // namespace dingo
