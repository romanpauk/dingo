//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <type_traits>

namespace dingo {

struct none_t {};

template <typename T> struct is_none : std::bool_constant<false> {};
template <> struct is_none<none_t> : std::bool_constant<true> {};
template <typename T> static constexpr auto is_none_v = is_none<T>::value;

} // namespace dingo
