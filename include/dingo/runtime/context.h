//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/context/resolution.h>
#include <dingo/memory/object_store.h>
#include <dingo/runtime/transaction.h>

#include <cassert>
#include <cstddef>
#include <exception>
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

  template <typename ConstructFn>
  void *construct_erased_storage(construction_scope scope, std::size_t size,
                                 std::size_t alignment,
                                 void (*dtor)(void *) noexcept,
                                 ConstructFn &&construct_fn) {
    if (scope.is_persistent()) {
      return transaction_->construct_persistent_erased(
          size, alignment, dtor, std::forward<ConstructFn>(construct_fn));
    }
    return ephemeral_store_.construct_erased(
        *scratch_, size, alignment, dtor,
        std::forward<ConstructFn>(construct_fn));
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

// Commits a resolve transaction unless an exception leaves the resolve; the
// transaction then rolls back. The outermost successful resolve of a runtime
// seals it.
template <typename Transaction, typename Runtime>
class transaction_commit_guard {
public:
  transaction_commit_guard(Transaction &transaction, Runtime &runtime) noexcept
      : transaction_(transaction), runtime_(runtime),
        exceptions_(std::uncaught_exceptions()) {}

  ~transaction_commit_guard() noexcept {
    if (std::uncaught_exceptions() == exceptions_) {
      const bool outermost = transaction_.outermost();
      transaction_.commit();
      if (outermost) {
        runtime_.seal();
      }
    }
  }

private:
  Transaction &transaction_;
  Runtime &runtime_;
  int exceptions_;
};

} // namespace detail

namespace detail {
// The frame of a transaction a request opens: its scratch memory, the
// transaction, the context of the request, and the guard that commits the
// transaction unless an exception leaves the request. It is made and
// destroyed out of line, once for a runtime, not for every request.
template <typename Runtime> class transaction_frame {
public:
  using allocator_type = typename Runtime::allocator_type;
  using context_type = runtime_context<allocator_type>;
  using transaction_type = runtime_transaction<allocator_type>;

  DINGO_NOINLINE explicit transaction_frame(Runtime &runtime)
      : transaction_(runtime, scratch_), context_(scratch_, transaction_),
        commit_(transaction_, runtime) {}

  DINGO_NOINLINE ~transaction_frame() = default;

  transaction_frame(const transaction_frame &) = delete;
  transaction_frame &operator=(const transaction_frame &) = delete;

  context_type &context() noexcept { return context_; }

private:
  inline_arena<DINGO_CONTEXT_ARENA_BUFFER_SIZE> scratch_;
  transaction_type transaction_;
  context_type context_;
  transaction_commit_guard<transaction_type, Runtime> commit_;
};

// The frame of a transaction a request opens in the context of a request of
// another runtime: the context uses the transaction while the frame lives.
template <typename Runtime, typename Allocator> class nested_transaction_frame {
public:
  using context_type = runtime_context<Allocator>;
  using transaction_type = runtime_transaction<Allocator>;

  DINGO_NOINLINE nested_transaction_frame(Runtime &runtime,
                                          context_type &context)
      : transaction_(runtime, scratch_),
        scope_(context.use_transaction(transaction_)),
        commit_(transaction_, runtime) {}

  DINGO_NOINLINE ~nested_transaction_frame() = default;

  nested_transaction_frame(const nested_transaction_frame &) = delete;
  nested_transaction_frame &
  operator=(const nested_transaction_frame &) = delete;

private:
  inline_arena<DINGO_CONTEXT_ARENA_BUFFER_SIZE> scratch_;
  transaction_type transaction_;
  typename runtime_context_backend<Allocator>::transaction_scope scope_;
  transaction_commit_guard<transaction_type, Runtime> commit_;
};
} // namespace detail

template <typename Runtime, typename Fn>
DINGO_NOINLINE decltype(auto) execute_transaction(Runtime &runtime, Fn &&fn) {
  detail::transaction_frame<Runtime> frame(runtime);
  return std::forward<Fn>(fn)(frame.context());
}

template <typename Runtime, typename Allocator, typename Fn>
auto execute_transaction(Runtime &runtime, runtime_context<Allocator> &context,
                         Fn &&fn)
    -> std::invoke_result_t<Fn, runtime_context<Allocator> &> {
  assert(!context.owns(runtime));
  detail::nested_transaction_frame<Runtime, Allocator> frame(runtime, context);
  return std::forward<Fn>(fn)(context);
}

#ifdef _MSC_VER
#pragma warning(pop)
#endif

} // namespace dingo
