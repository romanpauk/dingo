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
template <class T> struct exact_lookup {
    using type = T;
};
template <class T, class = void> struct leaf_type;
template <class T, class U, class = void> struct rebind_type;
template <class T, class U, class = void> struct rebind_leaf_type;

namespace detail {
template <class T> struct decoration_traits {
    using type = T;

    template <class U>
    using rebind_t = U;
};

template <class T> struct decoration_traits<const T> {
    using type = typename decoration_traits<T>::type;

    template <class U>
    using rebind_t = typename decoration_traits<T>::template rebind_t<U>;
};

template <class T> struct decoration_traits<T&> {
    using type = typename decoration_traits<T>::type;

    template <class U>
    using rebind_t = typename decoration_traits<T>::template rebind_t<U>&;
};

template <class T> struct decoration_traits<T&&> {
    using type = typename decoration_traits<T>::type;

    template <class U>
    using rebind_t = typename decoration_traits<T>::template rebind_t<U>&&;
};

template <class T> struct pointer_traits {
    using type = T;

    template <class U>
    using rebind_t = U;
};

template <class T> struct pointer_traits<T*> {
    using type = typename pointer_traits<T>::type;

    template <class U>
    using rebind_t = typename pointer_traits<T>::template rebind_t<U>*;
};

template <class T> struct pointer_traits<const T*> {
    using type = typename pointer_traits<T>::type;

    template <class U>
    using rebind_t = typename pointer_traits<T>::template rebind_t<U>*;
};

template <class T> struct outer_traits {
    using decoration = decoration_traits<T>;
    using pointer = pointer_traits<typename decoration::type>;
    using type = typename pointer::type;

    template <class U>
    using rebind_t = typename decoration::template rebind_t<
        typename pointer::template rebind_t<U>>;
};

template <class T, class = void> struct leaf_base {
    using type = T;
};

template <class T>
struct leaf_base<
    T,
    std::enable_if_t<type_traits<T>::enabled && !std::is_pointer_v<T>>> {
    using type = typename leaf_type<typename type_traits<T>::value_type>::type;
};

template <typename T, size_t N> struct leaf_base<T[N], void> {
    using type = typename leaf_type<T>::type;
};

template <typename T> struct leaf_base<T[], void> {
    using type = typename leaf_type<T>::type;
};

template <typename... Args> struct leaf_base<type_list<Args...>, void> {
    using type = type_list<typename leaf_type<Args>::type...>;
};

template <class T, class U, class = void> struct rebind_base {
    using type = U;
};

template <class T, class U>
struct rebind_base<
    T, U,
    std::enable_if_t<type_traits<T>::enabled && !std::is_pointer_v<T>>> {
    using type = typename type_traits<T>::template rebind_t<U>;
};

template <typename T, size_t N, class U> struct rebind_base<T[N], U, void> {
    using rebound = typename rebind_type<T, U>::type;
    using type = rebound[N];
};

template <typename T, class U> struct rebind_base<T[], U, void> {
    using rebound = typename rebind_type<T, U>::type;
    using type = rebound[];
};

template <typename U, typename... Args>
struct rebind_base<type_list<Args...>, U, void> {
    using type = type_list<typename rebind_type<Args, U>::type...>;
};

template <class T, class U, class = void> struct rebind_leaf_base {
    using type = typename leaf_type<U>::type;
};

template <class T, class U> struct rebind_leaf_base<exact_lookup<T>, U, void> {
    using type = T;
};

template <class T, class U>
struct rebind_leaf_base<
    T, U,
    std::enable_if_t<type_traits<T>::enabled && !std::is_pointer_v<T>>> {
    using type = typename type_traits<T>::template rebind_t<
        typename rebind_leaf_type<typename type_traits<T>::value_type, U>::type>;
};

template <typename T, size_t N, class U>
struct rebind_leaf_base<T[N], U, void> {
    using rebound = typename rebind_leaf_type<T, U>::type;
    using type = rebound[N];
};

template <typename T, class U> struct rebind_leaf_base<T[], U, void> {
    using rebound = typename rebind_leaf_type<T, U>::type;
    using type = rebound[];
};

template <typename U, typename... Args>
struct rebind_leaf_base<type_list<Args...>, U, void> {
    using type = type_list<typename rebind_leaf_type<Args, U>::type...>;
};

template <class T, class U, class = void> struct lookup_base {
    using type = typename rebind_leaf_base<T, runtime_type>::type;
};

template <class T, class U>
struct lookup_base<exact_lookup<T>, U, void> {
    using type = typename rebind_leaf_type<T, runtime_type>::type;
};

template <class T, class U, class = void> struct resolved_base {
    using type = typename rebind_leaf_base<T, U>::type;
};

template <class T, class U>
struct resolved_base<
    exact_lookup<T>, U,
    std::enable_if_t<!std::is_same_v<typename leaf_type<T>::type,
                                     runtime_type>>> {
    using type = T;
};

template <class T, class U>
struct resolved_base<
    exact_lookup<T>, U,
    std::enable_if_t<std::is_same_v<typename leaf_type<T>::type,
                                    runtime_type>>> {
    using type = typename rebind_leaf_type<T, typename leaf_type<U>::type>::type;
};
} // namespace detail

template <class T, class> struct leaf_type {
  private:
    using outer = detail::outer_traits<T>;

  public:
    using type = typename detail::leaf_base<typename outer::type>::type;
};

template <class T> using leaf_type_t = typename leaf_type<T>::type;

template <class T, class U, class> struct rebind_type {
  private:
    using outer = detail::outer_traits<T>;
    using rebound = typename detail::rebind_base<typename outer::type, U>::type;

  public:
    using type = typename outer::template rebind_t<rebound>;
};

template <typename T, typename U>
using rebind_type_t = typename rebind_type<T, U>::type;

template <class T, class U, class> struct rebind_leaf_type {
  private:
    using outer = detail::outer_traits<T>;
    using rebound = typename detail::rebind_leaf_base<typename outer::type, U>::type;

  public:
    using type = typename outer::template rebind_t<rebound>;
};

template <typename T, typename U>
using rebind_leaf_t = typename rebind_leaf_type<T, U>::type;

template <class T, class = void> struct lookup_type {
  private:
    using outer = detail::outer_traits<T>;
    using rebound = typename detail::lookup_base<typename outer::type, runtime_type>::type;

  public:
    using type = typename outer::template rebind_t<rebound>;
};

template <typename T>
using lookup_type_t = typename lookup_type<T>::type;

template <class T, class U, class = void> struct resolved_type {
  private:
    using outer = detail::outer_traits<T>;
    using rebound = typename detail::resolved_base<typename outer::type, U>::type;

  public:
    using type = typename outer::template rebind_t<rebound>;
};

template <typename T, typename U>
using resolved_type_t = typename resolved_type<T, U>::type;

template <class T> struct is_exact_lookup : std::false_type {};
template <class T> struct is_exact_lookup<exact_lookup<T>> : std::true_type {};
template <class T> struct is_exact_lookup<const T> : is_exact_lookup<T> {};
template <class T> struct is_exact_lookup<T&> : is_exact_lookup<T> {};
template <class T> struct is_exact_lookup<T&&> : is_exact_lookup<T> {};
template <class T> struct is_exact_lookup<T*> : is_exact_lookup<T> {};
template <class T> struct is_exact_lookup<const T*> : is_exact_lookup<T> {};

template <class T>
inline constexpr bool is_exact_lookup_v = is_exact_lookup<T>::value;

} // namespace dingo
