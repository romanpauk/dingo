//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/memory/arena_allocator.h>
#include <dingo/memory/object_store.h>
#include <dingo/runtime/container_runtime.h>

#include <cassert>
#include <cstddef>
#include <memory>
#include <new>
#include <type_traits>
#include <utility>

namespace dingo {

template <typename Allocator> class runtime_transaction {
  using runtime_type = container_runtime<Allocator>;

  struct action_link {
    action_link(action_link *previous_node,
                void (*invoke_fn)(action_link *) noexcept) noexcept
        : previous(previous_node), invoke(invoke_fn) {}

    action_link *previous;
    void (*invoke)(action_link *) noexcept;
  };

  template <typename Fn> struct action_node final : action_link {
    template <typename FnT>
    explicit action_node(action_link *previous_node, FnT &&fn)
        : action_link(previous_node, &invoke_node),
          callback(std::forward<FnT>(fn)) {}

    static void invoke_node(action_link *base) noexcept {
      auto *node = static_cast<action_node *>(base);
      node->callback();
    }

    Fn callback;
  };

  using action_store = detail::object_store<arena<>>;

  struct action_checkpoint {
    action_link *commit_tail;
    action_link *rollback_tail;
    typename action_store::checkpoint objects;
  };

  struct action_state {
    explicit action_state(arena<> &scratch)
        : scratch_arena(std::addressof(scratch)) {}

    ~action_state() { objects.destroy(*scratch_arena); }

    action_checkpoint mark() const noexcept {
      return {commit_tail, rollback_tail, objects.mark()};
    }

    template <typename T, typename... Args> T &construct(Args &&...args) {
      return objects.template construct<T>(*scratch_arena,
                                           std::forward<Args>(args)...);
    }

    void destroy(typename action_store::checkpoint point) noexcept {
      objects.destroy(*scratch_arena, point);
    }

    arena<> *scratch_arena;
    action_store objects;
    action_link *commit_tail = nullptr;
    action_link *rollback_tail = nullptr;
  };

  union action_data {
    action_data() {}
    ~action_data() {}

    action_state state;
    action_checkpoint checkpoint;
  };

public:
  runtime_transaction(runtime_type &runtime, arena<> &scratch)
      : parent_(runtime.active_transaction_), runtime_(std::addressof(runtime)),
        checkpoint_(runtime.mark()) {
    if (parent_ == nullptr) {
      new (std::addressof(actions_.state)) action_state(scratch);
    } else {
      actions_.checkpoint = shared_actions().mark();
    }
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdangling-pointer"
#endif
    runtime.active_transaction_ = this;
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif
  }

  ~runtime_transaction() noexcept {
    rollback();
    if (parent_ == nullptr) {
      actions_.state.~action_state();
    }
  }

  runtime_transaction(const runtime_transaction &) = delete;
  runtime_transaction &operator=(const runtime_transaction &) = delete;

  template <typename T, typename... Args>
  T *construct_persistent(Args &&...args) {
    return construct_persistent_with<T>(
        [&](void *ptr) { new (ptr) T(std::forward<Args>(args)...); });
  }

  template <typename T, typename ConstructFn>
  T *construct_persistent_with(ConstructFn &&construct_fn) {
    auto *instance =
        reinterpret_cast<T *>(runtime_->allocate(sizeof(T), alignof(T)));
    std::forward<ConstructFn>(construct_fn)(instance);
    if constexpr (!std::is_trivially_destructible_v<T>) {
      try {
        runtime_->add_destructor(instance, &destructor<T>);
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
    auto &actions = shared_actions();
    add_action(actions, actions.rollback_tail, std::forward<Fn>(fn));
  }

  template <typename Fn> void on_commit(Fn &&fn) {
    static_assert(std::is_nothrow_invocable_v<std::decay_t<Fn> &>);
    static_assert(std::is_nothrow_destructible_v<std::decay_t<Fn>>);
    auto &actions = shared_actions();
    add_action(actions, actions.commit_tail, std::forward<Fn>(fn));
  }

  template <typename Fn> void on_finish(Fn &&fn) {
    static_assert(std::is_nothrow_invocable_v<std::decay_t<Fn> &>);
    static_assert(std::is_nothrow_destructible_v<std::decay_t<Fn>>);
    using action_type = std::decay_t<Fn>;
    auto &actions = shared_actions();
    auto &callback =
        actions.template construct<action_type>(std::forward<Fn>(fn));
    auto invoke = [&callback]() noexcept { callback(); };
    add_action(actions, actions.commit_tail, invoke);
    add_action(actions, actions.rollback_tail, invoke);
  }

  void add_runtime_destructor(void *instance, void (*dtor)(void *) noexcept) {
    runtime_->add_destructor(instance, dtor);
  }

  void commit() {
    if (runtime_ == nullptr) {
      return;
    }
    if (parent_ == nullptr) {
      finish_actions(true);
    }
    finish_runtime(true);
    runtime_ = nullptr;
  }

  void rollback() {
    if (runtime_ == nullptr) {
      return;
    }
    finish_actions(false);
    finish_runtime(false);
    runtime_ = nullptr;
  }

private:
  template <typename Fn>
  static void add_action(action_state &actions, action_link *&tail, Fn &&fn) {
    using action_type = std::decay_t<Fn>;
    using node_type = action_node<action_type>;
    auto &created =
        actions.template construct<node_type>(tail, std::forward<Fn>(fn));
    tail = std::addressof(created);
  }

  template <typename T> static void destructor(void *ptr) noexcept {
    reinterpret_cast<T *>(ptr)->~T();
  }

  void finish_runtime(bool commit) {
    assert(runtime_->active_transaction_ == this);
    if (commit) {
      runtime_->commit(checkpoint_);
    } else {
      runtime_->rollback(checkpoint_);
    }
    assert(runtime_->active_transaction_ == this);
    runtime_->active_transaction_ = parent_;
  }

  void finish_actions(bool committing) noexcept {
    auto &actions = shared_actions();
    const action_checkpoint checkpoint =
        parent_ != nullptr
            ? actions_.checkpoint
            : action_checkpoint{nullptr, nullptr,
                                typename action_store::checkpoint{nullptr}};
    auto *&tail = committing ? actions.commit_tail : actions.rollback_tail;
    auto *end = committing ? checkpoint.commit_tail : checkpoint.rollback_tail;
    while (tail != end) {
      auto *action = tail;
      tail = action->previous;
      action->invoke(action);
    }

    actions.commit_tail = checkpoint.commit_tail;
    actions.rollback_tail = checkpoint.rollback_tail;
    actions.destroy(checkpoint.objects);
  }

  runtime_transaction *root_transaction() noexcept {
    auto *root = this;
    while (root->parent_ != nullptr) {
      root = root->parent_;
    }
    return root;
  }

  action_state &shared_actions() noexcept {
    return root_transaction()->actions_.state;
  }

  runtime_transaction *parent_;
  action_data actions_;
  runtime_type *runtime_;
  typename runtime_type::checkpoint checkpoint_;
};

} // namespace dingo
