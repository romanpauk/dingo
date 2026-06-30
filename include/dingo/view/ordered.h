//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/view/storage.h>

#include <functional>
#include <map>
#include <utility>

namespace dingo {
namespace detail {

template <typename Key, typename Rows, typename Allocator>
class ordered_lookup_storage {
  using value_type = std::pair<const Key, Rows>;
  using storage_allocator = lookup_storage_allocator_t<value_type, Allocator>;
  using storage_type = std::map<Key, Rows, std::less<Key>, storage_allocator>;

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

  template <typename... Args> auto try_emplace(const Key &key, Args &&...args) {
    return values_.try_emplace(key, std::forward<Args>(args)...);
  }

  iterator erase(iterator it) { return values_.erase(it); }

private:
  storage_type values_;
};

} // namespace detail

struct ordered {
  template <typename Key, typename Rows, typename Allocator>
  using storage = detail::ordered_lookup_storage<Key, Rows, Allocator>;
};

} // namespace dingo
