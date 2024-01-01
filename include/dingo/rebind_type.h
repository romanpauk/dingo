//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/config.h>

#include <dingo/type_list.h>

#include <memory>
#include <optional>

namespace dingo {
struct runtime_type {};

template <class T, class U> struct rebind_type {
    using type = U;
};

template <typename T, typename U>
using rebind_type_t = typename rebind_type<T, U>::type;

template <class T, class U> struct rebind_type<T&, U> {
    using type = typename rebind_type<T, U>::type&;
};
template <class T, class U> struct rebind_type<T&&, U> {
    using type = typename rebind_type<T, U>::type&&;
};
template <class T, class U> struct rebind_type<T*, U> {
    using type = typename rebind_type<T, U>::type*;
};

template <class T, class U> struct rebind_type<std::shared_ptr<T>, U> {
    using type = std::shared_ptr<U>;
};

template <class T, class U> struct rebind_type<std::optional<T>, U> {
    using type = std::optional<U>;
};

// TODO: how to rebind those properly?
template <class T, class Deleter, class U>
struct rebind_type<std::unique_ptr<T, Deleter>, U> {
    using type = std::unique_ptr<U, std::default_delete<U>>;
};

template <typename U, typename... Args>
struct rebind_type<type_list<Args...>, U> {
    using type = type_list<typename rebind_type<Args, U>::type...>;
};

} // namespace dingo