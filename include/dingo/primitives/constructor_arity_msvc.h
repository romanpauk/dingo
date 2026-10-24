//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

// The arity search of the msvc backend of constructor detection.
//
// MSVC can instantiate a recursive primary template before it selects a
// zero-arity specialization, and it does not apply brace elision while probing
// a std::array. This search therefore probes each arity with a class instead
// of a constant expression, leaves the primary undefined so that only a nonzero
// arity recurses and zero is never decremented, and takes the arity of a
// std::array from its size. It is standard C++ and is tested on every compiler.

#include <dingo/primitives/constructor_probe.h>

#include <array>
#include <cstddef>
#include <type_traits>
#include <utility>

namespace dingo::core::detail {

template <typename Type, typename = void, typename... Args>
struct msvc_list_initialization_impl : std::false_type {};

template <typename Type, typename... Args>
struct msvc_list_initialization_impl<
    Type, std::void_t<decltype(Type{std::declval<Args>()...})>, Args...>
    : std::true_type {};

template <typename Element, std::size_t Size, typename = void, typename... Args>
struct msvc_array_initialization : std::false_type {};

template <typename Element, std::size_t Size, typename... Args>
struct msvc_array_initialization<
    Element, Size,
    std::void_t<decltype(std::array<Element, Size>{{std::declval<Args>()...}})>,
    Args...> : std::true_type {};

template <typename Type, typename... Args>
struct msvc_list_initialization
    : msvc_list_initialization_impl<Type, void, Args...> {};

// A std::array is initialized with the braces of its nested array.
template <typename Element, std::size_t Size, typename... Args>
struct msvc_list_initialization<std::array<Element, Size>, Args...>
    : msvc_array_initialization<Element, Size, void, Args...> {};

// The copy constructor is never detected.
template <typename Type, typename... Args>
struct msvc_not_copy : std::true_type {};

template <typename Type, typename Argument>
struct msvc_not_copy<Type, Argument>
    : std::negation<std::is_same<std::decay_t<Argument>, Type>> {};

template <typename Type, typename... Args>
struct msvc_constructible
    : std::conjunction<msvc_list_initialization<Type, Args...>,
                       msvc_not_copy<Type, Args...>> {};

template <typename Type, typename Argument, typename Sequence>
struct msvc_probe_impl;

template <typename Type, typename Argument, std::size_t... Indices>
struct msvc_probe_impl<Type, Argument, std::index_sequence<Indices...>>
    : msvc_constructible<
          Type,
          std::conditional_t<true, Argument,
                             std::integral_constant<std::size_t, Indices>>...> {
};

// Whether `Arity` arguments of type `Argument` construct `Type`. The search
// never reaches invalid_arity and probes through msvc_arity_probe directly.
template <typename Type, typename Argument, std::size_t Arity>
using msvc_arity_probe =
    msvc_probe_impl<Type, Argument, std::make_index_sequence<Arity>>;

// MSVC may instantiate a probe from a discarded branch, so the probe of
// invalid_arity is false instead of an index sequence of that size.
template <typename Type, typename Argument, std::size_t Arity>
struct msvc_probe
    : msvc_probe_impl<Type, Argument, std::make_index_sequence<Arity>> {};

template <typename Type, typename Argument>
struct msvc_probe<Type, Argument, invalid_arity> : std::false_type {};

template <typename Type, std::size_t Arity,
          bool Match =
              msvc_arity_probe<Type, probe_argument<Type>, Arity>::value,
          typename = void>
struct msvc_arity;

template <typename Type, std::size_t Arity>
struct msvc_arity<Type, Arity, false, std::enable_if_t<(Arity > 0)>>
    : msvc_arity<Type, Arity - 1> {};

template <typename Type, std::size_t Arity>
struct msvc_arity<Type, Arity, true, std::enable_if_t<(Arity > 0)>>
    : std::integral_constant<std::size_t, Arity> {};

template <typename Type>
struct msvc_arity<Type, 0, false, void>
    : std::integral_constant<std::size_t, invalid_arity> {};

template <typename Type>
struct msvc_arity<Type, 0, true, void>
    : std::integral_constant<std::size_t, 0> {};

template <typename Type, std::size_t Arity> struct msvc_search {
  static constexpr std::size_t found = msvc_arity<Type, Arity>::value;
  static constexpr bool generic =
      found != 0 && msvc_probe<Type, opaque_argument, found>::value;
};

// A std::array takes one argument per element.
template <std::size_t Size> struct msvc_array_search {
  static constexpr std::size_t found = Size;
  static constexpr bool generic = false;
};

template <typename Type, std::size_t Arity> struct msvc_search_of {
  using type = msvc_search<Type, Arity>;
};

template <typename Element, std::size_t Size, std::size_t Arity>
struct msvc_search_of<std::array<Element, Size>, Arity> {
  using type =
      std::conditional_t<(Size <= Arity), msvc_array_search<Size>,
                         msvc_search<std::array<Element, Size>, Arity>>;
};

template <typename Type, std::size_t Arity>
struct constructor_search<Type, msvc_constructor_detection, Arity>
    : msvc_search_of<Type, Arity>::type {};

} // namespace dingo::core::detail
