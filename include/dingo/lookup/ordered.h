//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/lookup/storage.h>
#include <dingo/lookup/tags.h>

#include <functional>
#include <map>
#include <utility>

namespace dingo {
namespace detail {

template <typename Key, typename Mapped, typename Cardinality,
          typename Allocator>
class ordered_lookup_storage;

template <typename Key, typename Mapped, typename Allocator>
class ordered_lookup_storage<Key, Mapped, ::dingo::one, Allocator> {
  using value_type = std::pair<const Key, Mapped>;
  using storage_allocator = lookup_storage_allocator_t<value_type, Allocator>;
  using storage_type = std::map<Key, Mapped, std::less<Key>, storage_allocator>;

public:
  using iterator = typename storage_type::iterator;
  using const_iterator = typename storage_type::const_iterator;

  explicit ordered_lookup_storage(Allocator &allocator)
      : values_(std::less<Key>{},
                make_lookup_storage_allocator<storage_allocator>(allocator)) {}

  iterator find(const Key &key) { return values_.find(key); }
  const_iterator find(const Key &key) const { return values_.find(key); }
  iterator end() { return values_.end(); }
  const_iterator end() const { return values_.end(); }

  template <typename Value>
  std::pair<iterator, bool> try_emplace(const Key &key, Value &&value) {
    return values_.try_emplace(key, std::forward<Value>(value));
  }

  void erase(iterator handle) { values_.erase(handle); }

private:
  storage_type values_;
};

template <typename Key, typename Mapped, typename Allocator>
class ordered_lookup_storage<Key, Mapped, ::dingo::many, Allocator> {
  using value_type = std::pair<const Key, Mapped>;
  using storage_allocator = lookup_storage_allocator_t<value_type, Allocator>;
  using storage_type =
      std::multimap<Key, Mapped, std::less<Key>, storage_allocator>;

public:
  using iterator = typename storage_type::iterator;
  using const_iterator = typename storage_type::const_iterator;

  explicit ordered_lookup_storage(Allocator &allocator)
      : values_(std::less<Key>{},
                make_lookup_storage_allocator<storage_allocator>(allocator)) {}

  iterator find(const Key &key) { return values_.find(key); }
  const_iterator find(const Key &key) const { return values_.find(key); }
  iterator end() { return values_.end(); }
  const_iterator end() const { return values_.end(); }

  std::pair<iterator, iterator> equal_range(const Key &key) {
    return values_.equal_range(key);
  }

  std::pair<const_iterator, const_iterator> equal_range(const Key &key) const {
    return values_.equal_range(key);
  }

  template <typename Value> iterator emplace(const Key &key, Value &&value) {
    return values_.emplace(key, std::forward<Value>(value));
  }

  void erase(iterator handle) { values_.erase(handle); }

private:
  storage_type values_;
};

} // namespace detail

struct ordered {
  template <typename Key, typename Mapped, typename Cardinality,
            typename Allocator>
  using storage =
      detail::ordered_lookup_storage<Key, Mapped, Cardinality, Allocator>;
};

} // namespace dingo
