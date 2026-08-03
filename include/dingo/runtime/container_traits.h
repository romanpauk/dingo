//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/detail/container_traits.h>
#include <dingo/rtti/typeid_provider.h>

#include <memory>
#include <tuple>
#include <type_traits>

namespace dingo {

struct dynamic_container_traits {
  using rtti_type = rtti<typeid_provider>;
  using allocator_type = std::allocator<char>;
  using lookup_definition_type = std::tuple<>;
};

namespace detail {

template <typename T, typename = void>
struct is_runtime_container_traits : std::false_type {};

template <typename T>
struct is_runtime_container_traits<
    T, std::void_t<typename T::rtti_type, typename T::allocator_type,
                   container_lookup_definition_type_t<T>>> : std::true_type {};

template <typename T>
inline constexpr bool is_runtime_container_traits_v =
    is_runtime_container_traits<T>::value;

} // namespace detail
} // namespace dingo
