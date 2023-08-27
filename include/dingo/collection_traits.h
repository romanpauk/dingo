#pragma once

#include <dingo/decay.h>

#include <list>
#include <set>
#include <vector>

namespace dingo {
namespace detail {
template <class T> struct collection_traits {
    static const bool is_collection = false;
};

template <class T, class Allocator> struct collection_traits<std::vector<T, Allocator>> {
    static const bool is_collection = true;
    static void reserve(std::vector<T, Allocator>& v, size_t size) { v.reserve(size); }
    template <typename U> static void add(std::vector<T, Allocator>& v, U&& value) {
        v.emplace_back(std::forward<U>(value));
    }
};

template <class T, class Allocator> struct collection_traits<std::list<T, Allocator>> {
    static const bool is_collection = true;
    static void reserve(std::list<T, Allocator>& v, size_t size) {}
    template <typename U> static void add(std::list<T, Allocator>& v, U&& value) {
        v.emplace_back(std::forward<U>(value));
    }
};

template <class T, class Compare, class Allocator> struct collection_traits<std::set<T, Compare, Allocator>> {
    static const bool is_collection = true;
    static void reserve(std::set<T, Compare, Allocator>& v, size_t size) {}
    template <typename U> static void add(std::set<T, Compare, Allocator>& v, U&& value) {
        v.emplace(std::forward<U>(value));
    }
};
} // namespace detail

template <class T> struct collection_traits : detail::collection_traits<decay_t<T>> {};
} // namespace dingo
