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
#include <type_traits>
#include <utility>

namespace dingo {
struct ordered {
  template <typename Key, typename Value, typename Cardinality,
            typename Allocator>
  using storage = std::conditional_t<
      std::is_same_v<Cardinality, one>,
      std::map<Key, Value, std::less<Key>,
               detail::lookup_storage_allocator_t<std::pair<const Key, Value>,
                                                  Allocator>>,
      std::multimap<Key, Value, std::less<Key>,
                    detail::lookup_storage_allocator_t<
                        std::pair<const Key, Value>, Allocator>>>;
};

} // namespace dingo
