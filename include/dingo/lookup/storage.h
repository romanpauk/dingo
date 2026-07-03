//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <memory>

namespace dingo::detail {

template <typename Value, typename Allocator>
using lookup_storage_allocator_t =
    typename std::allocator_traits<Allocator>::template rebind_alloc<Value>;

template <typename StorageAllocator, typename Allocator>
StorageAllocator make_lookup_storage_allocator(Allocator &allocator) {
  return StorageAllocator(allocator);
}

template <typename Storage, typename Allocator> struct lookup_storage_factory {
  static Storage make(Allocator &allocator) { return Storage(allocator); }
};

template <typename Storage, typename Allocator>
Storage make_lookup_storage(Allocator &allocator) {
  return lookup_storage_factory<Storage, Allocator>::make(allocator);
}

} // namespace dingo::detail
