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

template <typename... Lists> struct type_list_cat;

template <> struct type_list_cat<> {
    using type = type_list<>;
};

template <typename... Types> struct type_list_cat<type_list<Types...>> {
    using type = type_list<Types...>;
};

template <typename... Head, typename... Tail, typename... Rest>
struct type_list_cat<type_list<Head...>, type_list<Tail...>, Rest...> {
    using type = typename type_list_cat<type_list<Head..., Tail...>, Rest...>::type;
};

template <typename... Lists>
using type_list_cat_t = typename type_list_cat<Lists...>::type;

template <typename TypeList, typename T> struct type_list_contains;

template <typename T> struct type_list_contains<type_list<>, T> : std::false_type {};

template <typename Head, typename... Tail, typename T>
struct type_list_contains<type_list<Head, Tail...>, T>
    : std::conditional_t<std::is_same_v<Head, T>, std::true_type,
                         type_list_contains<type_list<Tail...>, T>> {};

template <typename TypeList, typename T>
static constexpr bool type_list_contains_v = type_list_contains<TypeList, T>::value;

template <typename TypeList, typename T> struct type_list_push_unique;

template <typename... Types, typename T>
struct type_list_push_unique<type_list<Types...>, T> {
    using type = std::conditional_t<type_list_contains_v<type_list<Types...>, T>,
                                    type_list<Types...>, type_list<Types..., T>>;
};

template <typename TypeList, typename T>
using type_list_push_unique_t = typename type_list_push_unique<TypeList, T>::type;

template <typename Accumulator, typename TypeList> struct type_list_unique_accumulate;

template <typename Accumulator>
struct type_list_unique_accumulate<Accumulator, type_list<>> {
    using type = Accumulator;
};

template <typename Accumulator, typename Head, typename... Tail>
struct type_list_unique_accumulate<Accumulator, type_list<Head, Tail...>> {
    using type = typename type_list_unique_accumulate<
        type_list_push_unique_t<Accumulator, Head>, type_list<Tail...>>::type;
};

template <typename TypeList> struct type_list_unique {
    using type =
        typename type_list_unique_accumulate<type_list<>, TypeList>::type;
};

template <typename TypeList>
using type_list_unique_t = typename type_list_unique<TypeList>::type;

template <template <typename> typename Predicate, typename TypeList>
struct type_list_filter;

template <template <typename> typename Predicate>
struct type_list_filter<Predicate, type_list<>> {
    using type = type_list<>;
};

template <template <typename> typename Predicate, typename Head, typename... Tail>
struct type_list_filter<Predicate, type_list<Head, Tail...>> {
  private:
    using tail_type = typename type_list_filter<Predicate, type_list<Tail...>>::type;

  public:
    using type = std::conditional_t<Predicate<Head>::value,
                                    type_list_cat_t<type_list<Head>, tail_type>,
                                    tail_type>;
};

template <template <typename> typename Predicate, typename TypeList>
using type_list_filter_t = typename type_list_filter<Predicate, TypeList>::type;

template <typename... Types>
struct type_list_size : std::integral_constant<size_t, sizeof...(Types)> {};
template <typename... Types>
static constexpr size_t type_list_size_v = type_list_size<Types...>::value;

template <typename RTTI, typename Function>
bool for_type(type_list<>, const typename RTTI::type_index&, Function&&) {
    return false;
}

template <typename RTTI, typename Function, typename Head, typename... Tail>
bool for_type(type_list<Head, Tail...>, const typename RTTI::type_index& type,
              Function&& fn) {
    if (RTTI::template get_type_index<Head>() == type) {
        fn(type_list_iterator<Head>());
        return true;
    } else {
        return for_type<RTTI>(type_list<Tail...>{}, type,
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
