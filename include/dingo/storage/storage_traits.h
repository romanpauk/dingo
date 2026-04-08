//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/config.h>

#include <dingo/resolution/recursion_guard.h>
#include <dingo/storage/detail/materialized_source.h>
#include <dingo/type/type_list.h>
#include <dingo/type/type_traits.h>

#include <cstddef>
#include <type_traits>

namespace dingo {
struct unique;
struct shared;
struct external;
struct shared_cyclical;

template <class T> struct exact_lookup;

namespace detail {
struct no_materialization_scope {
    no_materialization_scope() = default;

    template <typename... Args>
    explicit no_materialization_scope(Args&&...) {}
};

template <typename StorageTag> struct storage_materialization_defaults {
    template <typename Leaf, typename Context, typename Storage>
    static auto make_guard(Context&, const Storage&) {
        return no_materialization_scope();
    }

    template <typename Storage>
    static bool preserves_closure(const Storage&) {
        return false;
    }

    template <typename Context, typename Storage, typename Container>
    static auto materialize_source(Context& context, Storage& storage,
                                   Container& container) {
        return make_resolved_source(storage.resolve(context, container));
    }
};

template <typename StorageTag, typename Conversions, typename Leaf,
          typename Context, typename Storage, typename = void>
struct storage_traits_make_guard {
    static auto apply(Context& context, const Storage& storage) {
        return storage_materialization_defaults<StorageTag>::
            template make_guard<Leaf>(context, storage);
    }
};

template <typename StorageTag, typename Conversions, typename Leaf,
          typename Context, typename Storage>
struct storage_traits_make_guard<
    StorageTag, Conversions, Leaf, Context, Storage,
    std::void_t<decltype(static_cast<Conversions*>(nullptr)
                             ->template make_guard<Leaf>(
                                 std::declval<Context&>(),
                                 std::declval<const Storage&>()))>> {
    static auto apply(Context& context, const Storage& storage) {
        return static_cast<Conversions*>(nullptr)
            ->template make_guard<Leaf>(context, storage);
    }
};

template <typename StorageTag, typename Conversions, typename Storage,
          typename = void>
struct storage_traits_preserves_closure {
    static bool apply(const Storage& storage) {
        return storage_materialization_defaults<StorageTag>::preserves_closure(
            storage);
    }
};

template <typename StorageTag, typename Conversions, typename Storage>
struct storage_traits_preserves_closure<
    StorageTag, Conversions, Storage,
    std::void_t<decltype(static_cast<const Conversions*>(nullptr)
                             ->preserves_closure(
                                 std::declval<const Storage&>()))>> {
    static bool apply(const Storage& storage) {
        return static_cast<const Conversions*>(nullptr)->preserves_closure(
            storage);
    }
};

template <typename StorageTag, typename Conversions, typename Context,
          typename Storage,
          typename Container, typename = void>
struct storage_traits_materialize_source {
    static auto apply(Context& context, Storage& storage, Container& container) {
        return storage_materialization_defaults<StorageTag>::materialize_source(
            context, storage, container);
    }
};

template <typename StorageTag, typename Conversions, typename Context,
          typename Storage,
          typename Container>
struct storage_traits_materialize_source<
    StorageTag, Conversions, Context, Storage, Container,
    std::void_t<decltype(static_cast<Conversions*>(nullptr)
                             ->materialize_source(
                                 std::declval<Context&>(),
                                 std::declval<Storage&>(),
                                 std::declval<Container&>()))>> {
    static auto apply(Context& context, Storage& storage, Container& container) {
        return static_cast<Conversions*>(nullptr)->materialize_source(
            context, storage, container);
    }
};

template <typename StorageTag, typename Conversions>
struct storage_materialization {
    template <typename Leaf, typename Context, typename Storage>
    static auto make_guard(Context& context, const Storage& storage) {
        return storage_traits_make_guard<StorageTag, Conversions, Leaf, Context,
                                         Storage>::apply(context, storage);
    }

    template <typename Storage>
    static bool preserves_closure(const Storage& storage) {
        return storage_traits_preserves_closure<StorageTag, Conversions,
                                                Storage>::apply(storage);
    }

    template <typename Context, typename Storage, typename Container>
    static auto materialize_source(Context& context, Storage& storage,
                                   Container& container) {
        return storage_traits_materialize_source<StorageTag, Conversions,
                                                 Context, Storage,
                                                 Container>::apply(
            context, storage, container);
    }
};
} // namespace detail

template <typename StorageTag, typename Type, typename U, typename = void>
struct storage_traits {
    static constexpr bool enabled = false;
    static constexpr bool is_stable = false;

    using value_types = type_list<>;
    using lvalue_reference_types = type_list<>;
    using rvalue_reference_types = type_list<>;
    using pointer_types = type_list<>;
    using conversion_types = type_list<>;
};

template <typename StorageTag, typename Type, typename U>
struct storage_traits<StorageTag, const Type, U>
    : storage_traits<StorageTag, Type, U> {};

template <typename StorageTag, typename Type, typename U>
struct storage_traits<StorageTag, Type&, U> : storage_traits<StorageTag, Type, U> {};

template <typename StorageTag, typename Type, typename U>
struct storage_traits<StorageTag, const Type&, U>
    : storage_traits<StorageTag, Type, U> {};

namespace detail {
template <typename Type, typename U, typename = void>
struct wrapper_rebind_leaf {
    using type = U;
};

template <typename Type, size_t N, typename U>
struct wrapper_rebind_leaf<Type[N], U, void> {
    using type = typename wrapper_rebind_leaf<Type, U>::type[N];
};

template <typename Type, typename U>
struct wrapper_rebind_leaf<Type[], U, void> {
    using type = typename wrapper_rebind_leaf<Type, U>::type[];
};

template <typename Type, typename U>
struct wrapper_rebind_leaf<
    Type, U,
    std::enable_if_t<type_traits<Type>::enabled && !std::is_pointer_v<Type>>> {
    using type = typename type_traits<Type>::template rebind_t<
        typename wrapper_rebind_leaf<typename type_traits<Type>::value_type,
                                     U>::type>;
};

template <typename Type, typename U>
using wrapper_rebind_leaf_t = typename wrapper_rebind_leaf<Type, U>::type;

template <typename Handle, typename T, typename U, typename = void>
struct smart_array_pointer_types {
    using type = type_list<U*, Handle*>;
};

template <typename Handle, typename T, typename U>
struct smart_array_pointer_types<
    Handle, T, U, std::enable_if_t<std::is_array_v<T>>> {
    using type =
        type_list<exact_lookup<typename wrapper_rebind_leaf<T, U>::type>*,
                  Handle*>;
};

template <typename Handle, typename = void>
struct wrapper_storage_types_impl {
    using lvalue_reference_types = type_list<Handle&>;
    using pointer_types = type_list<Handle*>;
    using copyable_value_types = std::conditional_t<
        std::is_copy_constructible_v<Handle>, type_list<Handle>, type_list<>>;
};

template <typename Handle>
struct wrapper_storage_types_impl<
    Handle, std::enable_if_t<type_traits<Handle>::enabled &&
                             type_traits<Handle>::is_pointer_like>> {
  private:
    // Walk the wrapper chain once and collect all derived interface types
    // together instead of recursing over the same chain separately for each
    // result list.
    using next =
        wrapper_storage_types_impl<typename type_traits<Handle>::value_type>;
    using copyable_handle_types = std::conditional_t<
        std::is_copy_constructible_v<Handle>, type_list<Handle>, type_list<>>;

  public:
    using lvalue_reference_types =
        type_list_cat_t<type_list<Handle&>, typename next::lvalue_reference_types>;
    using pointer_types =
        type_list_cat_t<type_list<Handle*>, typename next::pointer_types>;
    using copyable_value_types = type_list_cat_t<
        copyable_handle_types, typename next::copyable_value_types>;
};

template <typename Handle>
using wrapper_storage_types = wrapper_storage_types_impl<Handle>;
} // namespace detail
} // namespace dingo
