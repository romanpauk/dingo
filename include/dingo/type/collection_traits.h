//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/base/config.h>

#include <dingo/type/shape.h>

#include <cstddef>
#include <type_traits>
#include <utility>

namespace dingo {
namespace detail {
// A collection is recognised by the members of the standard containers, not
// by its name: a sequence appends with emplace_back, an associative
// collection adds with emplace and has a key_type. Each resolves its
// mapped_type, or its value_type. A sequence that reserves, is made of its
// size and is indexed is made at its size and set by index.
template <class T, class = void> struct has_emplace_back : std::false_type {};
template <class T>
struct has_emplace_back<
    T, std::void_t<typename T::value_type,
                   decltype(std::declval<T &>().emplace_back(
                       std::declval<typename T::value_type &&>()))>>
    : std::true_type {};

template <class T, class = void> struct has_key_emplace : std::false_type {};
template <class T>
struct has_key_emplace<
    T, std::void_t<typename T::key_type, typename T::value_type,
                   decltype(std::declval<T &>().emplace(
                       std::declval<typename T::value_type &&>()))>>
    : std::true_type {};

template <class T, class = void> struct is_sized_sequence : std::false_type {};
template <class T>
struct is_sized_sequence<
    T, std::void_t<decltype(std::declval<T &>().reserve(std::size_t())),
                   decltype(T(std::size_t())),
                   decltype(std::declval<T &>()[std::size_t()])>>
    : std::true_type {};

template <class T, class = void> struct collection_element {
  using type = typename T::value_type;
};
template <class T>
struct collection_element<T, std::void_t<typename T::mapped_type>> {
  using type = typename T::mapped_type;
};

template <class T, class = void> struct collection_traits {
  static const bool is_collection = false;
  static const bool has_fixed_size_construct = false;
};

template <class T>
struct collection_traits<T, std::enable_if_t<has_emplace_back<T>::value ||
                                             has_key_emplace<T>::value>> {
  static const bool is_collection = true;
  static const bool has_fixed_size_construct =
      has_emplace_back<T>::value && is_sized_sequence<T>::value;
  using resolve_type = typename collection_element<T>::type;

  static void reserve([[maybe_unused]] T &collection,
                      [[maybe_unused]] std::size_t size) {
    if constexpr (has_fixed_size_construct) {
      collection.reserve(size);
    }
  }
  static T make_fixed_size(std::size_t size) { return T(size); }
  template <typename U> static void add(T &collection, U &&value) {
    if constexpr (has_emplace_back<T>::value) {
      collection.emplace_back(std::forward<U>(value));
    } else {
      collection.emplace(std::forward<U>(value));
    }
  }
  template <typename U>
  static void set(T &collection, std::size_t index, U &&value) {
    collection[index] = std::forward<U>(value);
  }
};

} // namespace detail

template <class T>
struct collection_traits : detail::collection_traits<normalized_type_t<T>> {};
} // namespace dingo
