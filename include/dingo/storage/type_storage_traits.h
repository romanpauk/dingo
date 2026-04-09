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
