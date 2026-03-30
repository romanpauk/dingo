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

template <typename... Lists> struct type_list_cat;

template <> struct type_list_cat<> {
    using type = type_list<>;
};

template <typename... Types> struct type_list_cat<type_list<Types...>> {
    using type = type_list<Types...>;
};

template <typename... Left, typename... Right, typename... Tail>
struct type_list_cat<type_list<Left...>, type_list<Right...>, Tail...>
    : type_list_cat<type_list<Left..., Right...>, Tail...> {};

template <typename... Lists>
using type_list_cat_t = typename type_list_cat<Lists...>::type;

template <typename... Types>
struct type_list_size : std::integral_constant<size_t, sizeof...(Types)> {};
template <typename... Types>
static constexpr size_t type_list_size_v = type_list_size<Types...>::value;

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

template <typename Tuple, size_t I, typename Arg, typename Index>
struct tuple_replace_impl;

template <typename Tuple, size_t I, typename Arg, size_t... Is>
struct tuple_replace_impl<Tuple, I, Arg, std::index_sequence<Is...>> {
    static_assert(I < sizeof...(Is));
    using type = std::tuple<
        std::conditional_t<Is == I, Arg, std::tuple_element_t<Is, Tuple>>...>;
};

template <typename Tuple, size_t I, typename Arg> struct tuple_replace;

template <size_t I, typename Arg, typename... Args>
struct tuple_replace<std::tuple<Args...>, I, Arg> {
    using type =
        typename tuple_replace_impl<std::tuple<Args...>, I, Arg,
                                    std::index_sequence_for<Args...>>::type;
};

} // namespace dingo
