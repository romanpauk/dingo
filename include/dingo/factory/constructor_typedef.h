//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/config.h>

#include <type_traits>

namespace dingo {

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
template <typename T, bool = has_constructor_typedef_v<T>>
struct constructor_typedef_impl : T::dingo_constructor_type {};

template <typename T>
struct constructor_typedef_impl<T, false> {
    //static_assert(false, "constructor typedef not detected");
};

}

template <typename T>
struct constructor_typedef : detail::constructor_typedef_impl<T> {};

}



