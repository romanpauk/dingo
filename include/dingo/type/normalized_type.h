//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/config.h>

#include <dingo/registration/annotated.h>
#include <dingo/type/type_traits.h>

#include <type_traits>

namespace dingo {
template <class T, class = void> struct normalized_type : std::decay<T> {};

template <class T> struct normalized_type<const T> : normalized_type<T> {};
template <class T, size_t N>
struct normalized_type<T (*)[N], void> {
    using type = T[N];
};
template <class T, size_t N>
struct normalized_type<const T (*)[N], void> {
    using type = T[N];
};
template <class T, size_t N>
struct normalized_type<T (&)[N], void> {
    using type = T[N];
};
template <class T, size_t N>
struct normalized_type<const T (&)[N], void> {
    using type = T[N];
};
template <class T, size_t N> struct normalized_type<T[N]> : normalized_type<T> {};
template <class T> struct normalized_type<T[]> : normalized_type<T> {};
template <class T> struct normalized_type<T*> : normalized_type<T> {};
template <class T> struct normalized_type<const T*> : normalized_type<T> {};
template <class T> struct normalized_type<T&> : normalized_type<T> {};
template <class T> struct normalized_type<const T&> : normalized_type<T> {};
template <class T> struct normalized_type<T&&> : normalized_type<T> {};

template <class T>
struct normalized_type<
    T, std::enable_if_t<type_traits<T>::enabled && !std::is_pointer_v<T>>>
    : normalized_type<typename type_traits<T>::value_type> {};

template <class T, class Tag>
struct normalized_type<annotated<T, Tag>, void>
    : std::decay<annotated<typename normalized_type<T>::type, Tag>> {};

template <class T> using normalized_type_t = typename normalized_type<T>::type;
} // namespace dingo
