//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/base/config.h>
#include <dingo/memory/aligned_storage.h>
#include <dingo/memory/object_lifetime.h>

#include <new>
#include <type_traits>

namespace dingo {
namespace detail {

// The inline storage of one resolved object plus the flag that says whether it
// holds a live value. The trivial specialization stays trivially destructible
// so the static graph only counts slots that actually need destruction.
template <typename Type,
          bool TriviallyDestructible = std::is_trivially_destructible_v<Type>>
class destructible_slot;

template <typename Type> class destructible_slot<Type, true> {
protected:
  Type *slot_address() const {
    return std::launder(reinterpret_cast<Type *>(&instance_));
  }

  void mark_constructed() { constructed_ = true; }

  void destroy() { constructed_ = false; }

  bool constructed() const { return constructed_; }

  mutable aligned_storage_t<sizeof(Type), alignof(Type)> instance_;
  bool constructed_ = false;
};

template <typename Type> class destructible_slot<Type, false> {
protected:
  ~destructible_slot() { destroy(); }

  Type *slot_address() const {
    return std::launder(reinterpret_cast<Type *>(&instance_));
  }

  void mark_constructed() { constructed_ = true; }

  void destroy() {
    if (constructed_) {
      constructed_ = false;
      destroy_object_value(*slot_address());
    }
  }

  bool constructed() const { return constructed_; }

  mutable aligned_storage_t<sizeof(Type), alignof(Type)> instance_;
  bool constructed_ = false;
};

} // namespace detail
} // namespace dingo
