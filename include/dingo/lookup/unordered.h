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
#include <type_traits>
#include <unordered_map>
#include <utility>

namespace dingo {
namespace detail {

template <typename Key, typename Value, typename Hash, typename Equal,
          typename StorageAllocator, typename Allocator>
struct lookup_storage_factory<
    std::unordered_map<Key, Value, Hash, Equal, StorageAllocator>, Allocator> {
  using storage_type =
      std::unordered_map<Key, Value, Hash, Equal, StorageAllocator>;

  static storage_type make(Allocator &allocator) {
    return storage_type(
        0, Hash{}, Equal{},
        make_lookup_storage_allocator<StorageAllocator>(allocator));
  }
};

template <typename Key, typename Value, typename Hash, typename Equal,
          typename StorageAllocator, typename Allocator>
struct lookup_storage_factory<
    std::unordered_multimap<Key, Value, Hash, Equal, StorageAllocator>,
    Allocator> {
  using storage_type =
      std::unordered_multimap<Key, Value, Hash, Equal, StorageAllocator>;

  static storage_type make(Allocator &allocator) {
    return storage_type(
        0, Hash{}, Equal{},
        make_lookup_storage_allocator<StorageAllocator>(allocator));
  }
};

} // namespace detail

struct unordered {
  template <typename Key, typename Value, typename Cardinality,
            typename Allocator>
  using storage = std::conditional_t<
      std::is_same_v<Cardinality, one>,
      std::unordered_map<Key, Value, std::hash<Key>, std::equal_to<Key>,
                         detail::lookup_storage_allocator_t<
                             std::pair<const Key, Value>, Allocator>>,
      std::unordered_multimap<Key, Value, std::hash<Key>, std::equal_to<Key>,
                              detail::lookup_storage_allocator_t<
                                  std::pair<const Key, Value>, Allocator>>>;
};

} // namespace dingo
