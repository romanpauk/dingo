//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/core/config.h>
#include <dingo/core/selected.h>

#include <type_traits>
#include <utility>

namespace dingo {

template <typename T, typename Tag>
struct annotated : detail::selected<T, detail::type_selector<Tag>> {
  using base_type = detail::selected<T, detail::type_selector<Tag>>;

  template <
      typename U = T,
      std::enable_if_t<std::is_constructible_v<base_type, const U &>, int> = 0>
  annotated(const T &value) : base_type(value) {}
  template <typename U = T,
            std::enable_if_t<std::is_constructible_v<base_type, U &&>, int> = 0>
  annotated(T &&value) : base_type(std::move(value)) {}
};

template <typename T, typename Tag>
struct annotated<T &, Tag> : detail::selected<T &, detail::type_selector<Tag>> {
  annotated(T &value)
      : detail::selected<T &, detail::type_selector<Tag>>(value) {}
};

template <typename T> struct annotated_traits {
  using type = T;
};

template <typename T, typename Tag> struct annotated_traits<annotated<T, Tag>> {
  using type = T;
};

template <typename T, typename Tag>
struct annotated_traits<annotated<T, Tag> &> {
  using type = T &;
};

template <typename T, typename Tag>
struct annotated_traits<annotated<T, Tag> &&> {
  using type = T &&;
};

template <typename T, typename Tag>
struct annotated_traits<annotated<T, Tag> *> {
  using type = T *;
};

} // namespace dingo
