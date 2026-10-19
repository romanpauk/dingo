//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/type/primary.h>
#include <dingo/type/smart_pointer.h>

#include <memory>
#include <type_traits>

namespace dingo {
namespace detail {
template <typename Type, typename = void>
struct is_array_like_type : std::is_array<Type> {};

template <typename Type>
inline constexpr bool is_array_like_type_v = is_array_like_type<Type>::value;

template <typename Type, typename = void>
struct array_like_exact_interface_type {
  using type = Type;
};

template <typename Type>
using array_like_exact_interface_type_t =
    typename array_like_exact_interface_type<Type>::type;

template <typename Array, typename Deleter>
struct is_array_like_type<std::unique_ptr<Array, Deleter>,
                          std::enable_if_t<std::is_array_v<Array>>>
    : std::true_type {};

template <typename Array, typename Deleter>
struct array_like_exact_interface_type<
    std::unique_ptr<Array, Deleter>, std::enable_if_t<std::is_array_v<Array>>> {
  using type =
      typename type_traits<std::unique_ptr<Array, Deleter>>::value_type;
};

template <typename Array>
struct is_array_like_type<std::shared_ptr<Array>,
                          std::enable_if_t<std::is_array_v<Array>>>
    : std::true_type {};

template <typename Array>
struct array_like_exact_interface_type<
    std::shared_ptr<Array>, std::enable_if_t<std::is_array_v<Array>>> {
  using type = typename type_traits<std::shared_ptr<Array>>::value_type;
};
} // namespace detail
} // namespace dingo
