//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/core/config.h>

#include <type_traits>
#include <utility>

namespace dingo {
namespace detail {

template <typename T, typename Selector> struct selected;
template <typename Tag> struct type_selector {};
template <typename Key, auto Value> struct value_selector {};

template <typename T, typename Selector,
          bool IsConstructible = std::is_constructible_v<T>>
struct selected_base;

template <typename T, typename Selector>
struct selected_base<T, Selector, true> {
  selected_base(const selected_base &) = default;
  selected_base(selected_base &&) = default;
  selected_base &operator=(const selected_base &) = default;
  selected_base &operator=(selected_base &&) = default;

  explicit selected_base(T &&value) : value_(std::move(value)) {}

  operator T() { return std::move(value_); }

private:
  T value_;
};

template <typename T, typename Selector>
struct selected_base<T &, Selector, false> {
  selected_base(const selected_base &) = default;
  selected_base(selected_base &&) = default;
  selected_base &operator=(const selected_base &) = default;
  selected_base &operator=(selected_base &&) = default;

  explicit selected_base(T &value) : value_(value) {}

  operator T &() { return value_; }

private:
  T &value_;
};

template <typename T, typename Selector>
struct selected_base<T, Selector, false> : selected_base<T &, Selector, false> {
  explicit selected_base(T &value)
      : selected_base<T &, Selector, false>(value) {}
};

template <typename T, typename Selector>
struct selected : selected_base<T, Selector> {
  using selected_base<T, Selector>::selected_base;
};

template <typename T, typename Selector>
struct selected<T &, Selector> : selected_base<T &, Selector> {
  using selected_base<T &, Selector>::selected_base;
};

template <typename T> struct selected_traits {
  using type = T;
  using selector_type = void;
};

template <typename T, typename Selector>
struct selected_traits<selected<T, Selector>> {
  using type = T;
  using selector_type = Selector;
};

template <typename T, typename Selector>
struct selected_traits<selected<T, Selector> &> {
  using type = T &;
  using selector_type = Selector;
};

template <typename T, typename Selector>
struct selected_traits<selected<T, Selector> &&> {
  using type = T &&;
  using selector_type = Selector;
};

template <typename T, typename Selector>
struct selected_traits<selected<T, Selector> *> {
  using type = T *;
  using selector_type = Selector;
};

template <typename T> using selected_type_t = typename selected_traits<T>::type;

template <typename T>
using selected_selector_t = typename selected_traits<T>::selector_type;

template <typename T>
struct is_selected
    : std::bool_constant<!std::is_void_v<selected_selector_t<T>>> {};

template <typename T>
inline constexpr bool is_selected_v = is_selected<T>::value;

template <typename Selector> struct is_type_selector : std::false_type {};

template <typename Tag>
struct is_type_selector<type_selector<Tag>> : std::true_type {
  using type = Tag;
};

template <typename Selector>
inline constexpr bool is_type_selector_v =
    is_type_selector<std::decay_t<Selector>>::value;

template <typename Selector>
using type_selector_type_t =
    typename is_type_selector<std::decay_t<Selector>>::type;

template <typename Selector> struct is_value_selector : std::false_type {};

template <typename Key, auto Value>
struct is_value_selector<value_selector<Key, Value>> : std::true_type {
  using type = Key;
  static Key make() { return Key{Value}; }
};

template <typename Selector>
inline constexpr bool is_value_selector_v =
    is_value_selector<std::decay_t<Selector>>::value;

template <typename Selector>
using value_selector_type_t =
    typename is_value_selector<std::decay_t<Selector>>::type;
} // namespace detail

} // namespace dingo
