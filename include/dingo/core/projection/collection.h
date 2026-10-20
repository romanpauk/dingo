//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

// The public extension point that makes a type a collection, which a
// collection request fills with every binding it names, each resolved as the
// element type. The standard containers specialize it here, as a user
// collection does.

#include <cstddef>
#include <list>
#include <map>
#include <set>
#include <type_traits>
#include <utility>
#include <vector>

namespace dingo {

namespace detail {
template <class Type> struct collection_traits {
  static const bool is_collection = false;
};

template <class Type, class Allocator>
struct collection_traits<std::vector<Type, Allocator>> {
  static const bool is_collection = true;
  using resolve_type = Type;
  static void reserve(std::vector<Type, Allocator> &collection,
                      std::size_t size) {
    collection.reserve(size);
  }
  template <typename Value>
  static void add(std::vector<Type, Allocator> &collection, Value &&value) {
    collection.emplace_back(std::forward<Value>(value));
  }
};

template <class Type, class Allocator>
struct collection_traits<std::list<Type, Allocator>> {
  static const bool is_collection = true;
  using resolve_type = Type;
  static void reserve(std::list<Type, Allocator> & /*collection*/,
                      std::size_t /*size*/) {}
  template <typename Value>
  static void add(std::list<Type, Allocator> &collection, Value &&value) {
    collection.emplace_back(std::forward<Value>(value));
  }
};

template <class Type, class Compare, class Allocator>
struct collection_traits<std::set<Type, Compare, Allocator>> {
  static const bool is_collection = true;
  using resolve_type = Type;
  static void reserve(std::set<Type, Compare, Allocator> & /*collection*/,
                      std::size_t /*size*/) {}
  template <typename Value>
  static void add(std::set<Type, Compare, Allocator> &collection,
                  Value &&value) {
    collection.emplace(std::forward<Value>(value));
  }
};

template <class Key, class Value, class Compare, class Allocator>
struct collection_traits<std::map<Key, Value, Compare, Allocator>> {
  static const bool is_collection = true;
  using resolve_type = Value;
  static void reserve(std::map<Key, Value, Compare, Allocator> & /*collection*/,
                      std::size_t /*size*/) {}
  template <typename Element>
  static void add(std::map<Key, Value, Compare, Allocator> &collection,
                  Element &&value) {
    collection.emplace(std::forward<Element>(value));
  }
};
} // namespace detail

template <class Type>
struct collection_traits
    : detail::collection_traits<
          std::remove_cv_t<std::remove_reference_t<Type>>> {};

} // namespace dingo

namespace dingo::core {

// Adds an element to a collection by its collection traits.
struct collection_append {
  template <typename Collection, typename Element>
  void operator()(Collection &collection, Element &&element) const {
    ::dingo::collection_traits<Collection>::add(collection,
                                                std::forward<Element>(element));
  }
};

} // namespace dingo::core
