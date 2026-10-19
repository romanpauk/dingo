//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <type_traits>

namespace dingo {
// collection_traits is defined in type/collection_traits.h, which depends on
// type_traits.h through normalized_type.h. Only a declaration is needed here.
template <class T> struct collection_traits;

template <typename T, typename = void> struct copy_constructible_traits;

namespace detail {
template <typename T, bool IsCollection>
struct copy_constructible_with_collection {
  static constexpr bool value = std::is_copy_constructible_v<T>;
};

template <typename T> struct copy_constructible_with_collection<T, true> {
  static constexpr bool value = copy_constructible_traits<
      typename collection_traits<T>::resolve_type>::value;
};

template <typename T, typename = void> struct copy_constructible_base {
  using value_type = std::remove_cv_t<std::remove_reference_t<T>>;

  static constexpr bool value = std::is_copy_constructible_v<value_type>;
};

template <typename T>
struct copy_constructible_base<
    T, std::void_t<decltype(collection_traits<std::remove_cv_t<
                                std::remove_reference_t<T>>>::is_collection)>> {
  using collection_type =
      collection_traits<std::remove_cv_t<std::remove_reference_t<T>>>;
  using value_type = std::remove_cv_t<std::remove_reference_t<T>>;

  static constexpr bool value =
      copy_constructible_with_collection<value_type,
                                         collection_type::is_collection>::value;
};
} // namespace detail

template <typename T, typename>
struct copy_constructible_traits : detail::copy_constructible_base<T> {};

template <typename T>
inline constexpr bool is_copy_constructible_v =
    copy_constructible_traits<T>::value;
} // namespace dingo
