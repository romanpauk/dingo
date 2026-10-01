//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/core/config.h>

#include <cassert>
#include <memory>
#include <utility>

namespace dingo {
template <typename Allocator> struct allocator_base : public Allocator {
  template <typename AllocatorT>
  allocator_base(AllocatorT &&alloc)
      : Allocator(std::forward<AllocatorT>(alloc)) {}

  Allocator &get_allocator() { return *this; }
};
} // namespace dingo
