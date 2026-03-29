//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/config.h>

#include <dingo/type/type_list.h>
#include <dingo/type/type_traits.h>

#include <optional>

namespace dingo {
struct unique;
struct shared;
struct external;

template <typename StorageTag, typename Type, typename U, typename = void>
struct resolution_traits {
    using value_types = type_list<>;
    using lvalue_reference_types = type_list<>;
    using rvalue_reference_types = type_list<>;
    using pointer_types = type_list<>;
    using conversion_types = type_list<>;
};

namespace detail {
template <typename AccessTraits, typename ResolutionTraits>
struct combined_storage_types {
    using value_types = type_list_cat_t<typename AccessTraits::value_types,
                                        typename ResolutionTraits::value_types>;
    using lvalue_reference_types =
        type_list_cat_t<typename AccessTraits::lvalue_reference_types,
                        typename ResolutionTraits::lvalue_reference_types>;
    using rvalue_reference_types =
        type_list_cat_t<typename AccessTraits::rvalue_reference_types,
                        typename ResolutionTraits::rvalue_reference_types>;
    using pointer_types = type_list_cat_t<typename AccessTraits::pointer_types,
                                          typename ResolutionTraits::pointer_types>;
    using conversion_types =
        type_list_cat_t<typename AccessTraits::conversion_types,
                        typename ResolutionTraits::conversion_types>;
};
} // namespace detail

template <typename Type, typename U>
struct storage_traits<
    shared, Type, U,
    std::enable_if_t<!type_traits<Type>::enabled && !std::is_reference_v<Type> &&
                     !std::is_array_v<Type>>> {
    static constexpr bool enabled = true;

    using value_types = type_list<U>;
    using lvalue_reference_types = type_list<U&>;
    using rvalue_reference_types = type_list<>;
    using pointer_types = type_list<U*>;
    using conversion_types = type_list<>;
};

template <typename Type, typename U>
struct storage_traits<
    external, Type, U,
    std::enable_if_t<!type_traits<Type>::enabled && !std::is_reference_v<Type> &&
                     !std::is_array_v<Type>>> {
    static constexpr bool enabled = true;

    using value_types = type_list<U>;
    using lvalue_reference_types = type_list<U&>;
    using rvalue_reference_types = type_list<>;
    using pointer_types = type_list<U*>;
    using conversion_types = type_list<>;
};

template <typename Type, typename U>
struct storage_traits<external, Type&, U> : storage_traits<external, Type, U> {};

template <typename Type, typename U>
struct storage_traits<
    unique, Type, U,
    std::enable_if_t<!type_traits<Type>::enabled && !std::is_reference_v<Type> &&
                     !std::is_array_v<Type>>> {
    static constexpr bool enabled = true;

    using value_types = type_list<U>;
    using lvalue_reference_types = type_list<U&>;
    using rvalue_reference_types = type_list<U&&>;
    using pointer_types = type_list<>;
    using conversion_types = type_list<>;
};

template <typename Type, typename U>
struct resolution_traits<
    unique, Type, U,
    std::enable_if_t<!type_traits<Type>::enabled && !std::is_reference_v<Type> &&
                     !std::is_array_v<Type>>> {
    using value_types = type_list<std::optional<U>>;
    using lvalue_reference_types = type_list<>;
    using rvalue_reference_types = type_list<std::optional<U>&&>;
    using pointer_types = type_list<>;
    using conversion_types = type_list<std::optional<U>>;
};

template <typename StorageTag, typename Type, typename U, typename = void>
struct type_storage_traits;

template <typename StorageTag, typename Type, typename U>
struct type_storage_traits<
    StorageTag, Type, U,
    std::enable_if_t<storage_traits<StorageTag, Type, U>::enabled>>
    : detail::combined_storage_types<
          storage_traits<StorageTag, Type, U>,
          resolution_traits<StorageTag, Type, U>> {};
} // namespace dingo
