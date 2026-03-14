//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/config.h>

#include <dingo/type_list.h>
#include <dingo/type_traits.h>

#include <type_traits>

namespace dingo {
struct runtime_type {};

template <class T, class U, class = void> struct rebind_type {
    using type = U;
};

template <typename T, typename U>
using rebind_type_t = typename rebind_type<T, U>::type;

template <class T, class U> struct rebind_type<const T, U, void> {
    using type = typename rebind_type<T, U>::type;
};

template <class T, class U> struct rebind_type<T&, U, void> {
    using type = typename rebind_type<T, U>::type&;
};

template <class T, class U> struct rebind_type<T&&, U, void> {
    using type = typename rebind_type<T, U>::type&&;
};

template <class T, class U> struct rebind_type<T*, U, void> {
    using type = typename rebind_type<T, U>::type*;
};

template <class T, class U> struct rebind_type<const T*, U, void> {
    using type = typename rebind_type<T, U>::type*;
};

template <class T, class U>
struct rebind_type<
    T, U,
    std::enable_if_t<type_traits<T>::enabled && !std::is_pointer_v<T>>> {
    using type = typename type_traits<T>::template rebind_t<U>;
};

template <typename U, typename... Args>
struct rebind_type<type_list<Args...>, U, void> {
    using type = type_list<typename rebind_type<Args, U>::type...>;
};

} // namespace dingo
