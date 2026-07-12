//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/core/config.h>
#include <dingo/core/construction_scope.h>

#include <dingo/type/type_list.h>

#include <cassert>
#include <memory>
#include <utility>

namespace dingo {
template <typename... Types> struct conversion_cache;

template <> struct conversion_cache<> {
  void reset() {}
};

template <typename T> struct conversion_cache_entry {
  template <typename Owner, typename... Args>
  T &construct(construction_scope scope, Owner &owner, Args &&...args) {
    if (value_ == nullptr) {
      value_ = std::addressof(
          owner.template construct<T>(scope, std::forward<Args>(args)...));
    }
    return *value_;
  }

  template <typename Context, typename... Args>
  T &construct_tracked(Context &context, Args &&...args) {
    if (value_ == nullptr) {
      assert(value_ == nullptr);
      auto *created = std::addressof(context.template construct<T>(
          persistent_scope, std::forward<Args>(args)...));
      context.on_rollback([this]() noexcept { value_ = nullptr; });
      value_ = created;
    }
    return *value_;
  }

  void reset() { value_ = nullptr; }

  T *value_ = nullptr;
};

template <typename... Types>
struct conversion_cache : conversion_cache_entry<Types>... {
  template <typename T, typename Owner, typename... Args>
  T &construct(construction_scope scope, Owner &owner, Args &&...args) {
    return static_cast<conversion_cache_entry<T> &>(*this).construct(
        scope, owner, std::forward<Args>(args)...);
  }

  template <typename T, typename Context, typename... Args>
  T &construct_tracked(Context &context, Args &&...args) {
    return static_cast<conversion_cache_entry<T> &>(*this).construct_tracked(
        context, std::forward<Args>(args)...);
  }

  void reset() {
    (static_cast<conversion_cache_entry<Types> &>(*this).reset(), ...);
  }
};

// Conversion caches retain typed slots but leave object lifetime to the
// allocation owner passed to construct().
template <typename... Args>
struct conversion_cache<type_list<Args...>> : conversion_cache<Args...> {};

template <> struct conversion_cache<type_list<>> {
  void reset() {}
};

} // namespace dingo
