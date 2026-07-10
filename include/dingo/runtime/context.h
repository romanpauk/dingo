//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/core/context_base.h>
#include <dingo/memory/object_store.h>
#include <dingo/runtime/transaction.h>
#include <dingo/type/dependency_traits.h>

#include <memory>
#include <type_traits>
#include <utility>

namespace dingo {

template <typename Allocator>
class runtime_context : public detail::context_path_state {
  using transaction_type = runtime_transaction<Allocator>;
  enum class construction_kind {
    ephemeral,
    persistent,
  };

  struct construction_policy {
    construction_kind kind;
    construction_policy *previous;
  };

public:
  class transaction_scope {
  public:
    transaction_scope(runtime_context &context, transaction_type &transaction)
        : context_(std::addressof(context)), previous_(context.transaction_) {
      context_->transaction_ = std::addressof(transaction);
    }

    ~transaction_scope() { context_->transaction_ = previous_; }

    transaction_scope(const transaction_scope &) = delete;
    transaction_scope &operator=(const transaction_scope &) = delete;
    transaction_scope(transaction_scope &&) = delete;
    transaction_scope &operator=(transaction_scope &&) = delete;

  private:
    runtime_context *context_;
    transaction_type *previous_;
  };

  class construction_scope {
  public:
    construction_scope(runtime_context &context, construction_kind kind)
        : context_(std::addressof(context)), policy_{kind, context.policy_} {
      context_->policy_ = std::addressof(policy_);
    }

    ~construction_scope() {
      if (context_ != nullptr) {
        context_->policy_ = policy_.previous;
      }
    }

    construction_scope(const construction_scope &) = delete;
    construction_scope &operator=(const construction_scope &) = delete;
    construction_scope(construction_scope &&) = delete;
    construction_scope &operator=(construction_scope &&) = delete;

  private:
    runtime_context *context_;
    construction_policy policy_;
  };

  runtime_context(arena<> &scratch, transaction_type &transaction)
      : scratch_(std::addressof(scratch)),
        transaction_(std::addressof(transaction)) {}

  ~runtime_context() noexcept { ephemeral_store_.destroy(*scratch_); }

  runtime_context(const runtime_context &) = delete;
  runtime_context &operator=(const runtime_context &) = delete;

  template <typename T, typename... Args> T &construct(Args &&...args) {
    return construct_with_policy<T>(
        [&](void *ptr) { new (ptr) T(std::forward<Args>(args)...); });
  }

  template <typename T, typename DetectionTag, typename Container>
  T construct(Container &container) {
    using temporary_type = normalized_type_t<T>;
    auto &instance = construct_with_policy<temporary_type>([&](void *ptr) {
      detail::default_constructor_detection<temporary_type, DetectionTag>()
          .template construct<temporary_type>(ptr, *this, container);
    });

    if constexpr (std::is_lvalue_reference_v<T>) {
      return instance;
    } else {
      return std::move(instance);
    }
  }

  template <typename T, typename... Args>
  T &construct_persistent(Args &&...args) {
    return *transaction_->template construct_persistent<T>(
        std::forward<Args>(args)...);
  }

  template <typename Fn> void on_rollback(Fn &&fn) {
    transaction_->on_rollback(std::forward<Fn>(fn));
  }

  template <typename Runtime> bool owns(const Runtime &runtime) const noexcept {
    return transaction_->owns(runtime);
  }

  transaction_scope use_transaction(transaction_type &transaction) {
    return transaction_scope(*this, transaction);
  }

  void add_runtime_destructor(void *instance, void (*dtor)(void *) noexcept) {
    transaction_->add_runtime_destructor(instance, dtor);
  }

  construction_scope use_ephemeral_construction() {
    return construction_scope(*this, construction_kind::ephemeral);
  }

  construction_scope use_persistent_construction() {
    return construction_scope(*this, construction_kind::persistent);
  }

  bool retains_constructed_sources() const noexcept {
    return policy_ != nullptr && policy_->kind == construction_kind::persistent;
  }

  template <typename T, typename Container> T resolve(Container &container) {
    if (retains_constructed_sources() && !request_may_escape_v<T>) {
      auto scope = use_ephemeral_construction();
      return detail::resolve_context_request<T>(*this, container);
    }

    return detail::resolve_context_request<T>(*this, container);
  }

private:
  template <typename T, typename ConstructFn>
  T &construct_with_policy(ConstructFn &&construct_fn) {
    if (policy_ != nullptr && policy_->kind == construction_kind::persistent) {
      return *transaction_->template construct_persistent_with<T>(
          std::forward<ConstructFn>(construct_fn));
    }

    return ephemeral_store_.template construct_at<T>(
        *scratch_, std::forward<ConstructFn>(construct_fn));
  }

  arena<> *scratch_;
  detail::object_store<arena<>> ephemeral_store_;
  transaction_type *transaction_;
  construction_policy *policy_ = nullptr;
};

template <typename Runtime, typename Fn>
decltype(auto) execute_transaction(Runtime &runtime, Fn &&fn) {
  using allocator_type = typename Runtime::allocator_type;
  using context_type = runtime_context<allocator_type>;
  inline_arena<DINGO_CONTEXT_ARENA_BUFFER_SIZE> scratch;
  runtime_transaction<allocator_type> transaction(runtime, scratch);
  context_type context(scratch, transaction);

  if constexpr (std::is_void_v<std::invoke_result_t<Fn, context_type &>>) {
    std::forward<Fn>(fn)(context);
    transaction.commit();
  } else {
    decltype(auto) result = std::forward<Fn>(fn)(context);
    transaction.commit();
    return result;
  }
}

template <typename Runtime, typename Allocator, typename Fn>
auto execute_transaction(Runtime &runtime, runtime_context<Allocator> &context,
                         Fn &&fn)
    -> std::invoke_result_t<Fn, runtime_context<Allocator> &> {
  using result_type = std::invoke_result_t<Fn, runtime_context<Allocator> &>;
  assert(!context.owns(runtime));

  inline_arena<DINGO_CONTEXT_ARENA_BUFFER_SIZE> scratch;
  runtime_transaction<Allocator> transaction(runtime, scratch);
  auto scope = context.use_transaction(transaction);

  if constexpr (std::is_void_v<result_type>) {
    std::forward<Fn>(fn)(context);
    transaction.commit();
  } else {
    result_type result = std::forward<Fn>(fn)(context);
    transaction.commit();
    return std::forward<result_type>(result);
  }
}

} // namespace dingo
