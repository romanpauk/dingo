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

class runtime_context : public detail::context_path_state {
  enum class construction_kind {
    ephemeral,
    persistent,
  };

  struct construction_policy {
    construction_kind kind;
    construction_policy *previous;
  };

public:
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

  runtime_context(arena<> &scratch, runtime_transaction &transaction)
      : ephemeral_store_(scratch), transaction_(std::addressof(transaction)) {}

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
        std::forward<ConstructFn>(construct_fn));
  }

  detail::object_store<arena<>> ephemeral_store_;
  runtime_transaction *transaction_;
  construction_policy *policy_ = nullptr;
};

template <typename Runtime, typename Fn>
decltype(auto) execute_transaction(Runtime &runtime, Fn &&fn) {
  inline_arena<DINGO_CONTEXT_ARENA_BUFFER_SIZE> scratch;
  runtime_transaction transaction(runtime, scratch);
  runtime_context context(scratch, transaction);

  if constexpr (std::is_void_v<std::invoke_result_t<Fn, runtime_context &>>) {
    std::forward<Fn>(fn)(context);
    transaction.commit();
  } else {
    decltype(auto) result = std::forward<Fn>(fn)(context);
    transaction.commit();
    return result;
  }
}

} // namespace dingo
