//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/core/config.h>

#include <dingo/core/key.h>
#include <dingo/registration/annotated.h>
#include <dingo/type/type_traits.h>

#include <type_traits>

namespace dingo {
template <class T, class = void> struct normalized_type;

namespace detail {
template <class T, class = void> struct normalized_type_impl : std::decay<T> {};

template <class T, size_t N> struct normalized_type_impl<T (*)[N], void> {
  using type = std::remove_cv_t<T>[N];
};
template <class T, size_t N> struct normalized_type_impl<T (&)[N], void> {
  using type = std::remove_cv_t<T>[N];
};
template <class T, size_t N>
struct normalized_type_impl<T[N], void> : normalized_type<T> {};
template <class T>
struct normalized_type_impl<T[], void> : normalized_type<T> {};
template <class T>
struct normalized_type_impl<T *, void> : normalized_type<T> {};
template <class T>
struct normalized_type_impl<T &, void> : normalized_type<T> {};
template <class T>
struct normalized_type_impl<T &&, void> : normalized_type<T> {};

template <class T>
struct normalized_type_impl<
    T, std::enable_if_t<type_traits<T>::enabled && !std::is_pointer_v<T>>>
    : normalized_type<typename type_traits<T>::value_type> {};

template <class T, class Tag>
struct normalized_type_impl<annotated<T, Tag>, void>
    : std::decay<annotated<typename normalized_type<T>::type, Tag>> {};

template <class T, class Selector>
struct normalized_type_impl<selected<T, Selector>, void>
    : std::decay<selected<typename normalized_type<T>::type, Selector>> {};
} // namespace detail

template <class T, class Enable>
struct normalized_type
    : detail::normalized_type_impl<std::remove_cv_t<T>, Enable> {};

template <class T> using normalized_type_t = typename normalized_type<T>::type;
} // namespace dingo
