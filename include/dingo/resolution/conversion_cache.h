//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/base/config.h>
#include <dingo/base/construction_scope.h>

#include <dingo/base/type_list.h>

#include <cassert>
#include <memory>
#include <utility>

namespace dingo {
template <typename Types> struct conversion_cache;
// A conversion constructed once and kept until the cache is reset or the
// transaction that constructed it rolls back.
template <typename T> struct conversion_cache_entry {
  template <typename Context, typename... Args>
  T &construct(Context &context, Args &&...args) {
    if (value_ == nullptr) {
      auto *created = std::addressof(context.template construct<T>(
          persistent_scope, std::forward<Args>(args)...));
      context.on_rollback(
          +[](void *self, void *) noexcept {
            static_cast<conversion_cache_entry *>(self)->value_ = nullptr;
          },
          this);
      value_ = created;
    }
    return *value_;
  }

  void reset() { value_ = nullptr; }

  T *value_ = nullptr;
};

// The conversions a binding retains, one entry per type. The cache keeps
// typed slots and leaves object lifetime to the allocation owner passed to
// construct().
template <typename... Types>
struct conversion_cache<type_list<Types...>>
    : conversion_cache_entry<Types>... {
  template <typename T, typename Context, typename... Args>
  T &construct(Context &context, Args &&...args) {
    return static_cast<conversion_cache_entry<T> &>(*this).construct(
        context, std::forward<Args>(args)...);
  }

  void reset() {
    (static_cast<conversion_cache_entry<Types> &>(*this).reset(), ...);
  }
};

} // namespace dingo
