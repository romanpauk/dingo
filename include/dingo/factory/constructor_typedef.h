//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/core/config.h>

#include <type_traits>

namespace dingo {

// Keep typedef detection in this header lightweight so opt-in construction
// annotations can include it without pulling in the heavier construction
// backend machinery from constructor_traits.h.
template <typename T, typename = void>
struct has_constructor_typedef : std::false_type {};

template <typename T>
struct has_constructor_typedef<
    T, typename std::void_t<typename T::dingo_constructor_type>>
    : std::true_type {};

template <typename T>
static constexpr bool has_constructor_typedef_v =
    has_constructor_typedef<T>{};

namespace detail {
enum class constructor_kind { concrete, generic, invalid };

template <typename T, bool = has_constructor_typedef_v<T>>
struct constructor_typedef_impl : T::dingo_constructor_type {};

template <typename T> struct constructor_typedef_impl<T, false> {};
} // namespace detail

template <typename T>
struct constructor_typedef : detail::constructor_typedef_impl<T> {
    static constexpr detail::constructor_kind kind =
        detail::constructor_kind::concrete;
};

} // namespace dingo
