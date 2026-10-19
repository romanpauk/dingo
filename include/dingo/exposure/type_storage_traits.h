//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/base/config.h>

#include <dingo/base/type_list.h>
#include <dingo/exposure/object_resolutions.h>
#include <dingo/exposure/resolutions.h>
#include <dingo/exposure/shape_resolutions.h>
#include <dingo/exposure/source.h>
#include <dingo/exposure/storage_traits.h>
#include <dingo/type/type_traits.h>

#include <type_traits>

namespace dingo {
template <typename...> inline constexpr bool always_false_v = false;

namespace detail {
struct no_materialization_scope {
  no_materialization_scope() = default;

  template <typename... Args> explicit no_materialization_scope(Args &&...) {}
};
} // namespace detail

template <typename StorageTag, typename Type, typename = void>
struct storage_materialization_traits {
  static constexpr bool can_retain_source = false;

  template <typename Leaf, typename Context, typename Storage>
  static auto make_guard(Context &, const Storage &) {
    static_assert(always_false_v<StorageTag, Type>,
                  "storage_materialization_traits must be specialized for "
                  "this storage tag");
  }

  template <typename Storage> static bool retains_source(const Storage &) {
    static_assert(always_false_v<StorageTag, Type>,
                  "storage_materialization_traits must be specialized for "
                  "this storage tag");
    return false;
  }

  template <typename Context, typename Storage, typename Container>
  static auto materialize_source(Context &, Storage &, Container &) {
    static_assert(always_false_v<StorageTag, Type>,
                  "storage_materialization_traits must be specialized for "
                  "this storage tag");
  }
};

template <typename StorageTag, typename Type, typename U, typename = void>
struct resolution_traits {
  using value_types = type_list<>;
  using lvalue_reference_types = type_list<>;
  using rvalue_reference_types = type_list<>;
  using pointer_types = type_list<>;
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
  using pointer_types =
      type_list_cat_t<typename AccessTraits::pointer_types,
                      typename ResolutionTraits::pointer_types>;
};
} // namespace detail

template <typename StorageTag, typename Type, typename U, typename = void>
struct type_storage_traits;

template <typename StorageTag, typename Type, typename U>
struct type_storage_traits<
    StorageTag, Type, U,
    std::enable_if_t<storage_traits<StorageTag, Type, U>::enabled>>
    : detail::combined_storage_types<storage_traits<StorageTag, Type, U>,
                                     resolution_traits<StorageTag, Type, U>> {
public:
  static constexpr bool is_stable =
      storage_traits<StorageTag, Type, U>::is_stable;
};
} // namespace dingo
