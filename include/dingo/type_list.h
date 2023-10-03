#pragma once

#include <dingo/config.h>

#include <dingo/type_traits.h>

namespace dingo {
template <typename... Types> struct type_list {};
template <typename T> struct type_list_iterator {
    using type = T;
};

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

template <typename RTTI>
bool find_type(type_list<>, const typename RTTI::type_index&) {
    return false;
}

template <typename RTTI, typename Head, typename... Tail>
bool find_type(type_list<Head, Tail...>,
               const typename RTTI::type_index& type) {
    if (RTTI::template get_type_index<Head>() == type)
        return true;
    else
        return find_type<RTTI>(type_list<Tail...>{}, type);
}
} // namespace dingo
