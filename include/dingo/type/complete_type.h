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
// Detect completeness through overload resolution instead of a partial
// specialization. MSVC x64 accepts the specialization-based probe for some
// forward declarations that still need to be rejected during registration.
template <typename T, std::size_t = sizeof(T)>
std::true_type is_complete_impl(int);

template <typename T>
std::false_type is_complete_impl(...);

template <typename T>
struct is_complete : decltype(is_complete_impl<T>(0)) {};

template <typename T>
inline constexpr bool is_complete_v = is_complete<T>::value;

template <typename T>
inline constexpr bool requires_complete_type_v =
    !std::is_void_v<T> && !std::is_function_v<T>;
} // namespace detail

template <typename T, typename = void> struct is_complete : detail::is_complete<T> {};
} // namespace dingo
