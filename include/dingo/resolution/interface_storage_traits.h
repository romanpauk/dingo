//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/config.h>
#include <dingo/type/rebind_type.h>
#include <dingo/type/type_traits.h>

namespace dingo {
namespace detail {
template <typename Source, typename Target>
inline constexpr bool is_handle_rebindable_v =
    type_traits<Source>::enabled && type_traits<Source>::is_pointer_like &&
    type_traits<Source>::template is_handle_rebindable<Target>;
} // namespace detail

template <typename Storage, typename Interface>
inline constexpr bool is_interface_storage_rebindable_v =
    detail::is_handle_rebindable_v<Storage,
                                   rebind_leaf_t<Storage, Interface>>;
} // namespace dingo
