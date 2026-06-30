//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/memory/static_allocator.h>

#include <memory>
#include <type_traits>

namespace dingo::detail {

template <typename Value, typename Allocator>
using lookup_storage_allocator_t = std::conditional_t<
    ::dingo::is_static_allocator_v<Allocator>, std::allocator<Value>,
    typename std::allocator_traits<Allocator>::template rebind_alloc<Value>>;

template <typename StorageAllocator, typename Allocator>
StorageAllocator make_lookup_storage_allocator(Allocator &allocator) {
  if constexpr (::dingo::is_static_allocator_v<Allocator>) {
    (void)allocator;
    return StorageAllocator{};
  } else {
    return StorageAllocator(allocator);
  }
}

} // namespace dingo::detail
