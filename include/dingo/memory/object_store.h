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

template <typename T> class linked_stack {
  struct node {
    explicit node(node *previous_node, T entry)
        : previous(previous_node), value(std::move(entry)) {}

    node *previous;
    T value;
  };

public:
  struct checkpoint {
    node *tail;
  };

  linked_stack() = default;

  ~linked_stack() { assert(empty()); }

  linked_stack(const linked_stack &) = delete;
  linked_stack &operator=(const linked_stack &) = delete;

  template <typename Allocator> T &push(const Allocator &allocator, T value) {
    using node_allocator =
        typename std::allocator_traits<Allocator>::template rebind_alloc<node>;
    using node_allocator_traits = std::allocator_traits<node_allocator>;
    node_allocator rebound_allocator(allocator);
    auto *created = node_allocator_traits::allocate(rebound_allocator, 1);
    try {
      node_allocator_traits::construct(rebound_allocator, created, tail_,
                                       std::move(value));
    } catch (...) {
      node_allocator_traits::deallocate(rebound_allocator, created, 1);
      throw;
    }
    tail_ = created;
    return tail_->value;
  }

  bool empty() const noexcept { return tail_ == nullptr; }

  T &top() noexcept {
    assert(tail_ != nullptr);
    return tail_->value;
  }

  checkpoint mark() const noexcept { return checkpoint{tail_}; }

  template <typename Allocator> void pop(const Allocator &allocator) noexcept {
    using node_allocator =
        typename std::allocator_traits<Allocator>::template rebind_alloc<node>;
    using node_allocator_traits = std::allocator_traits<node_allocator>;
    node_allocator rebound_allocator(allocator);
    assert(tail_ != nullptr);
    auto *released = tail_;
    tail_ = released->previous;
    node_allocator_traits::destroy(rebound_allocator, released);
    node_allocator_traits::deallocate(rebound_allocator, released, 1);
  }

  template <typename Allocator, typename Fn>
  void pop_until(checkpoint point, const Allocator &allocator, Fn &&fn) {
    while (tail_ != point.tail) {
      fn(top());
      pop(allocator);
    }
  }

private:
  node *tail_ = nullptr;
};

template <typename Arena> class object_store {
  struct destructible {
    void *instance;
    void (*dtor)(void *) noexcept;
  };

  using destructor_allocator = arena_allocator<destructible, Arena>;
  using destructor_journal = linked_stack<destructible>;

public:
  using checkpoint = typename destructor_journal::checkpoint;

  void *allocate(Arena &arena, std::size_t size, std::size_t alignment) {
    assert(size <=
           static_cast<std::size_t>(std::numeric_limits<intptr_t>::max()));
    assert(alignment <=
           static_cast<std::size_t>(std::numeric_limits<intptr_t>::max()));
    return arena.allocate(static_cast<intptr_t>(size),
                          static_cast<intptr_t>(alignment));
  }

  template <typename T, typename... Args>
  T &construct(Arena &arena, Args &&...args) {
    return construct_at<T>(arena, [&](T *instance) {
      new (instance) T(std::forward<Args>(args)...);
    });
  }

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

  void add_destructor(Arena &arena, void *instance,
                      void (*dtor)(void *) noexcept) {
    destructors_.push(destructor_allocator(arena),
                      destructible{instance, dtor});
  }

  checkpoint mark() const noexcept { return destructors_.mark(); }

  void destroy(Arena &arena) noexcept {
    destroy(arena, typename destructor_journal::checkpoint{nullptr});
  }

  void destroy(Arena &arena, checkpoint point) noexcept {
    destructors_.pop_until(
        point, destructor_allocator(arena),
        [](destructible &entry) noexcept { entry.dtor(entry.instance); });
  }

private:
  template <typename T> static void destructor(void *ptr) noexcept {
    reinterpret_cast<T *>(ptr)->~T();
  }

  destructor_journal destructors_;
};

} // namespace dingo::detail
