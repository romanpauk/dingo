//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/context/context_base.h>
#include <dingo/memory/object_store.h>
#include <dingo/runtime/transaction.h>

#include <memory>
#include <type_traits>
#include <utility>

namespace dingo {

template <typename Allocator> class runtime_context_backend {
  using transaction_type = runtime_transaction<Allocator>;

public:
  static constexpr bool is_transactional = true;

  class transaction_scope {
  public:
    transaction_scope(runtime_context_backend &context,
                      transaction_type &transaction)
        : context_(std::addressof(context)), previous_(context.transaction_) {
      context_->transaction_ = std::addressof(transaction);
    }

    ~transaction_scope() { context_->transaction_ = previous_; }

    transaction_scope(const transaction_scope &) = delete;
    transaction_scope &operator=(const transaction_scope &) = delete;
    transaction_scope(transaction_scope &&) = delete;
    transaction_scope &operator=(transaction_scope &&) = delete;

  private:
    runtime_context_backend *context_;
    transaction_type *previous_;
  };

  runtime_context_backend(arena<> &scratch, transaction_type &transaction)
      : scratch_(std::addressof(scratch)),
        transaction_(std::addressof(transaction)) {}

  ~runtime_context_backend() noexcept { ephemeral_store_.destroy(*scratch_); }

  runtime_context_backend(const runtime_context_backend &) = delete;
  runtime_context_backend &operator=(const runtime_context_backend &) = delete;

  void on_rollback(void (*fn)(void *, void *) noexcept, void *first,
                   void *second = nullptr) {
    transaction_->on_rollback(fn, first, second);
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

protected:
  template <typename T, typename ConstructFn>
  T &construct_storage(construction_scope scope, ConstructFn &&construct_fn) {
    if (scope.is_persistent()) {
      return *transaction_->template construct_persistent_with<T>(
          std::forward<ConstructFn>(construct_fn));
    }

    return ephemeral_store_.template construct_at<T>(
        *scratch_, std::forward<ConstructFn>(construct_fn));
  }

private:
  arena<> *scratch_;
  detail::object_store<arena<>> ephemeral_store_;
  transaction_type *transaction_;
};

template <typename Allocator>
using runtime_context = resolution_context<runtime_context_backend<Allocator>>;

namespace detail {

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4702)
#endif

// Commits a resolve transaction. The outermost successful resolve of a runtime
// seals it.
template <typename Transaction, typename Runtime>
class transaction_commit_guard {
public:
  transaction_commit_guard(Transaction &transaction, Runtime &runtime) noexcept
      : transaction_(std::addressof(transaction)),
        runtime_(std::addressof(runtime)) {}

  ~transaction_commit_guard() noexcept {
    if (transaction_ != nullptr) {
      const bool outermost = transaction_->outermost();
      transaction_->commit();
      if (outermost) {
        runtime_->seal();
      }
    }
  }

  void cancel() noexcept { transaction_ = nullptr; }

private:
  Transaction *transaction_;
  Runtime *runtime_;
};

} // namespace detail

template <typename Runtime, typename Fn>
DINGO_NOINLINE decltype(auto) execute_transaction(Runtime &runtime, Fn &&fn) {
  using allocator_type = typename Runtime::allocator_type;
  using context_type = runtime_context<allocator_type>;
  inline_arena<DINGO_CONTEXT_ARENA_BUFFER_SIZE> scratch;
  runtime_transaction<allocator_type> transaction(runtime, scratch);
  context_type context(scratch, transaction);
  detail::transaction_commit_guard commit(transaction, runtime);

  try {
    if constexpr (std::is_void_v<std::invoke_result_t<Fn, context_type &>>) {
      std::forward<Fn>(fn)(context);
    } else {
      return std::forward<Fn>(fn)(context);
    }
  } catch (...) {
    commit.cancel();
    throw;
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
  detail::transaction_commit_guard commit(transaction, runtime);

  try {
    if constexpr (std::is_void_v<result_type>) {
      std::forward<Fn>(fn)(context);
    } else {
      return std::forward<Fn>(fn)(context);
    }
  } catch (...) {
    commit.cancel();
    throw;
  }
}

#ifdef _MSC_VER
#pragma warning(pop)
#endif

} // namespace dingo
