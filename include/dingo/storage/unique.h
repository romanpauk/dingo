//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/core/config.h>

#include <dingo/factory/constructor.h>
#include <dingo/storage/storage.h>
#include <dingo/storage/type_storage_traits.h>
#include <dingo/type/normalized_type.h>

namespace dingo {
struct unique {};

template <typename Type> struct storage_materialization_traits<unique, Type> {
    template <typename Leaf, typename Context, typename Storage>
    static auto make_guard(Context& context, const Storage&) {
        return detail::recursion_guard<Leaf>(context);
    }

    template <typename Storage>
    static bool preserves_closure(const Storage&) {
        return false;
    }

    template <typename Context, typename Storage, typename Container>
    static auto materialize_source(Context& context, Storage& storage,
                                   Container& container) {
        using source_type = std::remove_cv_t<std::remove_reference_t<
            decltype(storage.resolve(context, container))>>;
        return detail::make_rvalue_source<source_type>(
            std::in_place, [&](void* ptr) {
                new (ptr) source_type(storage.resolve(context, container));
            });
    }
};

template <typename Type, typename U>
struct storage_traits<
    unique, Type, U,
    std::enable_if_t<!type_traits<Type>::enabled && !std::is_reference_v<Type> &&
                     !std::is_array_v<Type> && !is_alternative_type_v<Type>>> {
    static constexpr bool enabled = true;
    static constexpr bool is_stable = false;

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
                     !std::is_array_v<Type> && !is_alternative_type_v<Type>>> {
    using value_types = type_list<std::optional<U>>;
    using lvalue_reference_types = type_list<>;
    using rvalue_reference_types = type_list<std::optional<U>&&>;
    using pointer_types = type_list<>;
    using conversion_types = type_list<std::optional<U>>;
};

template <typename Type, typename U>
struct storage_traits<unique, Type*, U> {
    static constexpr bool enabled = true;
    static constexpr bool is_stable = false;

    using value_types = type_list<std::unique_ptr<U>, std::shared_ptr<U>>;
    using lvalue_reference_types = type_list<>;
    using rvalue_reference_types =
        type_list<std::unique_ptr<U>&&, std::shared_ptr<U>&&>;
    using pointer_types = type_list<U*>;
    using conversion_types = type_list<std::unique_ptr<U>, std::shared_ptr<U>>;
};

template <typename T, typename U>
struct storage_traits<unique, T[], U> {
    static constexpr bool enabled = true;
    static constexpr bool is_stable = false;

    using rebound_unique_handle =
        detail::wrapper_rebind_leaf_t<std::unique_ptr<T[]>, U>;
    using rebound_shared_handle =
        detail::wrapper_rebind_leaf_t<std::shared_ptr<T[]>, U>;

    using value_types = type_list<rebound_unique_handle, rebound_shared_handle>;
    using lvalue_reference_types = type_list<>;
    using rvalue_reference_types =
        type_list<rebound_unique_handle&&, rebound_shared_handle&&>;
    using pointer_types = type_list<typename detail::wrapper_rebind_leaf<T, U>::type*>;
    using conversion_types =
        type_list<rebound_unique_handle, rebound_shared_handle>;
};

template <typename T, size_t N, typename U>
struct storage_traits<unique, T[N], U> {
    static constexpr bool enabled = true;
    static constexpr bool is_stable = false;

    using rebound_unique_handle =
        detail::wrapper_rebind_leaf_t<std::unique_ptr<T[]>, U>;
    using rebound_shared_handle =
        detail::wrapper_rebind_leaf_t<std::shared_ptr<T[]>, U>;
    using rebound_row_type = typename detail::wrapper_rebind_leaf<T, U>::type;
    using rebound_exact_type =
        typename detail::wrapper_rebind_leaf<T[N], U>::type;

    using value_types = type_list<rebound_unique_handle, rebound_shared_handle>;
    using lvalue_reference_types = type_list<>;
    using rvalue_reference_types =
        type_list<rebound_unique_handle&&, rebound_shared_handle&&>;
    using pointer_types =
        type_list<rebound_row_type*, exact_lookup<rebound_exact_type>*>;
    using conversion_types =
        type_list<rebound_unique_handle, rebound_shared_handle>;
};

template <typename Array, typename Deleter, typename U>
struct storage_traits<
    unique, std::unique_ptr<Array, Deleter>, U,
    std::enable_if_t<std::is_array_v<Array> && (std::extent_v<Array, 0> == 0)>> {
    static constexpr bool enabled = true;
    static constexpr bool is_stable = false;

    using rebound_handle =
        detail::wrapper_rebind_leaf_t<std::unique_ptr<Array, Deleter>, U>;
    using shared_handle =
        detail::wrapper_rebind_leaf_t<std::shared_ptr<Array>, U>;

    using value_types = type_list<rebound_handle, shared_handle>;
    using lvalue_reference_types = type_list<>;
    using rvalue_reference_types = type_list<rebound_handle&&, shared_handle&&>;
    using pointer_types = type_list<>;
    using conversion_types = type_list<rebound_handle, shared_handle>;
};

template <typename T, typename Deleter, typename U>
struct storage_traits<unique, std::unique_ptr<T, Deleter>, U,
                      std::enable_if_t<!std::is_array_v<T>>> {
    static constexpr bool enabled = true;
    static constexpr bool is_stable = false;

    using rebound_handle =
        detail::wrapper_rebind_leaf_t<std::unique_ptr<T, Deleter>, U>;
    using inner_handle = detail::wrapper_rebind_leaf_t<T, U>;

    using value_types = type_list<rebound_handle, std::shared_ptr<inner_handle>>;
    using lvalue_reference_types = type_list<>;
    using rvalue_reference_types =
        type_list<rebound_handle&&, std::shared_ptr<inner_handle>&&>;
    using pointer_types = type_list<>;
    using conversion_types =
        type_list<rebound_handle, std::shared_ptr<inner_handle>>;
};

template <typename Array, typename U>
struct storage_traits<
    unique, std::shared_ptr<Array>, U,
    std::enable_if_t<std::is_array_v<Array> && (std::extent_v<Array, 0> == 0)>> {
    static constexpr bool enabled = true;
    static constexpr bool is_stable = false;

    using rebound_handle = detail::wrapper_rebind_leaf_t<std::shared_ptr<Array>, U>;

    using value_types = type_list<rebound_handle>;
    using lvalue_reference_types = type_list<>;
    using rvalue_reference_types = type_list<rebound_handle&&>;
    using pointer_types = type_list<>;
    using conversion_types = type_list<rebound_handle>;
};

template <typename T, typename U>
struct storage_traits<unique, std::shared_ptr<T>, U,
                      std::enable_if_t<!std::is_array_v<T>>> {
    static constexpr bool enabled = true;
    static constexpr bool is_stable = false;

    using rebound_handle = detail::wrapper_rebind_leaf_t<std::shared_ptr<T>, U>;

    using value_types = type_list<rebound_handle>;
    using lvalue_reference_types = type_list<>;
    using rvalue_reference_types = type_list<rebound_handle&&>;
    using pointer_types = type_list<>;
    using conversion_types = type_list<rebound_handle>;
};

template <typename T, typename U>
struct storage_traits<unique, std::optional<T>, U> {
    static constexpr bool enabled = true;
    static constexpr bool is_stable = false;

    using value_types = type_list<std::optional<U>>;
    using lvalue_reference_types = type_list<>;
    using rvalue_reference_types = type_list<std::optional<U>&&>;
    using pointer_types = type_list<>;
    using conversion_types = type_list<std::optional<U>>;
};

template <typename Type, typename U>
struct storage_traits<
    unique, Type, U,
    std::enable_if_t<!type_traits<Type>::enabled && !std::is_reference_v<Type> &&
                     !std::is_array_v<Type> && is_alternative_type_v<Type>>> {
    static constexpr bool enabled = true;
    static constexpr bool is_stable = false;

    using value_types = type_list<U>;
    using lvalue_reference_types = type_list<>;
    using rvalue_reference_types = type_list<U&&>;
    using pointer_types = type_list<>;
    using conversion_types = type_list<>;
};

namespace detail {
template <typename Type, typename U> struct conversions<unique, Type, U>
    : type_storage_traits<unique, Type, U> {};

template <typename Type, typename StoredType, typename Factory,
          typename Conversions>
class storage<unique, Type, StoredType, Factory, Conversions> : Factory {
  public:
    template <typename... Args>
    storage(Args&&... args) : Factory(std::forward<Args>(args)...) {}

    using conversions = Conversions;
    using type = Type;
    using stored_type = StoredType;
    using tag_type = unique;

    template <typename Context, typename Container>
    decltype(auto) resolve(Context& context, Container& container) {
        return Factory::template construct<Type>(context, container);
    }
};

template <typename Type, size_t N, typename StoredType, typename Factory,
          typename Conversions>
class storage<unique, Type[N], StoredType, Factory, Conversions> : Factory {
  public:
    template <typename... Args>
    storage(Args&&... args) : Factory(std::forward<Args>(args)...) {}

    using conversions = Conversions;
    using type = Type[N];
    using stored_type = StoredType;
    using tag_type = unique;

    template <typename Context, typename Container>
    decltype(auto) resolve(Context& context, Container& container) {
        return Factory::template construct<Type[N]>(context, container);
    }
};
} // namespace detail
} // namespace dingo
