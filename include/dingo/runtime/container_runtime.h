//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/core/config.h>

#include <dingo/memory/arena_allocator.h>
#include <dingo/memory/object_store.h>

#include <cassert>
#include <cstddef>
#include <utility>

namespace dingo {

class runtime_transaction;

template <typename Allocator> class container_runtime {
  using arena_type = arena<Allocator>;
  using store_type = detail::object_store<arena_type>;

  friend class runtime_transaction;

public:
  using allocator_type = Allocator;

  struct checkpoint {
    typename arena_type::checkpoint arena;
    typename store_type::checkpoint objects;
  };

  explicit container_runtime(const Allocator &allocator)
      : arena_(DINGO_RUNTIME_ARENA_BLOCK_SIZE, allocator), store_(arena_) {}

  ~container_runtime() { assert(active_transaction_ == nullptr); }

  container_runtime(const container_runtime &) = delete;
  container_runtime &operator=(const container_runtime &) = delete;

  template <typename T, typename... Args> T &construct(Args &&...args) {
    return store_.template construct<T>(std::forward<Args>(args)...);
  }

  checkpoint mark() const { return checkpoint{arena_.mark(), store_.mark()}; }

  void *allocate(std::size_t size, std::size_t alignment) {
    return store_.allocate(size, alignment);
  }

  void add_destructor(void *instance, void (*dtor)(void *) noexcept) {
    store_.add_destructor(instance, dtor);
  }

  void rollback(checkpoint point) {
    store_.destroy(point.objects);
    arena_.rewind(point.arena);
  }

  void commit(checkpoint) {}

private:
  arena_type arena_;
  store_type store_;
  runtime_transaction *active_transaction_ = nullptr;
};

} // namespace dingo
