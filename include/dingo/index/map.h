//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/core/config.h>

#include <dingo/index/index.h>

#include <map>

namespace dingo {
namespace index_type {
using map = associative<std::map>;
} // namespace index_type

namespace detail {
template <template <typename...> typename Container, typename Key,
          typename Value, typename Allocator>
struct associative_index_storage_type {
  using allocator_type = typename std::allocator_traits<
      Allocator>::template rebind_alloc<std::pair<const Key, Value>>;
  using type = Container<Key, Value, std::less<Key>, allocator_type>;
};

template <template <typename...> typename Container, typename Key,
          typename Value, typename Allocator>
struct index_storage<index_type::associative<Container>, Key, Value,
                     Allocator> {
  using map_type =
      typename associative_index_storage_type<Container, Key, Value,
                                              Allocator>::type;
  using iterator = typename map_type::iterator;

  explicit index_storage(Allocator &allocator) : map_(allocator) {}

  std::pair<iterator, bool> emplace(Key key, Value value) {
    return map_.emplace(std::move(key), std::move(value));
  }

  iterator find(const Key &key) { return map_.find(key); }

  iterator end() { return map_.end(); }

private:
  map_type map_;
};
} // namespace detail
} // namespace dingo
