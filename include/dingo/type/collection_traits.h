//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/base/config.h>

#include <dingo/type/shape.h>

#include <list>
#include <map>
#include <set>
#include <vector>

namespace dingo {
namespace detail {
template <class T> struct collection_traits {
  static const bool is_collection = false;
  static const bool has_fixed_size_construct = false;
};

// A collection that grows by add and reserves nothing.
template <class Collection, class Element> struct growing_collection_traits {
  static const bool is_collection = true;
  static const bool has_fixed_size_construct = false;
  using resolve_type = Element;
  static void reserve(Collection &, size_t) {}
};

template <class T, class Allocator>
struct collection_traits<std::vector<T, Allocator>> {
  static const bool is_collection = true;
  static const bool has_fixed_size_construct = true;
  using resolve_type = T;
  static void reserve(std::vector<T, Allocator> &collection, size_t size) {
    collection.reserve(size);
  }
  static std::vector<T, Allocator> make_fixed_size(size_t size) {
    return std::vector<T, Allocator>(size);
  }
  template <typename U>
  static void add(std::vector<T, Allocator> &collection, U &&value) {
    collection.emplace_back(std::forward<U>(value));
  }
  template <typename U>
  static void set(std::vector<T, Allocator> &collection, size_t index,
                  U &&value) {
    collection[index] = std::forward<U>(value);
  }
};

template <class T, class Allocator>
struct collection_traits<std::list<T, Allocator>>
    : growing_collection_traits<std::list<T, Allocator>, T> {
  template <typename U>
  static void add(std::list<T, Allocator> &collection, U &&value) {
    collection.emplace_back(std::forward<U>(value));
  }
};

template <class T, class Compare, class Allocator>
struct collection_traits<std::set<T, Compare, Allocator>>
    : growing_collection_traits<std::set<T, Compare, Allocator>, T> {
  template <typename U>
  static void add(std::set<T, Compare, Allocator> &collection, U &&value) {
    collection.emplace(std::forward<U>(value));
  }
};

template <class Key, class Value, class Compare, class Allocator>
struct collection_traits<std::map<Key, Value, Compare, Allocator>>
    : growing_collection_traits<std::map<Key, Value, Compare, Allocator>,
                                Value> {
  template <typename U>
  static void add(std::map<Key, Value, Compare, Allocator> &collection,
                  U &&value) {
    collection.emplace(std::forward<U>(value));
  }
};

} // namespace detail

template <class T>
struct collection_traits : detail::collection_traits<normalized_type_t<T>> {};
} // namespace dingo
