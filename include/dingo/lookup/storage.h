//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <memory>
#include <type_traits>

namespace dingo::detail {

template <typename Value, typename Allocator>
using lookup_storage_allocator_t =
    typename std::allocator_traits<Allocator>::template rebind_alloc<Value>;

template <typename StorageAllocator, typename Allocator>
StorageAllocator make_lookup_storage_allocator(Allocator &allocator) {
  return StorageAllocator(allocator);
}

template <typename Storage, typename = void>
struct has_allocator_type : std::false_type {};

template <typename Storage>
struct has_allocator_type<Storage,
                          std::void_t<typename Storage::allocator_type>>
    : std::true_type {};

// A standard container is constructed from the container's allocator rebound
// to its element; any other storage takes the allocator itself.
template <typename Storage, typename Allocator>
Storage make_lookup_storage(Allocator &allocator) {
  if constexpr (has_allocator_type<Storage>::value) {
    return Storage(typename Storage::allocator_type(allocator));
  } else {
    return Storage(allocator);
  }
}

} // namespace dingo::detail
