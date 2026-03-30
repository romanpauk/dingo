//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

// #include <dingo/config.h>

#include <dingo/factory/constructor.h>
#include <dingo/type/rebind_type.h>
#include <dingo/storage/storage.h>
#include <dingo/type/type_list.h>

namespace dingo {
struct unique;

template <typename T> struct storage {
    using type = T;
    template <typename U> using rebind_t = storage<U>;
};

template <typename T> struct scope {
    using type = T;
    template <typename U> using rebind_t = scope<U>;
};

template <typename... Args> struct interfaces {
    using type = type_list<Args...>;

    template <typename U> using rebind_t = interfaces<U>;
};

template <typename... Args> struct interfaces<type_list<Args...>> {
    using type = type_list<Args...>;

    template <typename U> using rebind_t = interfaces<U>;
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

template <typename... Args>
using registration_scope_t = get_type_t<::dingo::scope<void>, type_list<Args...>>;

template <typename... Args>
using registration_storage_t = get_type_t<
    ::dingo::storage<void>,
    type_list<Args...,
              ::dingo::storage<typename get_type_t<::dingo::factory<void>,
                                                   type_list<Args...>>::type>>>;

template <typename... Args>
using registration_factory_t =
    get_type_t<::dingo::factory<void>,
               type_list<Args...,
                         ::dingo::factory<::dingo::constructor_detection<
                             leaf_type_t<typename registration_storage_t<Args...>::type>>>>>;

template <typename StorageType, typename ScopeType, typename = void>
struct deduced_interface_type {
    using type = ::dingo::interfaces<leaf_type_t<StorageType>>;
};

template <typename StorageType, typename ScopeType>
struct deduced_interface_type<
    StorageType, ScopeType,
    std::void_t<typename ::dingo::detail::alternative_type_interface_types<
        std::remove_cv_t<std::remove_reference_t<StorageType>>>::type>> {
    using type = ::dingo::interfaces<
        typename ::dingo::detail::alternative_type_interface_types<
            std::remove_cv_t<std::remove_reference_t<StorageType>>>::type>;
};

template <typename StorageType, typename ScopeType>
struct deduced_interface_type<
    StorageType, ScopeType,
    std::enable_if_t<!std::is_same_v<typename ScopeType::type, unique> &&
                     type_traits<std::remove_cv_t<
                         std::remove_reference_t<StorageType>>>::enabled &&
                     type_traits<std::remove_cv_t<
                         std::remove_reference_t<StorageType>>>::is_value_borrowable &&
                     is_alternative_type_v<std::remove_cv_t<leaf_type_t<StorageType>>>>> {
    using type = ::dingo::interfaces<
        typename ::dingo::detail::alternative_type_interface_types<
            std::remove_cv_t<leaf_type_t<StorageType>>>::type>;
};

template <typename StorageType, typename ScopeType>
struct deduced_interface_type<
    StorageType, ScopeType,
    std::enable_if_t<!std::is_same_v<typename ScopeType::type, unique> &&
                     type_traits<StorageType>::enabled &&
                     !std::is_pointer_v<StorageType> &&
                     std::is_array_v<typename type_traits<StorageType>::value_type>>> {
    using type = ::dingo::interfaces<typename type_traits<StorageType>::value_type,
                                     leaf_type_t<StorageType>>;
};

template <typename StorageType, typename ScopeType>
struct deduced_interface_type<
    StorageType, ScopeType,
    std::enable_if_t<std::is_array_v<StorageType> &&
                     (std::rank_v<StorageType> > 1)>> {
    using type = ::dingo::interfaces<std::remove_extent_t<StorageType>,
                                     StorageType, leaf_type_t<StorageType>>;
};

template <typename StorageType, typename ScopeType>
struct deduced_interface_type<
    StorageType, ScopeType,
    std::enable_if_t<std::is_array_v<StorageType> &&
                     (std::rank_v<StorageType> == 1)>> {
    using type = ::dingo::interfaces<StorageType, leaf_type_t<StorageType>>;
};

template <typename... Args>
using registration_interface_t =
    get_type_t<::dingo::interfaces<void>,
               type_list<Args...,
                         typename deduced_interface_type<
                             typename registration_storage_t<Args...>::type,
                             registration_scope_t<Args...>>::type,
                         ::dingo::interfaces<leaf_type_t<
                             typename registration_factory_t<Args...>::type>>>>;

} // namespace detail

// TODO:
// the default factory is hardcoded

template <typename... Args> struct type_registration {
    // Scope has to be scpecified as there is no way how to deduce it
    using scope_type = detail::registration_scope_t<Args...>;
    static_assert(!std::is_same_v<scope_type, scope<void>>,
                  "failed to deduce a scope type");

    // Storage can be deduced from Factory
    using storage_type = detail::registration_storage_t<Args...>;
    static_assert(!std::is_same_v<storage_type, storage<void>>,
                  "failed to deduce a storage type");

    // Factory can be deduced from Storage
    using factory_type = detail::registration_factory_t<Args...>;
    static_assert(!std::is_same_v<factory_type, factory<void>>,
                  "failed to deduce a factory type");

    // Interface can be deduced from Storage or Factory
    using interface_type = detail::registration_interface_t<Args...>;
    static_assert(!std::is_same_v<interface_type, interfaces<void>>,
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
