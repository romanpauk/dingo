//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/base/config.h>
#include <dingo/base/selected.h>

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
  // The value the maker makes, made in place.
  template <typename Maker,
            std::enable_if_t<detail::is_selected_maker_v<std::decay_t<Maker>>,
                             int> = 0>
  explicit annotated(Maker &&maker) : base_type(std::forward<Maker>(maker)) {}
};

template <typename T, typename Tag>
struct annotated<T &, Tag> : detail::selected<T &, detail::type_selector<Tag>> {
  annotated(T &value)
      : detail::selected<T &, detail::type_selector<Tag>>(value) {}
};

// A request or an interface without its annotation and its key selection,
// also behind a reference or a pointer.
template <typename T> struct unannotated {
  using type = T;
};

template <typename T> struct unannotated<T &> {
  using type = typename unannotated<T>::type &;
};

template <typename T> struct unannotated<T &&> {
  using type = typename unannotated<T>::type &&;
};

template <typename T> struct unannotated<T *> {
  using type = typename unannotated<T>::type *;
};

template <typename T, typename Tag>
struct unannotated<annotated<T, Tag>> : unannotated<T> {};

template <typename T, typename Selector>
struct unannotated<detail::selected<T, Selector>> : unannotated<T> {};

template <typename T> using unannotated_t = typename unannotated<T>::type;

} // namespace dingo
