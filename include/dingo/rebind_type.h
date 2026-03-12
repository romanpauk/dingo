//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/config.h>

#include <dingo/type_list.h>
#include <dingo/wrapper_traits.h>

#include <memory>
#include <optional>

namespace dingo {
struct runtime_type {};

template <typename T> struct add_lvalue_reference_if_defined {
    using type = T&;
};

template <> struct add_lvalue_reference_if_defined<void> {
    using type = void;
};

template <typename T> struct add_rvalue_reference_if_defined {
    using type = T&&;
};

template <> struct add_rvalue_reference_if_defined<void> {
    using type = void;
};

template <typename T> struct add_pointer_if_defined {
    using type = T*;
};

template <> struct add_pointer_if_defined<void> {
    using type = void;
};

template <class T, class U, class = void> struct rebind_type {
    using type = void;
};

template <class T, class U>
struct rebind_type<T, U, std::void_t<detail::wrapper_rebind_t<T, U>>> {
    using type = detail::wrapper_rebind_t<T, U>;
};

template <typename T, typename U>
using rebind_type_t = typename rebind_type<T, U>::type;

template <class T, class U> struct rebind_type<const T, U> {
    using type = typename rebind_type<T, U>::type;
};

template <class T, class U> struct rebind_type<T&, U> {
    using type =
        typename add_lvalue_reference_if_defined<typename rebind_type<T, U>::type>::type;
};

template <class T, class U> struct rebind_type<T&&, U> {
    using type =
        typename add_rvalue_reference_if_defined<typename rebind_type<T, U>::type>::type;
};

template <class T, class U> struct rebind_type<T*, U> {
    using type = typename add_pointer_if_defined<typename rebind_type<T, U>::type>::type;
};

template <class T, class U> struct rebind_type<const T*, U> {
    using type = typename add_pointer_if_defined<typename rebind_type<T, U>::type>::type;
};

template <typename U, typename... Args>
struct rebind_type<type_list<Args...>, U> {
    using type = type_list<typename rebind_type<Args, U>::type...>;
};

template <typename... Args> struct type_list_filter_void;

template <> struct type_list_filter_void<> {
    using type = type_list<>;
};

template <typename Head, typename... Tail>
struct type_list_filter_void<Head, Tail...> {
    using tail_type = typename type_list_filter_void<Tail...>::type;
    using type = std::conditional_t<std::is_void_v<Head>, tail_type,
                                    type_list_cat_t<type_list<Head>, tail_type>>;
};

template <typename List, typename U> struct rebind_defined_type_list;

template <typename U, typename... Args>
struct rebind_defined_type_list<type_list<Args...>, U> {
    using type = typename type_list_filter_void<rebind_type_t<Args, U>...>::type;
};

template <typename List, typename U>
using rebind_defined_type_list_t =
    typename rebind_defined_type_list<List, U>::type;

template <typename List, typename U, typename Canonical = runtime_type>
using canonical_rebind_defined_type_list_t =
    rebind_defined_type_list_t<rebind_defined_type_list_t<List, U>, Canonical>;

} // namespace dingo
