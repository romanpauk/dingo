//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/core/keyed.h>

#include <type_traits>

namespace dingo {

struct none_t {};

template <typename T> struct is_none : std::bool_constant<false> {};
template <> struct is_none<none_t> : std::bool_constant<true> {};
template <typename T> static constexpr auto is_none_v = is_none<T>::value;

namespace detail {

template <typename T> struct is_typed_key : std::false_type {};

template <typename T>
struct is_typed_key<key<T>> : std::bool_constant<!std::is_void_v<T>> {};

template <typename T>
inline constexpr bool is_typed_key_v = is_typed_key<std::decay_t<T>>::value;

template <typename Identity, typename Binding>
struct keyed_binding_identity : Binding {
    using Binding::Binding;
};

} // namespace detail
} // namespace dingo
