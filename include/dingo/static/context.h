//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/base/config.h>
#include <dingo/context/resolution_context.h>
#include <dingo/memory/arena_allocator.h>
#include <dingo/memory/object_store.h>

#include <cstddef>
#include <new>
#include <type_traits>
#include <utility>

namespace dingo {

// The memory of a static resolution: the instances it constructs live until
// the context ends. Nothing outlives the context, so there is no transaction to
// roll back. A trivially destructible instance that fits is placed in a buffer
// of the context, which a resolution compiles away; any other lives in an
// arena made on its first use.
class static_context_backend {
  using arena_type = inline_arena<DINGO_CONTEXT_ARENA_BUFFER_SIZE>;
  static constexpr std::size_t buffer_size = DINGO_CONTEXT_ARENA_BUFFER_SIZE;

public:
  static constexpr bool is_transactional = false;

  static_context_backend() = default;
  ~static_context_backend() {
    if (arena_ != nullptr) {
      release();
    }
  }

  static_context_backend(const static_context_backend &) = delete;
  static_context_backend &operator=(const static_context_backend &) = delete;

  void on_rollback(void (*)(void *, void *) noexcept, void *,
                   void * = nullptr) noexcept {}

protected:
  template <typename T, typename ConstructFn>
  T &construct_storage(construction_scope, ConstructFn &&construct_fn) {
    if constexpr (std::is_trivially_destructible_v<T> &&
                  sizeof(T) <= buffer_size &&
                  alignof(T) <= alignof(std::max_align_t)) {
      const std::size_t offset = (used_ + alignof(T) - 1) & ~(alignof(T) - 1);
      if (offset + sizeof(T) <= buffer_size) {
        used_ = offset + sizeof(T);
        void *ptr = buffer_ + offset;
        std::forward<ConstructFn>(construct_fn)(ptr);
        return *std::launder(static_cast<T *>(ptr));
      }
    }
    return construct_in_arena<T>(std::forward<ConstructFn>(construct_fn));
  }

private:
  template <typename T, typename ConstructFn>
  DINGO_NOINLINE T &construct_in_arena(ConstructFn &&construct_fn) {
    if (arena_ == nullptr) {
      arena_ = new (&arena_storage_) arena_type();
    }
    return store_.template construct_at<T>(
        *arena_, std::forward<ConstructFn>(construct_fn));
  }

  DINGO_NOINLINE void release() noexcept {
    store_.destroy(*arena_);
    arena_->~arena_type();
  }

  alignas(std::max_align_t) unsigned char buffer_[buffer_size];
  std::size_t used_ = 0;
  alignas(arena_type) unsigned char arena_storage_[sizeof(arena_type)];
  arena_type *arena_ = nullptr;
  detail::object_store<arena<>> store_;
};

using static_context = resolution_context<static_context_backend>;

} // namespace dingo
