//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

// Metafunctions of the language, which know nothing of the library.

#include <cstddef>
#include <type_traits>

namespace dingo {
namespace detail {
// Detect completeness through overload resolution instead of a partial
// specialization. MSVC x64 accepts the specialization-based probe for some
// forward declarations that still need to be rejected during registration.
template <typename T, std::size_t = sizeof(T)>
std::true_type is_complete_impl(int);

template <typename T> std::false_type is_complete_impl(...);

template <typename T> struct is_complete : decltype(is_complete_impl<T>(0)) {};

template <typename T>
inline constexpr bool is_complete_v = is_complete<T>::value;

template <typename T>
inline constexpr bool requires_complete_type_v =
    !std::is_void_v<T> && !std::is_function_v<T>;

template <typename T, typename, auto... Values>
struct is_brace_constructible_impl : std::false_type {};

template <typename T, auto... Values>
struct is_brace_constructible_impl<T, std::void_t<decltype(T{Values...})>,
                                   Values...> : std::true_type {};

// Whether T{Values...} is well-formed.
template <typename T, auto... Values>
inline constexpr bool is_brace_constructible_v =
    is_brace_constructible_impl<T, void, Values...>::value;

// To with the const and volatile qualifiers of From.
template <typename From, typename To> struct copy_cv {
private:
  using const_type =
      std::conditional_t<std::is_const_v<From>, std::add_const_t<To>, To>;

public:
  using type = std::conditional_t<std::is_volatile_v<From>,
                                  std::add_volatile_t<const_type>, const_type>;
};

template <typename From, typename To>
using copy_cv_t = typename copy_cv<From, To>::type;

} // namespace detail
} // namespace dingo
