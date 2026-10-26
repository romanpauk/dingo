//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/base/config.h>

#include <dingo/memory/arena_allocator.h>
#include <dingo/memory/object_store.h>

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <utility>

namespace dingo {

template <typename Allocator> class runtime_transaction;

template <typename Allocator> class container_runtime {
  using arena_type = arena<Allocator>;
  using store_type = detail::object_store<arena_type>;

  friend class runtime_transaction<Allocator>;

public:
  using allocator_type = Allocator;

  struct checkpoint {
    typename arena_type::checkpoint arena;
    typename store_type::checkpoint objects;
  };

  explicit container_runtime(const Allocator &allocator)
      : arena_(DINGO_RUNTIME_ARENA_BLOCK_SIZE, allocator) {}

  ~container_runtime() {
    assert(active_transaction() == nullptr);
    store_.destroy(arena_);
  }

  container_runtime(const container_runtime &) = delete;
  container_runtime &operator=(const container_runtime &) = delete;

  template <typename T, typename... Args> T &construct(Args &&...args) {
    return store_.template construct<T>(arena_, std::forward<Args>(args)...);
  }

  checkpoint mark() const { return checkpoint{arena_.mark(), store_.mark()}; }

  void *allocate(std::size_t size, std::size_t alignment) {
    return store_.allocate(arena_, size, alignment);
  }

  void add_destructor(void *instance, void (*dtor)(void *) noexcept) {
    store_.add_destructor(arena_, instance, dtor);
  }

  void rollback(checkpoint point) {
    store_.destroy(arena_, point.objects);
    arena_.rewind(point.arena);
  }

  void commit(checkpoint) {}

  // Set once the first resolve succeeds. Registrations that would replace a
  // lookup are refused from then on.
  bool sealed() const noexcept { return (state_ & sealed_bit) != 0; }

  void seal() noexcept { state_ |= sealed_bit; }

private:
  using transaction_type = runtime_transaction<Allocator>;

  // The sealed flag lives in the low bit of the word that holds the active
  // transaction, which is at least pointer-aligned, so sealing costs no space.
  static constexpr std::uintptr_t sealed_bit = 1;

  transaction_type *active_transaction() const noexcept {
    return reinterpret_cast<transaction_type *>(state_ & ~sealed_bit);
  }

  void active_transaction(transaction_type *transaction) noexcept {
    state_ =
        reinterpret_cast<std::uintptr_t>(transaction) | (state_ & sealed_bit);
  }

  arena_type arena_;
  store_type store_;
  std::uintptr_t state_ = 0;
};

} // namespace dingo
