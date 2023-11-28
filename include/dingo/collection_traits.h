//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/config.h>

#include <dingo/decay.h>

#include <list>
#include <map>
#include <set>
#include <vector>

namespace dingo {
namespace detail {
template <class T> struct collection_traits {
    static const bool is_collection = false;
};

template <class T, class Allocator>
struct collection_traits<std::vector<T, Allocator>> {
    static const bool is_collection = true;
    using resolve_type = T;
    static void reserve(std::vector<T, Allocator>& collection, size_t size) {
        collection.reserve(size);
    }
    template <typename U>
    static void add(std::vector<T, Allocator>& collection, U&& value) {
        collection.emplace_back(std::forward<U>(value));
    }
};

template <class T, class Allocator>
struct collection_traits<std::list<T, Allocator>> {
    static const bool is_collection = true;
    using resolve_type = T;
    static void reserve(std::list<T, Allocator>&, size_t) {}
    template <typename U>
    static void add(std::list<T, Allocator>& collection, U&& value) {
        collection.emplace_back(std::forward<U>(value));
    }
};

template <class T, class Compare, class Allocator>
struct collection_traits<std::set<T, Compare, Allocator>> {
    static const bool is_collection = true;
    using resolve_type = T;
    static void reserve(std::set<T, Compare, Allocator>&, size_t) {}
    template <typename U>
    static void add(std::set<T, Compare, Allocator>& collection, U&& value) {
        collection.emplace(std::forward<U>(value));
    }
};

template <class Key, class Value, class Compare, class Allocator>
struct collection_traits<std::map<Key, Value, Compare, Allocator>> {
    static const bool is_collection = true;
    using resolve_type = Value;
    static void reserve(std::map<Key, Value, Compare, Allocator>&, size_t) {}
    template <typename U>
    static void add(std::map<Key, Value, Compare, Allocator>& collection,
                    U&& value) {
        collection.emplace(std::forward<U>(value));
    }
};

} // namespace detail

template <class T>
struct collection_traits : detail::collection_traits<decay_t<T>> {};
} // namespace dingo
