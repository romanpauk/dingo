//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/memory/arena_allocator.h>

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <new>
#include <type_traits>
#include <utility>

namespace dingo::detail {

// Objects constructed in an arena, destroyed in reverse order of their
// construction. The arena owns the memory; the store links the destructors.
template <typename Arena> class object_store {
  // A destructor to run, linked to the one added before it.
  struct destructor_node {
    destructor_node *previous;
    void *instance;
    void (*dtor)(void *) noexcept;
  };

public:
  struct checkpoint {
    destructor_node *tail;
  };

  object_store() = default;
  ~object_store() { assert(tail_ == nullptr); }
  object_store(const object_store &) = delete;
  object_store &operator=(const object_store &) = delete;

  void *allocate(Arena &arena, std::size_t size, std::size_t alignment) {
    assert(size <=
           static_cast<std::size_t>(std::numeric_limits<intptr_t>::max()));
    assert(alignment <=
           static_cast<std::size_t>(std::numeric_limits<intptr_t>::max()));
    void *memory = arena.allocate(static_cast<intptr_t>(size),
                                  static_cast<intptr_t>(alignment));
    if (memory == nullptr) {
      throw std::bad_alloc();
    }
    return memory;
  }

  template <typename T, typename... Args>
  T &construct(Arena &arena, Args &&...args) {
    return construct_at<T>(arena, [&](T *instance) {
      new (instance) T(std::forward<Args>(args)...);
    });
  }

// A runtime-selected construction callback may be non-returning for a specific
// template instantiation. MSVC diagnoses the bookkeeping below it as
// unreachable even though that instantiation is not selected at runtime.
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4702)
#endif
  template <typename T, typename ConstructFn>
  T &construct_at(Arena &arena, ConstructFn &&construct) {
    auto *instance =
        reinterpret_cast<T *>(allocate(arena, sizeof(T), alignof(T)));
    std::forward<ConstructFn>(construct)(instance);
    if constexpr (!std::is_trivially_destructible_v<T>) {
      try {
        add_destructor(arena, instance, &destructor<T>);
      } catch (...) {
        instance->~T();
        throw;
      }
    }
    return *instance;
  }
#ifdef _MSC_VER
#pragma warning(pop)
#endif

  void add_destructor(Arena &arena, void *instance,
                      void (*dtor)(void *) noexcept) {
    tail_ =
        new (allocate(arena, sizeof(destructor_node), alignof(destructor_node)))
            destructor_node{tail_, instance, dtor};
  }

  checkpoint mark() const noexcept { return checkpoint{tail_}; }

  void destroy(Arena &arena) noexcept { destroy(arena, checkpoint{nullptr}); }

  // Runs the destructors added since the checkpoint, newest first.
  void destroy(Arena &, checkpoint point) noexcept {
    while (tail_ != point.tail) {
      auto *node = tail_;
      tail_ = node->previous;
      node->dtor(node->instance);
    }
  }

private:
  template <typename T> static void destructor(void *ptr) noexcept {
    reinterpret_cast<T *>(ptr)->~T();
  }

  destructor_node *tail_ = nullptr;
};

} // namespace dingo::detail
