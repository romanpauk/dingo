//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/core/config.h>

#include <dingo/resolution/recursion_guard.h>
#include <dingo/storage/materialized_source.h>
#include <dingo/type/type_list.h>
#include <dingo/type/type_traits.h>

#include <cstddef>
#include <type_traits>

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

template <typename T, typename List> struct type_list_contains_distinct;

template <typename T>
struct type_list_contains_distinct<T, type_list<>> : std::false_type {};

template <typename T, typename Head, typename... Tail>
struct type_list_contains_distinct<T, type_list<Head, Tail...>>
    : std::bool_constant<
          !std::is_same_v<T, std::remove_cv_t<std::remove_reference_t<Head>>> ||
          type_list_contains_distinct<T, type_list<Tail...>>::value> {};

template <typename T, typename List>
inline constexpr bool type_list_contains_distinct_v =
    type_list_contains_distinct<T, List>::value;

template <typename T, typename List> struct type_list_max_distinct_size;

template <typename T>
struct type_list_max_distinct_size<T, type_list<>>
    : std::integral_constant<std::size_t, 0> {};

template <typename T, typename Head, typename... Tail>
struct type_list_max_distinct_size<T, type_list<Head, Tail...>>
    : std::integral_constant<
          std::size_t,
          !std::is_same_v<T, std::remove_cv_t<std::remove_reference_t<Head>>>
              ? (sizeof(std::remove_cv_t<std::remove_reference_t<Head>>) >
                         type_list_max_distinct_size<T,
                                                     type_list<Tail...>>::value
                     ? sizeof(std::remove_cv_t<std::remove_reference_t<Head>>)
                     : type_list_max_distinct_size<T,
                                                   type_list<Tail...>>::value)
              : type_list_max_distinct_size<T, type_list<Tail...>>::value> {};

template <typename T, typename List>
inline constexpr std::size_t type_list_max_distinct_size_v =
    type_list_max_distinct_size<T, List>::value;

template <typename T, typename List> struct type_list_max_distinct_align;

template <typename T>
struct type_list_max_distinct_align<T, type_list<>>
    : std::integral_constant<std::size_t, 0> {};

template <typename T, typename Head, typename... Tail>
struct type_list_max_distinct_align<T, type_list<Head, Tail...>>
    : std::integral_constant<
          std::size_t,
          !std::is_same_v<T, std::remove_cv_t<std::remove_reference_t<Head>>>
              ? (alignof(std::remove_cv_t<std::remove_reference_t<Head>>) >
                         type_list_max_distinct_align<T,
                                                      type_list<Tail...>>::value
                     ? alignof(std::remove_cv_t<std::remove_reference_t<Head>>)
                     : type_list_max_distinct_align<T,
                                                    type_list<Tail...>>::value)
              : type_list_max_distinct_align<T, type_list<Tail...>>::value> {};

template <typename T, typename List>
inline constexpr std::size_t type_list_max_distinct_align_v =
    type_list_max_distinct_align<T, List>::value;

template <typename T, typename List>
struct type_list_has_nontrivial_distinct_destructor;

template <typename T>
struct type_list_has_nontrivial_distinct_destructor<T, type_list<>>
    : std::false_type {};

template <typename T, typename Head, typename... Tail>
struct type_list_has_nontrivial_distinct_destructor<T, type_list<Head, Tail...>>
    : std::bool_constant<
          (!std::is_same_v<T, std::remove_cv_t<std::remove_reference_t<Head>>> &&
           !std::is_trivially_destructible_v<
               std::remove_cv_t<std::remove_reference_t<Head>>>) ||
          type_list_has_nontrivial_distinct_destructor<
              T, type_list<Tail...>>::value> {};

template <typename T, typename List>
inline constexpr bool type_list_has_nontrivial_distinct_destructor_v =
    type_list_has_nontrivial_distinct_destructor<T, List>::value;

template <typename Storage, typename = void>
struct static_conversion_temporary_slots_base
    : std::integral_constant<std::size_t, 0> {};

template <typename Storage>
struct static_conversion_temporary_slots_base<
    Storage,
    std::void_t<
        decltype(Storage::static_conversion_temporary_slots)>>
    : std::integral_constant<
          std::size_t,
          Storage::static_conversion_temporary_slots> {};

template <typename Storage, typename = void>
struct static_conversion_temporary_size_base
    : std::integral_constant<std::size_t, 0> {};

template <typename Storage>
struct static_conversion_temporary_size_base<
    Storage,
    std::void_t<
        decltype(Storage::static_conversion_temporary_size)>>
    : std::integral_constant<
          std::size_t,
          Storage::static_conversion_temporary_size> {};

template <typename Storage, typename = void>
struct static_conversion_temporary_align_base
    : std::integral_constant<std::size_t, 0> {};

template <typename Storage>
struct static_conversion_temporary_align_base<
    Storage,
    std::void_t<
        decltype(Storage::static_conversion_temporary_align)>>
    : std::integral_constant<
          std::size_t,
          Storage::static_conversion_temporary_align> {};

template <typename Storage, typename = void>
struct static_conversion_destructible_slots_base
    : std::integral_constant<std::size_t, 0> {};

template <typename Storage>
struct static_conversion_destructible_slots_base<
    Storage,
    std::void_t<
        decltype(Storage::static_conversion_destructible_slots)>>
    : std::integral_constant<
          std::size_t,
          Storage::static_conversion_destructible_slots> {};

template <typename Storage, typename = void>
struct static_conversion_temporary_slots
    : static_conversion_temporary_slots_base<Storage> {};

template <typename Storage>
struct static_conversion_temporary_slots<
    Storage, std::void_t<typename Storage::conversions>>
    : static_conversion_temporary_slots_base<typename Storage::conversions> {};

template <typename Storage, typename = void>
struct static_conversion_temporary_size
    : static_conversion_temporary_size_base<Storage> {};

template <typename Storage>
struct static_conversion_temporary_size<
    Storage, std::void_t<typename Storage::conversions>>
    : static_conversion_temporary_size_base<typename Storage::conversions> {};

template <typename Storage, typename = void>
struct static_conversion_temporary_align
    : static_conversion_temporary_align_base<Storage> {};

template <typename Storage>
struct static_conversion_temporary_align<
    Storage, std::void_t<typename Storage::conversions>>
    : static_conversion_temporary_align_base<typename Storage::conversions> {};

template <typename Storage, typename = void>
struct static_conversion_destructible_slots
    : static_conversion_destructible_slots_base<Storage> {};

template <typename Storage>
struct static_conversion_destructible_slots<
    Storage, std::void_t<typename Storage::conversions>>
    : static_conversion_destructible_slots_base<typename Storage::conversions> {
};
} // namespace detail

template <typename Storage>
inline constexpr std::size_t static_conversion_temporary_slots_v =
    detail::static_conversion_temporary_slots<Storage>::value;

template <typename Storage>
inline constexpr std::size_t static_conversion_temporary_size_v =
    detail::static_conversion_temporary_size<Storage>::value;

template <typename Storage>
inline constexpr std::size_t static_conversion_temporary_align_v =
    detail::static_conversion_temporary_align<Storage>::value;

template <typename Storage>
inline constexpr std::size_t static_conversion_destructible_slots_v =
    detail::static_conversion_destructible_slots<Storage>::value;

template <typename StorageTag, typename Type, typename U, typename = void>
struct type_storage_traits;

template <typename StorageTag, typename Type, typename U>
struct type_storage_traits<
    StorageTag, Type, U,
    std::enable_if_t<storage_traits<StorageTag, Type, U>::enabled>>
    : detail::combined_storage_types<
          storage_traits<StorageTag, Type, U>,
          resolution_traits<StorageTag, Type, U>> {
  private:
    using combined_types = detail::combined_storage_types<
        storage_traits<StorageTag, Type, U>,
        resolution_traits<StorageTag, Type, U>>;

  public:
    static constexpr bool is_stable =
        storage_traits<StorageTag, Type, U>::is_stable;
    static constexpr std::size_t static_conversion_temporary_slots =
        detail::type_list_contains_distinct_v<
            Type,
            typename combined_types::conversion_types> &&
                !is_stable
            ? 1
            : 0;
    static constexpr std::size_t static_conversion_temporary_size =
        !is_stable
            ? detail::type_list_max_distinct_size_v<
                  Type, typename combined_types::conversion_types>
            : 0;
    static constexpr std::size_t static_conversion_temporary_align =
        !is_stable
            ? detail::type_list_max_distinct_align_v<
                  Type, typename combined_types::conversion_types>
            : 0;
    static constexpr std::size_t static_conversion_destructible_slots =
            !is_stable &&
                    detail::type_list_has_nontrivial_distinct_destructor_v<
                        Type, typename combined_types::conversion_types>
                ? 1
                : 0;
};
} // namespace dingo
