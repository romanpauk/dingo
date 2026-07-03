//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/core/key.h>
#include <dingo/core/selected.h>

#include <type_traits>

namespace dingo::detail {
template <typename Selector> struct dependency_selector {
  using type = Selector;
};

template <typename T> struct dependency_selector<key_type<T>> {
  using type = type_selector<T>;
};

template <typename Selector>
using dependency_selector_t = typename dependency_selector<Selector>::type;
} // namespace dingo::detail

namespace dingo {
template <typename T, typename Selector = void>
using dependency = std::conditional_t<
    std::is_void_v<Selector>, T,
    detail::selected<T, detail::dependency_selector_t<Selector>>>;
} // namespace dingo
