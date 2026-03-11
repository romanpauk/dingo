//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/config.h>

#include <dingo/annotated.h>
#include <dingo/wrapper_traits.h>

#include <memory>
#include <optional>
#include <type_traits>

namespace dingo {
template <class T> struct decay;

namespace detail {
template <class T, class Element = wrapper_element_t<T>,
          bool Unwrap = !std::is_same_v<wrapper_base_t<T>, Element>>
struct decay_impl {
    using type = wrapper_base_t<T>;
};

template <class T, class Element>
struct decay_impl<T, Element, true> {
    using type = typename decay<Element>::type;
};
} // namespace detail

// TODO: this has misleading name
template <class T> struct decay {
    using type = typename detail::decay_impl<T>::type;
};

template <class T, class Tag>
struct decay<annotated<T, Tag>>
    : std::decay<annotated<typename decay<T>::type, Tag>> {};

template <class T> using decay_t = typename decay<T>::type;
} // namespace dingo
