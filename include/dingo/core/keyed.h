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
template <typename T, auto... Values> struct key;

namespace detail {
template <typename T, auto Value, typename = void>
struct is_key_value_brace_constructible : std::false_type {};

template <typename T, auto Value>
struct is_key_value_brace_constructible<T, Value,
                                        std::void_t<decltype(T{Value})>>
    : std::true_type {};

template <typename T, auto Value>
inline constexpr bool is_key_value_brace_constructible_v =
    is_key_value_brace_constructible<T, Value>::value;

template <typename T, auto... Values> struct key_value {
  static_assert(sizeof...(Values) <= 1,
                "dingo::key<T, V> accepts at most one value");
};

template <typename T, auto Value> struct key_value<T, Value> {
  static_assert(is_key_value_brace_constructible_v<T, Value>,
                "dingo::key<T, V> requires V to be usable as T{V}");

  static T make() { return T{Value}; }
};

template <typename T, auto... Values>
struct is_key_value_usable : std::false_type {};

template <typename T> struct is_key_value_usable<T> : std::true_type {};

template <typename T, auto Value>
struct is_key_value_usable<T, Value>
    : is_key_value_brace_constructible<T, Value> {};

template <typename T, auto... Values>
inline constexpr bool is_key_value_usable_v =
    is_key_value_usable<T, Values...>::value;

} // namespace detail

template <typename T, typename Key>
struct keyed : detail::selected<T, detail::type_selector<Key>> {
  using detail::selected<T, detail::type_selector<Key>>::selected;
};

template <typename T, typename Key>
struct keyed<T &, Key> : detail::selected<T &, detail::type_selector<Key>> {
  using detail::selected<T &, detail::type_selector<Key>>::selected;
};

template <typename T, typename Selector>
struct indexed : detail::selected<T, Selector> {
  using detail::selected<T, Selector>::selected;
};

template <typename T, typename Selector>
struct indexed<T &, Selector> : detail::selected<T &, Selector> {
  using detail::selected<T &, Selector>::selected;
};

template <typename T> struct keyed_traits {
  using type = T;
  using key_type = void;
};

template <typename T, typename Key> struct keyed_traits<keyed<T, Key>> {
  using type = T;
  using key_type = Key;
};

template <typename T, typename Key> struct keyed_traits<keyed<T, Key> &> {
  using type = T &;
  using key_type = Key;
};

template <typename T, typename Key> struct keyed_traits<keyed<T, Key> &&> {
  using type = T &&;
  using key_type = Key;
};

template <typename T, typename Key> struct keyed_traits<keyed<T, Key> *> {
  using type = T *;
  using key_type = Key;
};

template <typename T> using keyed_type_t = typename keyed_traits<T>::type;

template <typename T> using keyed_key_t = typename keyed_traits<T>::key_type;

template <typename T>
struct is_keyed : std::bool_constant<!std::is_void_v<keyed_key_t<T>>> {};

template <typename T> inline constexpr bool is_keyed_v = is_keyed<T>::value;

template <typename T> struct indexed_traits {
  using type = T;
  using selector_type = void;
};

template <typename T, typename Selector>
struct indexed_traits<indexed<T, Selector>> {
  using type = T;
  using selector_type = Selector;
};

template <typename T, typename Selector>
struct indexed_traits<indexed<T, Selector> &> {
  using type = T &;
  using selector_type = Selector;
};

template <typename T, typename Selector>
struct indexed_traits<indexed<T, Selector> &&> {
  using type = T &&;
  using selector_type = Selector;
};

template <typename T, typename Selector>
struct indexed_traits<indexed<T, Selector> *> {
  using type = T *;
  using selector_type = Selector;
};

template <typename T> using indexed_type_t = typename indexed_traits<T>::type;

template <typename T>
using indexed_selector_t = typename indexed_traits<T>::selector_type;

template <typename T>
struct is_indexed : std::bool_constant<!std::is_void_v<indexed_selector_t<T>>> {
};

template <typename T> inline constexpr bool is_indexed_v = is_indexed<T>::value;

namespace detail {

template <typename T> struct is_typed_key : std::false_type {};

template <typename T, auto... Values>
struct is_typed_key<key<T, Values...>>
    : std::bool_constant<sizeof...(Values) == 0 && !std::is_void_v<T>> {};

template <typename T>
inline constexpr bool is_typed_key_v = is_typed_key<std::decay_t<T>>::value;

template <typename T> struct is_key_value : std::false_type {};

template <typename T, auto Value>
struct is_key_value<key<T, Value>> : std::bool_constant<!std::is_void_v<T>> {};

template <typename T>
inline constexpr bool is_key_value_v = is_key_value<std::decay_t<T>>::value;

template <typename Selector> struct key_value_traits;

template <typename T, auto Value> struct key_value_traits<key<T, Value>> {
  using type = T;

  static T make() { return key_value<T, Value>::make(); }
};

template <typename Left, typename Right>
struct is_same_key_value : std::false_type {};

template <typename T, auto Left, auto Right>
struct is_same_key_value<key<T, Left>, key<T, Right>>
    : std::bool_constant<(T{Left} == T{Right})> {};

template <typename T, auto Value>
struct is_value_selector<key<T, Value>> : std::true_type {
  using type = T;
  static T make() { return key_value<T, Value>::make(); }
};

template <typename Identity, typename Binding>
struct keyed_binding_identity : Binding {
  using Binding::Binding;
};

} // namespace detail

} // namespace dingo
