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

#include <memory>
#include <optional>
#include <variant>

namespace dingo {
struct unique;
struct shared;
struct external;

template <typename StorageTag, typename Type, typename U, typename = void>
struct type_storage_traits;

template <typename Type, typename U>
struct type_storage_traits<
    shared, Type, U,
    std::enable_if_t<!has_type_traits_v<Type> && !std::is_reference_v<Type>>> {
    using value_types = type_list<U>;
    using lvalue_reference_types = type_list<U&>;
    using rvalue_reference_types = type_list<>;
    using pointer_types = type_list<U*>;
    using conversion_types = type_list<>;
};

template <typename Type, typename U>
struct type_storage_traits<shared, Type*, U> {
    using value_types = type_list<>;
    using lvalue_reference_types = type_list<U&>;
    using rvalue_reference_types = type_list<>;
    using pointer_types = type_list<U*>;
    using conversion_types = type_list<>;
};

template <typename Type, typename U>
struct type_storage_traits<
    external, Type, U,
    std::enable_if_t<!has_type_traits_v<Type> && !std::is_reference_v<Type>>> {
    using value_types = type_list<U>;
    using lvalue_reference_types = type_list<U&>;
    using rvalue_reference_types = type_list<>;
    using pointer_types = type_list<U*>;
    using conversion_types = type_list<>;
};

template <typename Type, typename U>
struct type_storage_traits<external, Type&, U> : type_storage_traits<external, Type, U> {
};

template <typename Type, typename U>
struct type_storage_traits<external, Type*, U> {
    using value_types = type_list<U>;
    using lvalue_reference_types = type_list<U&>;
    using rvalue_reference_types = type_list<>;
    using pointer_types = type_list<U*>;
    using conversion_types = type_list<>;
};

template <typename Type, typename U>
struct type_storage_traits<
    unique, Type, U,
    std::enable_if_t<!has_type_traits_v<Type> && !std::is_reference_v<Type>>> {
    using value_types = type_list<U, std::optional<U>>;
    using lvalue_reference_types = type_list<U&>;
    using rvalue_reference_types = type_list<U&&, std::optional<U>&&>;
    using pointer_types = type_list<>;
    using conversion_types = type_list<std::optional<U>>;
};

template <typename Type, typename U>
struct type_storage_traits<unique, Type*, U> {
    using value_types = type_list<std::unique_ptr<U>, std::shared_ptr<U>>;
    using lvalue_reference_types = type_list<>;
    using rvalue_reference_types =
        type_list<std::unique_ptr<U>&&, std::shared_ptr<U>&&>;
    using pointer_types = type_list<U*>;
    using conversion_types = type_list<std::unique_ptr<U>, std::shared_ptr<U>>;
};

template <typename T, typename Deleter, typename U>
struct type_storage_traits<unique, std::unique_ptr<T, Deleter>, U> {
    using value_types = type_list<std::unique_ptr<U>, std::shared_ptr<U>>;
    using lvalue_reference_types = type_list<>;
    using rvalue_reference_types =
        type_list<std::unique_ptr<U>&&, std::shared_ptr<U>&&>;
    using pointer_types = type_list<>;
    using conversion_types = type_list<std::unique_ptr<U>, std::shared_ptr<U>>;
};

template <typename T, typename Deleter, typename U>
struct type_storage_traits<shared, std::unique_ptr<T, Deleter>, U> {
    using value_types = type_list<U>;
    using lvalue_reference_types = type_list<U&, std::unique_ptr<U>&>;
    using rvalue_reference_types = type_list<>;
    using pointer_types = type_list<U*, std::unique_ptr<U>*>;
    using conversion_types = type_list<>;
};

template <typename T, typename Deleter, typename U>
struct type_storage_traits<external, std::unique_ptr<T, Deleter>, U> {
    using value_types = type_list<>;
    using lvalue_reference_types = type_list<U&, std::unique_ptr<U>&>;
    using rvalue_reference_types = type_list<>;
    using pointer_types = type_list<U*, std::unique_ptr<U>*>;
    using conversion_types = type_list<>;
};

template <typename T, typename U>
struct type_storage_traits<unique, std::shared_ptr<T>, U> {
    using value_types = type_list<std::shared_ptr<U>>;
    using lvalue_reference_types = type_list<>;
    using rvalue_reference_types = type_list<std::shared_ptr<U>&&>;
    using pointer_types = type_list<>;
    using conversion_types = type_list<std::shared_ptr<U>>;
};

template <typename T, typename U>
struct type_storage_traits<shared, std::shared_ptr<T>, U> {
    using value_types = type_list<U, std::shared_ptr<U>>;
    using lvalue_reference_types = type_list<U&, std::shared_ptr<U>&>;
    using rvalue_reference_types = type_list<>;
    using pointer_types = type_list<U*, std::shared_ptr<U>*>;
    using conversion_types = type_list<std::shared_ptr<U>>;
};

template <typename T, typename U>
struct type_storage_traits<external, std::shared_ptr<T>, U> {
    using value_types = type_list<std::shared_ptr<U>>;
    using lvalue_reference_types = type_list<U&, std::shared_ptr<U>&>;
    using rvalue_reference_types = type_list<>;
    using pointer_types = type_list<U*, std::shared_ptr<U>*>;
    using conversion_types = type_list<std::shared_ptr<U>>;
};

template <typename T, typename U>
struct type_storage_traits<unique, std::optional<T>, U> {
    using value_types = type_list<std::optional<U>>;
    using lvalue_reference_types = type_list<>;
    using rvalue_reference_types = type_list<std::optional<U>&&>;
    using pointer_types = type_list<>;
    using conversion_types = type_list<std::optional<U>>;
};

template <typename... Ts>
struct type_storage_traits<unique, std::variant<Ts...>, std::variant<Ts...>> {
    using value_types = type_list<std::variant<Ts...>>;
    using lvalue_reference_types = type_list<>;
    using rvalue_reference_types = type_list<std::variant<Ts...>&&>;
    using pointer_types = type_list<>;
    using conversion_types = type_list<std::variant<Ts...>>;
};

template <typename T, typename U>
struct type_storage_traits<shared, std::optional<T>, U> {
    using value_types = type_list<>;
    using lvalue_reference_types = type_list<U&, std::optional<U>&>;
    using rvalue_reference_types = type_list<>;
    using pointer_types = type_list<U*, std::optional<U>*>;
    using conversion_types = type_list<>;
};

template <typename T, typename U>
struct type_storage_traits<external, std::optional<T>, U> {
    using value_types = type_list<>;
    using lvalue_reference_types = type_list<U&, std::optional<U>&>;
    using rvalue_reference_types = type_list<>;
    using pointer_types = type_list<U*, std::optional<U>*>;
    using conversion_types = type_list<>;
};
} // namespace dingo
