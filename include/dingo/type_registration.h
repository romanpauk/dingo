//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

// #include <dingo/config.h>

#include <dingo/factory/constructor.h>
#include <dingo/rebind_type.h>
#include <dingo/storage.h>
#include <dingo/type_list.h>

namespace dingo {
template <typename T> struct storage {
    using type = T;
    template <typename U> using rebind_t = storage<U>;
};

template <typename T> struct scope {
    using type = T;
    template <typename U> using rebind_t = scope<U>;
};

template <typename... Args> struct interface {
    using type = type_list<Args...>;
    using type_tuple = std::tuple<Args...>;

    template <typename U> using rebind_t = interface<U>;
};

template <typename... Args> struct interface<type_list<Args...>> {
    using type = type_list<Args...>;
    using type_tuple = std::tuple<Args...>;

    template <typename U> using rebind_t = interface<U>;
};

template <typename T> struct factory {
    using type = T;
    template <typename U> using rebind_t = factory<U>;
};

template <typename T> struct conversions {
    using type = T;
    template <typename U> using rebind_t = conversions<U>;
};

namespace detail {
template <typename T, typename...> struct get_type;
template <typename T> struct get_type<T, type_list<>> {
    using type = T;
};

template <typename T, typename Head, typename... Tail>
struct get_type<T, type_list<Head, Tail...>> {
    using type = std::conditional_t<
        std::is_same_v<T, typename Head::template rebind_t<void>> &&
            !std::is_same_v<Head, T>, /* &&... is needed for interface deduction
                                       */
        Head, typename get_type<T, type_list<Tail...>>::type>;
};

template <typename T, typename... Args>
using get_type_t = typename get_type<T, Args...>::type;

} // namespace detail

// TODO:
// the default factory is hardcoded

template <typename... Args> struct type_registration {
    // Scope has to be scpecified as there is no way how to deduce it
    using scope_type = detail::get_type_t<scope<void>, type_list<Args...>>;
    static_assert(!std::is_same_v<scope_type, scope<void>>,
                  "failed to deduce a scope type");

    // Storage can be deduced from Factory
    using storage_type = detail::get_type_t<
        storage<void>,
        type_list<Args..., storage<typename detail::get_type_t<
                               factory<void>, type_list<Args...>>::type>>>;
    static_assert(!std::is_same_v<storage_type, storage<void>>,
                  "failed to deduce a storage type");

    // Factory can be deduced from Storage
    using factory_type = detail::get_type_t<
        factory<void>,
        type_list<Args...,
                  factory<constructor_detection<decay_t<typename detail::get_type_t<
                      storage<void>, type_list<Args...>>::type>>>>>;
    static_assert(!std::is_same_v<factory_type, factory<void>>,
                  "failed to deduce a factory type");

    // Interface can be deduced from Storage or Factory
    using interface_type = detail::get_type_t<
        interface<void>,
        type_list<Args...,
                  interface<decay_t<typename detail::get_type_t<
                      storage<void>, type_list<Args...>>::type>>,
                  interface<decay_t<typename detail::get_type_t<
                      factory<void>, type_list<Args...>>::type>>>>;
    static_assert(!std::is_same_v<interface_type, interface<void>>,
                  "failed to deduce an interface type");

    // Conversions are deduced from Storage and Scope
    using conversions_type = detail::get_type_t<
        conversions<void>,
        type_list<Args..., conversions<detail::conversions<
                               typename scope_type::type,
                               typename storage_type::type, runtime_type>>>>;
    static_assert(!std::is_same_v<conversions_type, conversions<void>>,
                  "failed to deduce a conversions type");
};

} // namespace dingo
