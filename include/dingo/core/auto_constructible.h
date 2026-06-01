//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/type/complete_type.h>

#include <type_traits>

namespace dingo {
namespace detail {

template <typename T, bool = is_complete<T>::value>
struct default_auto_constructible : std::false_type {};

template <typename T>
struct default_auto_constructible<T, true>
    : std::bool_constant<std::is_aggregate_v<T>> {};

} // namespace detail

template <typename T>
struct is_auto_constructible : detail::default_auto_constructible<T> {};

} // namespace dingo
