//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

// The arity search of the portable backend of constructor detection. One
// constant expression probes each arity with repeated probe arguments, from the
// largest arity down, so the constructor with the most parameters wins, and the
// copy constructor is never detected.

#include <dingo/constructor/constructor_probe.h>

#include <cstddef>
#include <type_traits>
#include <utility>

namespace dingo::core::detail {

template <typename Type, typename... Args>
using list_initialization_t = decltype(Type{std::declval<Args>()...});

template <typename Type, typename = void, typename... Args>
struct is_list_initializable_impl : std::false_type {};

template <typename Type, typename... Args>
struct is_list_initializable_impl<
    Type, std::void_t<list_initialization_t<Type, Args...>>, Args...>
    : std::bool_constant<sizeof...(Args) != 1 ||
                         (!std::is_same_v<Type, std::decay_t<Args>> && ...)> {};

template <typename Type, typename... Args>
inline constexpr bool is_list_initializable_v =
    is_list_initializable_impl<Type, void, Args...>::value;

template <typename Type, typename Argument, std::size_t... Indices>
constexpr bool constructible_with(std::index_sequence<Indices...>) {
  return is_list_initializable_v<Type, repeated<Argument, Indices>...>;
}

// The largest arity in [0, Arity] that constructs `Type`.
template <typename Type, std::size_t Arity> constexpr std::size_t find_arity() {
  if constexpr (constructible_with<Type, probe_argument<Type>>(
                    std::make_index_sequence<Arity>{})) {
    return Arity;
  } else if constexpr (Arity == 0) {
    return invalid_arity;
  } else {
    return find_arity<Type, Arity - 1>();
  }
}

// Whether the constructor of arity `Found` accepts any argument.
template <typename Type, std::size_t Found> constexpr bool is_generic() {
  if constexpr (Found == invalid_arity || Found == 0) {
    return false;
  } else {
    return constructible_with<Type, opaque_argument>(
        std::make_index_sequence<Found>{});
  }
}

template <typename Type, std::size_t Arity>
struct constructor_search<Type, portable_constructor_detection, Arity> {
  static constexpr std::size_t found = find_arity<Type, Arity>();
  static constexpr bool generic = is_generic<Type, found>();
};

} // namespace dingo::core::detail
