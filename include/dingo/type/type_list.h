//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/config.h>

#include <cstddef>
#include <initializer_list>
#include <tuple>
#include <type_traits>
#include <utility>

namespace dingo {
template <typename... Types> struct type_list {};
template <typename T> struct type_list_iterator {
    using type = T;
};

namespace detail {
template <typename T> struct type_list_as_tuple;

template <typename... Types>
struct type_list_as_tuple<type_list<Types...>> {
    using type = std::tuple<Types...>;
};

template <typename T> struct tuple_as_type_list;

template <typename... Types>
struct tuple_as_type_list<std::tuple<Types...>> {
    using type = type_list<Types...>;
};
} // namespace detail

template <typename... Lists> struct type_list_cat {
    using type = typename detail::tuple_as_type_list<
        decltype(std::tuple_cat(std::declval<typename detail::type_list_as_tuple<
                                    Lists>::type>()...))>::type;
};

template <typename... Lists>
using type_list_cat_t = typename type_list_cat<Lists...>::type;

template <typename List> struct type_list_size;

template <typename... Types>
struct type_list_size<type_list<Types...>>
    : std::integral_constant<size_t, sizeof...(Types)> {};

template <typename List>
static constexpr size_t type_list_size_v = type_list_size<List>::value;

template <typename List> struct type_list_head;

template <typename Head, typename... Tail>
struct type_list_head<type_list<Head, Tail...>> {
    using type = Head;
};

template <typename List>
using type_list_head_t = typename type_list_head<List>::type;

template <typename T> struct to_type_list {
    using type = T;
};

template <typename... Types> struct to_type_list<std::tuple<Types...>> {
    using type = type_list<typename to_type_list<Types>::type...>;
};

template <typename T>
using to_type_list_t = typename to_type_list<T>::type;

template <typename T> struct to_tuple {
    using type = T;
};

template <typename... Types> struct to_tuple<type_list<Types...>> {
    using type = std::tuple<typename to_tuple<Types>::type...>;
};

template <typename T>
using to_tuple_t = typename to_tuple<T>::type;

template <typename RTTI, typename Function, typename... Types>
bool for_type(type_list<Types...>, const typename RTTI::type_index& type,
              Function&& fn) {
    bool matched = false;
    (void)std::initializer_list<int>{
        (((!matched && RTTI::template get_type_index<Types>() == type)
              ? (fn(type_list_iterator<Types>{}), matched = true, 0)
              : 0))...};
    return matched;
}

template <typename Function, typename... Types>
void for_each(type_list<Types...>, Function&& fn) {
    (fn(type_list_iterator<Types>{}), ...);
}

} // namespace dingo
