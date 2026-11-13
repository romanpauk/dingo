//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/base/config.h>
#include <dingo/context/resolution.h>
#include <dingo/memory/arena_allocator.h>
#include <dingo/memory/object_store.h>

#include <cstddef>
#include <memory>
#include <new>
#include <type_traits>
#include <utility>

namespace dingo {

// What a static container constructs for its own lifetime: the dependencies
// of the instances it keeps that no storage keeps, such as a unique instance a
// shared one refers to. The memory is allocated on its first use and released
// with the container. Nothing rolls it back, so what a failed resolution made
// stays until then.
class static_persistent_memory {
public:
  static_persistent_memory() = default;
  ~static_persistent_memory() {
    if (arena_ != nullptr) {
      store_.destroy(*arena_);
    }
  }

  static_persistent_memory(const static_persistent_memory &) = delete;
  static_persistent_memory &
  operator=(const static_persistent_memory &) = delete;

  template <typename T, typename ConstructFn>
  DINGO_NOINLINE T &construct(ConstructFn &&construct_fn) {
    if (arena_ == nullptr) {
      arena_ = std::make_unique<arena<>>(DINGO_RUNTIME_ARENA_BLOCK_SIZE);
    }
    return store_.template construct_at<T>(
        *arena_, std::forward<ConstructFn>(construct_fn));
  }

private:
  std::unique_ptr<arena<>> arena_;
  detail::object_store<arena<>> store_;
};

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
  // A container without persistent memory passes none.
  explicit static_context_backend(static_persistent_memory *) {}
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

// The memory of a resolution of a static container that keeps instances
// constructed for its kept ones, or whose parent does: the persistent
// instances live as long as the container whose memory the context has.
class static_persistent_context_backend : public static_context_backend {
public:
  explicit static_persistent_context_backend(
      static_persistent_memory *persistent)
      : persistent_(persistent) {}

  // The memory persistent instances are constructed in, which a parent
  // container replaces with its own while it resolves its bindings. Without
  // one, they live as long as the context.
  static_persistent_memory *persistent_memory() const { return persistent_; }
  void persistent_memory(static_persistent_memory *persistent) {
    persistent_ = persistent;
  }

protected:
  template <typename T, typename ConstructFn>
  T &construct_storage(construction_scope scope, ConstructFn &&construct_fn) {
    if (scope.is_persistent() && persistent_ != nullptr) {
      return persistent_->template construct<T>(
          std::forward<ConstructFn>(construct_fn));
    }
    return static_context_backend::template construct_storage<T>(
        scope, std::forward<ConstructFn>(construct_fn));
  }

private:
  static_persistent_memory *persistent_;
};

// The memory of a request a runtime child forwards to a static container,
// which resolves it in a context of its own. The persistent instances live in
// the memory of the container, as what it keeps refers to them. The others,
// and the persistent ones of a container without the memory, which keeps
// nothing that refers to them, are the request's: they live in the context of
// the child, as long as the child needs them.
template <typename Requester> class forwarded_context_backend {
public:
  static constexpr bool is_transactional = false;
  // The runtime child rejects what it cannot resolve as it runs.
  static constexpr bool rejects_at_run_time = true;

  forwarded_context_backend(Requester &requester,
                            static_persistent_memory *persistent)
      : requester_(std::addressof(requester)), persistent_(persistent) {}

  forwarded_context_backend(const forwarded_context_backend &) = delete;
  forwarded_context_backend &
  operator=(const forwarded_context_backend &) = delete;

  void on_rollback(void (*)(void *, void *) noexcept, void *,
                   void * = nullptr) noexcept {}

  static_persistent_memory *persistent_memory() const { return persistent_; }
  void persistent_memory(static_persistent_memory *persistent) {
    persistent_ = persistent;
  }

protected:
  template <typename T, typename ConstructFn>
  T &construct_storage(construction_scope scope, ConstructFn &&construct_fn) {
    if (scope.is_persistent() && persistent_ != nullptr) {
      return persistent_->template construct<T>(
          std::forward<ConstructFn>(construct_fn));
    }
    return requester_->template construct_in_place<T>(
        scope, std::forward<ConstructFn>(construct_fn));
  }

private:
  Requester *requester_;
  static_persistent_memory *persistent_;
};

using static_context = resolution_context<static_context_backend>;
using static_persistent_context =
    resolution_context<static_persistent_context_backend>;
template <typename Requester>
using forwarded_context =
    resolution_context<forwarded_context_backend<Requester>>;

namespace detail {
// Whether a context places its persistent instances in a memory that a
// container replaces with its own while it resolves its bindings.
template <typename Context, typename = void>
struct has_persistent_memory_slot : std::false_type {};

template <typename Context>
struct has_persistent_memory_slot<
    Context, std::void_t<decltype(std::declval<Context &>().persistent_memory(
                 std::declval<static_persistent_memory *>()))>>
    : std::true_type {};

// Places the persistent instances of a static context in memory while it
// lives, and restores the memory the context had after.
template <typename Context> class persistent_memory_scope {
public:
  persistent_memory_scope(Context &context, static_persistent_memory *memory)
      : context_(context), previous_(context.persistent_memory()) {
    context.persistent_memory(memory);
  }
  ~persistent_memory_scope() { context_.persistent_memory(previous_); }

  persistent_memory_scope(const persistent_memory_scope &) = delete;
  persistent_memory_scope &operator=(const persistent_memory_scope &) = delete;

private:
  Context &context_;
  static_persistent_memory *previous_;
};
} // namespace detail

} // namespace dingo
