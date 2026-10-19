//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <type_traits>

namespace dingo {
namespace detail {
template <typename T, typename, auto... Values>
struct is_brace_constructible_impl : std::false_type {};

template <typename T, auto... Values>
struct is_brace_constructible_impl<T, std::void_t<decltype(T{Values...})>,
                                   Values...> : std::true_type {};

template <typename T, auto... Values>
struct is_brace_constructible
    : is_brace_constructible_impl<T, void, Values...> {};

template <typename T, auto... Values>
inline constexpr bool is_brace_constructible_v =
    is_brace_constructible<T, Values...>::value;
} // namespace detail
} // namespace dingo
