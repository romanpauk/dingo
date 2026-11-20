//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/lookup/storage.h>
#include <dingo/lookup/tags.h>

#include <map>
#include <type_traits>
#include <utility>

namespace dingo {
// Keys ordered by the comparison std::map makes of them by default, in nodes
// of the allocator of nodes of the lookups.
struct ordered {
  // Each value is in a node of its own, which no insert or erase moves.
  static constexpr bool node_based = true;

  template <typename Key, typename Value, typename Cardinality,
            typename Allocator>
  using storage = std::conditional_t<
      std::is_same_v<Cardinality, one>,
      std::map<Key, Value, typename std::map<Key, Value>::key_compare,
               detail::lookup_node_allocator_t<std::pair<const Key, Value>,
                                               Allocator>>,
      std::multimap<Key, Value, typename std::multimap<Key, Value>::key_compare,
                    detail::lookup_node_allocator_t<std::pair<const Key, Value>,
                                                    Allocator>>>;
};

} // namespace dingo
