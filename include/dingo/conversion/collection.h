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
// collection does; they add only an element their objects are constructed
// from.

#include <dingo/primitives/constructor.h>

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
  template <typename Value,
            std::enable_if_t<std::is_constructible_v<Type, Value>, int> = 0>
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
  template <typename Value,
            std::enable_if_t<std::is_constructible_v<Type, Value>, int> = 0>
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
  template <typename Value,
            std::enable_if_t<std::is_constructible_v<Type, Value>, int> = 0>
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
  template <
      typename Element,
      std::enable_if_t<
          std::is_constructible_v<
              typename std::map<Key, Value, Compare, Allocator>::value_type,
              Element>,
          int> = 0>
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
template <typename Collection, typename = void>
inline constexpr bool has_reserve_v = false;

template <typename Collection>
inline constexpr bool has_reserve_v<
    Collection,
    std::void_t<decltype(::dingo::collection_traits<Collection>::reserve(
        std::declval<Collection &>(), std::size_t()))>> = true;

// Reserves room for `size` elements in `collection` by its collection
// traits, when they reserve.
template <typename Collection>
void reserve_collection(Collection &collection, std::size_t size) {
  if constexpr (has_reserve_v<Collection>) {
    ::dingo::collection_traits<Collection>::reserve(collection, size);
  }
}

// Whether the collection traits of `Collection` add an element of its
// resolve type, which a collection of the bindings of the element type is
// filled with.
template <typename Collection, typename = void>
inline constexpr bool appends_elements_v = false;

template <typename Collection>
inline constexpr bool appends_elements_v<
    Collection,
    std::void_t<decltype(::dingo::collection_traits<Collection>::add(
        std::declval<Collection &>(),
        std::declval<typename ::dingo::collection_traits<
            Collection>::resolve_type>()))>> = true;

// Whether an object of `Type` is copied. A collection copies its elements, so
// it is copied when its elements of its resolve type are, which the copy
// constructor of a standard container does not tell; any other type is
// copied as std::is_copy_constructible tells. An element incomplete where it
// is asked is not asked.
template <typename Type, typename = void>
struct is_copied : std::is_copy_constructible<Type> {};

template <typename Type>
struct is_copied<
    Type, std::enable_if_t<::dingo::collection_traits<Type>::is_collection>> {
  using element = typename ::dingo::collection_traits<Type>::resolve_type;
  static constexpr bool value =
      std::is_copy_constructible_v<Type> &&
      (!detail::is_complete<element>::value || is_copied<element>::value);
};

template <typename Type>
inline constexpr bool is_copied_v = is_copied<Type>::value;

struct collection_append {
  template <typename Collection, typename Element>
  void operator()(Collection &collection, Element &&element) const {
    ::dingo::collection_traits<Collection>::add(collection,
                                                std::forward<Element>(element));
  }
};

} // namespace dingo::core
