//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/config.h>

#include <dingo/resolution/recursion_guard.h>
#include <dingo/storage/materialized_source.h>
#include <dingo/type/type_list.h>
#include <dingo/type/type_traits.h>

namespace dingo {
struct unique;
struct shared;
struct external;
struct shared_cyclical;

template <typename...>
inline constexpr bool always_false_v = false;

namespace detail {
struct no_materialization_scope {
    no_materialization_scope() = default;

    template <typename... Args>
    explicit no_materialization_scope(Args&&...) {}
};
} // namespace detail

template <typename StorageTag, typename Type> struct storage_materialization_traits {
    template <typename Leaf, typename Context, typename Storage>
    static auto make_guard(Context&, const Storage&) {
        static_assert(always_false_v<StorageTag, Type>,
                      "storage_materialization_traits must be specialized for this storage tag");
    }

    template <typename Storage>
    static bool preserves_closure(const Storage&) {
        static_assert(always_false_v<StorageTag, Type>,
                      "storage_materialization_traits must be specialized for this storage tag");
        return false;
    }

    template <typename Context, typename Storage, typename Container>
    static auto materialize_source(Context&, Storage&, Container&) {
        static_assert(always_false_v<StorageTag, Type>,
                      "storage_materialization_traits must be specialized for this storage tag");
    }
};

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

template <typename Type> struct storage_materialization_traits<shared, Type> {
    template <typename Leaf, typename Context, typename Storage>
    static auto make_guard(Context& context, const Storage& storage) {
        return detail::recursion_guard_wrapper<Leaf>(
            context, !storage.is_resolved());
    }

    template <typename Storage>
    static bool preserves_closure(const Storage& storage) {
        // Only unresolved shared storage needs the factory closure to stay on
        // the resolving_context stack while address-based conversions are
        // materialized. Once the instance is resolved, there are no temporary
        // construction artifacts left to preserve.
        return !storage.is_resolved();
    }

    template <typename Context, typename Storage, typename Container>
    static auto materialize_source(Context& context, Storage& storage,
                                   Container& container) {
        return detail::make_resolved_source(storage.resolve(context, container));
    }
};

template <typename Type> struct storage_materialization_traits<external, Type> {
    template <typename Leaf, typename Context, typename Storage>
    static auto make_guard(Context&, const Storage&) {
        return detail::no_materialization_scope();
    }

    template <typename Storage>
    static bool preserves_closure(const Storage&) {
        return false;
    }

    template <typename Context, typename Storage, typename Container>
    static auto materialize_source(Context& context, Storage& storage,
                                   Container& container) {
        return detail::make_resolved_source(storage.resolve(context, container));
    }
};

template <typename Type>
struct storage_materialization_traits<shared_cyclical, Type> {
    template <typename Leaf, typename Context, typename Storage>
    static auto make_guard(Context&, const Storage&) {
        return detail::no_materialization_scope();
    }

    template <typename Storage>
    static bool preserves_closure(const Storage&) {
        return false;
    }

    template <typename Context, typename Storage, typename Container>
    static auto materialize_source(Context& context, Storage& storage,
                                   Container& container) {
        return detail::make_resolved_source(storage.resolve(context, container));
    }
};

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
    static constexpr bool is_stable = true;

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
    static constexpr bool is_stable = true;

    using value_types = type_list<U>;
    using lvalue_reference_types = type_list<U&>;
    using rvalue_reference_types = type_list<>;
    using pointer_types = type_list<U*>;
    using conversion_types = type_list<>;
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

template <typename StorageTag, typename Type, typename U, typename = void>
struct type_storage_traits;

template <typename StorageTag, typename Type, typename U>
struct type_storage_traits<
    StorageTag, Type, U,
    std::enable_if_t<storage_traits<StorageTag, Type, U>::enabled>>
    : detail::combined_storage_types<
          storage_traits<StorageTag, Type, U>,
          resolution_traits<StorageTag, Type, U>> {
    static constexpr bool is_stable =
        storage_traits<StorageTag, Type, U>::is_stable;
};
} // namespace dingo
