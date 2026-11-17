//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/context/resolution.h>
#include <dingo/memory/object_layout.h>
#include <dingo/memory/object_store.h>
#include <dingo/runtime/transaction.h>

#include <cassert>
#include <cstddef>
#include <exception>
#include <memory>
#include <optional>
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

  void defer_rollback(void (*fn)(void *, void *) noexcept, void *first,
                      void *second = nullptr) {
    transaction_->defer_rollback(fn, first, second);
  }

  template <typename Runtime> bool owns(const Runtime &runtime) const noexcept {
    return transaction_->owns(runtime);
  }

  transaction_scope use_transaction(transaction_type &transaction) {
    return transaction_scope(*this, transaction);
  }

  void defer_reset(void (*fn)(void *, void *) noexcept, void *cell,
                   void *argument, void (*reset)(void *) noexcept) {
    transaction_->defer_reset(fn, cell, argument, reset);
  }

protected:
  template <typename T, typename ConstructFn>
  T &construct_storage(construction_scope scope, ConstructFn &&construct_fn) {
    if (scope.is_persistent()) {
      return *transaction_->template construct_persistent_made<T>(
          std::forward<ConstructFn>(construct_fn));
    }

    return ephemeral_store_.template place<T>(
        *scratch_, std::forward<ConstructFn>(construct_fn));
  }

  template <typename ConstructFn>
  void *place_storage(construction_scope scope,
                      const detail::object_layout &layout,
                      ConstructFn &&construct_fn) {
    if (scope.is_persistent()) {
      return transaction_->place_persistent(
          layout, std::forward<ConstructFn>(construct_fn));
    }
    return ephemeral_store_.place(*scratch_, layout,
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
// transaction and the context of the request. The request commits the
// transaction once it is resolved; one an exception leaves is rolled back as
// the frame ends.
template <typename Runtime> class transaction_frame {
public:
  using allocator_type = typename Runtime::allocator_type;
  using context_type = runtime_context<allocator_type>;
  using transaction_type = runtime_transaction<allocator_type>;

  explicit transaction_frame(Runtime &runtime)
      : runtime_(runtime), transaction_(runtime, scratch_),
        context_(scratch_, transaction_) {}

  ~transaction_frame() = default;

  transaction_frame(const transaction_frame &) = delete;
  transaction_frame &operator=(const transaction_frame &) = delete;

  context_type &context() noexcept { return context_; }

  // Commits the transaction of a request that resolved, and seals the
  // runtime when it is the outermost one.
  void commit() noexcept {
    const bool outermost = transaction_.outermost();
    transaction_.commit();
    if (outermost) {
      runtime_.seal();
    }
  }

  // Runs fn of the context, and commits the transaction once it returns
  // what it makes, which is returned as it is made: an exception that leaves
  // fn leaves the transaction to roll back as the frame ends.
  template <typename Fn> decltype(auto) run(Fn &&fn) {
    struct committing {
      transaction_frame &frame;
      bool armed = true;
      ~committing() {
        if (armed) {
          frame.commit();
        }
      }
    } guard{*this};
    try {
      return std::forward<Fn>(fn)(context_);
    } catch (...) {
      guard.armed = false;
      throw;
    }
  }

private:
  Runtime &runtime_;
  inline_arena<DINGO_CONTEXT_ARENA_BUFFER_SIZE> scratch_;
  transaction_type transaction_;
  context_type context_;
};

// The frame of a request of a runtime in the context of a request: a
// transaction of the runtime, which the context uses while the frame lives,
// when the context is of another runtime; none when it is of this one.
template <typename Runtime, typename Allocator> class nested_transaction_frame {
public:
  using context_type = runtime_context<Allocator>;
  using transaction_type = runtime_transaction<Allocator>;

  DINGO_NOINLINE nested_transaction_frame(Runtime &runtime,
                                          context_type &context) {
    if (!context.owns(runtime)) {
      nested_.emplace(runtime, context);
    }
  }

  DINGO_NOINLINE ~nested_transaction_frame() = default;

  nested_transaction_frame(const nested_transaction_frame &) = delete;
  nested_transaction_frame &
  operator=(const nested_transaction_frame &) = delete;

private:
  struct nested {
    nested(Runtime &runtime, context_type &context)
        : transaction_(runtime, scratch_),
          scope_(context.use_transaction(transaction_)),
          commit_(transaction_, runtime) {}

    inline_arena<DINGO_CONTEXT_ARENA_BUFFER_SIZE> scratch_;
    transaction_type transaction_;
    typename runtime_context_backend<Allocator>::transaction_scope scope_;
    transaction_commit_guard<transaction_type, Runtime> commit_;
  };
  std::optional<nested> nested_;
};
} // namespace detail

template <typename Runtime, typename Fn>
DINGO_NOINLINE decltype(auto) execute_transaction(Runtime &runtime, Fn &&fn) {
  detail::transaction_frame<Runtime> frame(runtime);
  return frame.run(std::forward<Fn>(fn));
}

#ifdef _MSC_VER
#pragma warning(pop)
#endif

} // namespace dingo
