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
#include <memory>
#include <new>
#include <type_traits>
#include <utility>

namespace dingo {

class runtime_transaction {
  struct action_node {
    action_node *previous;
    void (*process)(action_node *, bool, bool) noexcept;
    bool processed;
  };

  template <typename Fn, bool OnCommit, bool OnRollback>
  struct action_node_model final : action_node {
    template <typename FnT>
    explicit action_node_model(action_node *previous_node, FnT &&fn)
        : action_node{previous_node, &process_node, false},
          callback(std::forward<FnT>(fn)) {}

    static void process_node(action_node *base, bool committing,
                             bool destroy) noexcept {
      auto *node = static_cast<action_node_model *>(base);
      if (destroy) {
        node->~action_node_model();
        return;
      }
      if ((committing && OnCommit) || (!committing && OnRollback)) {
        node->callback();
      }
    }

    Fn callback;
  };

  struct runtime_operations {
    void *(*allocate)(void *, std::size_t, std::size_t);
    void (*add_destructor)(void *, void *, void (*)(void *) noexcept);
    void (*finish)(void *, void *, bool, runtime_transaction *,
                   runtime_transaction *);
  };

public:
  template <typename Runtime>
  runtime_transaction(Runtime &runtime, arena<> &scratch)
      : parent_(runtime.active_transaction_),
        scratch_(parent_ != nullptr ? parent_->scratch_
                                    : std::addressof(scratch)),
        runtime_(std::addressof(runtime)),
        operations_(std::addressof(runtime_operations_for<Runtime>())) {
    using checkpoint_type = typename Runtime::checkpoint;
    static_assert(sizeof(checkpoint_type) <= sizeof(checkpoint_storage_));
    static_assert(alignof(checkpoint_type) <= alignof(void *));

    action_checkpoint_ = shared_action_tail();
    new (&checkpoint_storage_) checkpoint_type(runtime.mark());
    runtime.active_transaction_ = this;
  }

  ~runtime_transaction() noexcept { rollback(); }

  runtime_transaction(const runtime_transaction &) = delete;
  runtime_transaction &operator=(const runtime_transaction &) = delete;

  template <typename T, typename... Args>
  T *construct_persistent(Args &&...args) {
    return construct_persistent_with<T>(
        [&](void *ptr) { new (ptr) T(std::forward<Args>(args)...); });
  }

  template <typename T, typename ConstructFn>
  T *construct_persistent_with(ConstructFn &&construct_fn) {
    auto *instance = reinterpret_cast<T *>(
        operations_->allocate(runtime_, sizeof(T), alignof(T)));
    std::forward<ConstructFn>(construct_fn)(instance);
    if constexpr (!std::is_trivially_destructible_v<T>) {
      try {
        operations_->add_destructor(runtime_, instance, &destructor<T>);
      } catch (...) {
        instance->~T();
        throw;
      }
    }
    return instance;
  }

  template <typename Fn> void on_rollback(Fn &&fn) {
    static_assert(std::is_nothrow_invocable_v<std::decay_t<Fn> &>);
    static_assert(std::is_nothrow_destructible_v<std::decay_t<Fn>>);
    add_action<false, true>(std::forward<Fn>(fn));
  }

  template <typename Fn> void on_commit(Fn &&fn) {
    static_assert(std::is_nothrow_invocable_v<std::decay_t<Fn> &>);
    static_assert(std::is_nothrow_destructible_v<std::decay_t<Fn>>);
    add_action<true, false>(std::forward<Fn>(fn));
  }

  template <typename Fn> void on_finish(Fn &&fn) {
    static_assert(std::is_nothrow_invocable_v<std::decay_t<Fn> &>);
    static_assert(std::is_nothrow_destructible_v<std::decay_t<Fn>>);
    add_action<true, true>(std::forward<Fn>(fn));
  }

  void add_runtime_destructor(void *instance, void (*dtor)(void *) noexcept) {
    operations_->add_destructor(runtime_, instance, dtor);
  }

  template <bool OnCommit, bool OnRollback, typename Fn>
  void add_action(Fn &&fn) {
    using action_type = std::decay_t<Fn>;
    using node_type = action_node_model<action_type, OnCommit, OnRollback>;
    auto *storage = scratch_->allocate(sizeof(node_type), alignof(node_type));
    auto *created =
        new (storage) node_type(shared_action_tail(), std::forward<Fn>(fn));
    shared_action_tail() = created;
  }

  void commit() {
    if (runtime_ == nullptr) {
      return;
    }
    if (parent_ == nullptr) {
      finish_actions(true);
    }
    operations_->finish(runtime_, std::addressof(checkpoint_storage_), true,
                        this, parent_);
    runtime_ = nullptr;
  }

  void rollback() {
    if (runtime_ == nullptr) {
      return;
    }
    finish_actions(false);
    operations_->finish(runtime_, std::addressof(checkpoint_storage_), false,
                        this, parent_);
    runtime_ = nullptr;
  }

private:
  template <typename T> static void destructor(void *ptr) noexcept {
    reinterpret_cast<T *>(ptr)->~T();
  }

  template <typename Runtime>
  static void *allocate_runtime(void *runtime, std::size_t size,
                                std::size_t alignment) {
    return reinterpret_cast<Runtime *>(runtime)->allocate(size, alignment);
  }

  template <typename Runtime>
  static void add_runtime_destructor(void *runtime, void *instance,
                                     void (*dtor)(void *) noexcept) {
    reinterpret_cast<Runtime *>(runtime)->add_destructor(instance, dtor);
  }

  template <typename Runtime>
  static void finish_runtime(void *runtime_ptr, void *checkpoint, bool commit,
                             runtime_transaction *transaction,
                             runtime_transaction *parent) {
    auto &runtime = *reinterpret_cast<Runtime *>(runtime_ptr);
    assert(runtime.active_transaction_ == transaction);
    if (commit) {
      runtime.commit(
          *reinterpret_cast<typename Runtime::checkpoint *>(checkpoint));
    } else {
      runtime.rollback(
          *reinterpret_cast<typename Runtime::checkpoint *>(checkpoint));
    }
    assert(runtime.active_transaction_ == transaction);
    runtime.active_transaction_ = parent;
  }

  template <typename Runtime>
  static const runtime_operations &runtime_operations_for() noexcept {
    static const runtime_operations operations{&allocate_runtime<Runtime>,
                                               &add_runtime_destructor<Runtime>,
                                               &finish_runtime<Runtime>};
    return operations;
  }

  void finish_actions(bool committing) noexcept {
    auto *&tail = shared_action_tail();
    auto *cursor = tail;
    while (cursor != action_checkpoint_) {
      if (cursor->processed) {
        cursor = cursor->previous;
        continue;
      }
      cursor->processed = true;
      auto *head_before_callback = tail;
      cursor->process(cursor, committing, false);
      cursor = tail != head_before_callback ? tail : cursor->previous;
    }

    while (tail != action_checkpoint_) {
      auto *destroyed = tail;
      tail = destroyed->previous;
      destroyed->process(destroyed, committing, true);
    }
  }

  runtime_transaction *root_transaction() noexcept {
    auto *root = this;
    while (root->parent_ != nullptr) {
      root = root->parent_;
    }
    return root;
  }

  action_node *&shared_action_tail() noexcept {
    return root_transaction()->action_tail_;
  }

  runtime_transaction *parent_;
  arena<> *scratch_;
  action_node *action_tail_ = nullptr;
  action_node *action_checkpoint_ = nullptr;
  void *runtime_;
  const runtime_operations *operations_;
  alignas(void *) std::byte checkpoint_storage_[sizeof(void *) * 3];
};

} // namespace dingo
