//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/config.h>

namespace dingo {
template <typename... Types> struct type_list {};
template <typename T> struct type_list_iterator {
    using type = T;
};

template <typename... Types>
struct type_list_size : std::integral_constant<size_t, sizeof...(Types)> {};
template <typename... Types>
static constexpr size_t type_list_size_v = type_list_size<Types...>::value;

template <typename TypeIdentity, typename Function>
bool for_type(type_list<>, const typename TypeIdentity::type_index&,
              Function&&) {
    return false;
}

template <typename TypeIdentity, typename Function, typename Head,
          typename... Tail>
bool for_type(type_list<Head, Tail...>,
              const typename TypeIdentity::type_index& type, Function&& fn) {
    if (TypeIdentity::template get<Head>() == type) {
        fn(type_list_iterator<Head>());
        return true;
    } else {
        return for_type<TypeIdentity>(type_list<Tail...>{}, type,
                                      std::forward<Function>(fn));
    }
}

template <typename Function> void for_each(type_list<>, Function&&) {}

template <typename Function, typename Head, typename... Tail>
void for_each(type_list<Head, Tail...>, Function&& fn) {
    fn(type_list_iterator<Head>());
    for_each(type_list<Tail...>{}, std::forward<Function>(fn));
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
