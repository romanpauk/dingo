//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/core/config.h>

#include <dingo/index/map.h>

#include <unordered_map>

namespace dingo {
namespace index_type {
using unordered_map = associative<std::unordered_map>;
} // namespace index_type

namespace detail {
template <typename Key, typename Value, typename Allocator>
struct associative_index_storage_type<std::unordered_map, Key, Value,
                                      Allocator> {
  using allocator_type = typename std::allocator_traits<
      Allocator>::template rebind_alloc<std::pair<const Key, Value>>;
  using type = std::unordered_map<Key, Value, std::hash<Key>,
                                  std::equal_to<Key>, allocator_type>;
};
} // namespace detail
} // namespace dingo
