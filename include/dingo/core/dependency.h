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
template <typename T, typename Selector> struct dependency_type {
  using type = detail::selected<T, detail::dependency_selector_t<Selector>>;
};

template <typename T> struct dependency_type<T, key_type<none_t>> {
  using type = T;
};

template <typename T, typename Selector = key_type<none_t>>
using dependency = typename dependency_type<T, std::decay_t<Selector>>::type;
} // namespace dingo
