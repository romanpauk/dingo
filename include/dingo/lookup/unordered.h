//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/lookup/base.h>
#include <dingo/lookup/storage.h>
#include <dingo/lookup/tags.h>

#include <functional>
#include <type_traits>
#include <unordered_map>
#include <utility>

namespace dingo {
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

// The hash of the key of a base lookup, which an unordered backend keeps.
namespace std {
template <typename Rtti> struct hash<dingo::detail::base_lookup_key<Rtti>> {
  std::size_t
  operator()(const dingo::detail::base_lookup_key<Rtti> &key) const {
    using lookup_type_index =
        typename dingo::detail::base_lookup_key<Rtti>::type_index;
    const auto interface_hash =
        std::hash<lookup_type_index>{}(key.interface_type);
    const auto key_hash = std::hash<lookup_type_index>{}(key.key_type);
    return interface_hash ^ (key_hash + 0x9e3779b9U + (interface_hash << 6U) +
                             (interface_hash >> 2U));
  }
};
} // namespace std
