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

#include <memory>
#include <type_traits>
#include <utility>

namespace dingo {

template <typename Allocator>
class runtime_context : public detail::context_path_state {
  using transaction_type = runtime_transaction<Allocator>;

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

  runtime_context(arena<> &scratch, transaction_type &transaction)
      : scratch_(std::addressof(scratch)),
        transaction_(std::addressof(transaction)) {}

  ~runtime_context() noexcept { ephemeral_store_.destroy(*scratch_); }

  runtime_context(const runtime_context &) = delete;
  runtime_context &operator=(const runtime_context &) = delete;

  template <typename T, typename... Args>
  T &construct(construction_scope scope, Args &&...args) {
    return construct_with_scope<T>(
        scope, [&](void *ptr) { new (ptr) T(std::forward<Args>(args)...); });
  }

// Resolution selects failure paths at runtime. For some instantiations MSVC
// proves construction cannot return and reports the following delivery code as
// unreachable even though the instantiated path is not selected.
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4702)
#endif
  template <typename T, typename DetectionMode, typename Container>
  T construct(construction_scope scope, Container &container) {
    using temporary_type = normalized_type_t<T>;
    auto &instance =
        construct_with_scope<temporary_type>(scope, [&](void *ptr) {
          constructor_detection<temporary_type, DetectionMode>()
              .template construct<temporary_type>(ptr, scope, *this, container);
        });

    if constexpr (std::is_lvalue_reference_v<T>) {
      return instance;
    } else if constexpr (!std::is_rvalue_reference_v<T> &&
                         std::is_copy_constructible_v<temporary_type> &&
                         !std::is_move_constructible_v<temporary_type>) {
      return static_cast<const temporary_type &>(instance);
    } else {
      return std::move(instance);
    }
  }
#ifdef _MSC_VER
#pragma warning(pop)
#endif

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

  template <typename T, typename Container>
  T resolve(construction_scope scope, Container &container) {
    if constexpr (detail::is_selected_v<T>) {
      using request_type = detail::selected_type_t<T>;
      using selector_type = detail::selected_selector_t<T>;
      if constexpr (detail::is_type_selector_v<selector_type>) {
        using selector_key_type = detail::type_selector_type_t<selector_type>;
        return T(container.template resolve<request_type, false>(
            scope, *this,
            detail::make_lookup_key(
                detail::type_selector<selector_key_type>{})));
      } else {
        static_assert(detail::is_value_selector_v<selector_type>);
        return T(container.template resolve<request_type, false>(
            scope, *this, detail::make_lookup_key(selector_type{})));
      }
    } else {
      return container.template resolve<T, false>(scope, *this,
                                                  detail::no_lookup_key());
    }
  }

private:
  template <typename T, typename ConstructFn>
  T &construct_with_scope(construction_scope scope,
                          ConstructFn &&construct_fn) {
    if (scope.is_persistent()) {
      return *transaction_->template construct_persistent_with<T>(
          std::forward<ConstructFn>(construct_fn));
    }

    return ephemeral_store_.template construct_at<T>(
        *scratch_, std::forward<ConstructFn>(construct_fn));
  }

  arena<> *scratch_;
  detail::object_store<arena<>> ephemeral_store_;
  transaction_type *transaction_;
};

namespace detail {

// execute_transaction is specialized for each callback type. Keep frame
// setup and teardown in Runtime-only functions so those specializations share
// the generated code.
template <typename Runtime> class transaction_frame {
public:
  using allocator_type = typename Runtime::allocator_type;
  using context_type = runtime_context<allocator_type>;

  DINGO_NOINLINE explicit transaction_frame(Runtime &runtime)
      : transaction_frame_(runtime),
        context_(transaction_frame_.scratch(),
                 transaction_frame_.transaction()) {}

  DINGO_NOINLINE ~transaction_frame() noexcept {}

  transaction_frame(const transaction_frame &) = delete;
  transaction_frame &operator=(const transaction_frame &) = delete;

  context_type &context() noexcept { return context_; }
  void commit() noexcept { transaction_frame_.commit(); }

private:
  runtime_transaction_frame<Runtime> transaction_frame_;
  context_type context_;
};

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4702)
#endif

template <typename Transaction> class transaction_commit_guard {
public:
  explicit transaction_commit_guard(Transaction &transaction) noexcept
      : transaction_(std::addressof(transaction)) {}

  ~transaction_commit_guard() noexcept {
    if (transaction_ != nullptr) {
      transaction_->commit();
    }
  }

  void cancel() noexcept { transaction_ = nullptr; }

private:
  Transaction *transaction_;
};

} // namespace detail

template <typename Runtime, typename Fn>
DINGO_NOINLINE decltype(auto) execute_transaction(Runtime &runtime, Fn &&fn) {
  detail::transaction_frame<Runtime> frame(runtime);
  auto &context = frame.context();
  using result_type = decltype(std::forward<Fn>(fn)(context));
  // Void and reference results cannot fail after the callback returns, so they
  // avoid emitting a catch path for every transaction instantiation.
  if constexpr (std::is_void_v<result_type>) {
    std::forward<Fn>(fn)(context);
    frame.commit();
  } else if constexpr (std::is_reference_v<result_type>) {
    auto &&result = std::forward<Fn>(fn)(context);
    frame.commit();
    return static_cast<result_type>(result);
  } else {
    detail::transaction_commit_guard commit(frame);
    try {
      return std::forward<Fn>(fn)(context);
    } catch (...) {
      commit.cancel();
      throw;
    }
  }
}

template <typename Runtime, typename Allocator, typename Fn>
auto execute_transaction(Runtime &runtime, runtime_context<Allocator> &context,
                         Fn &&fn) -> decltype(std::forward<Fn>(fn)(context)) {
  using result_type = decltype(std::forward<Fn>(fn)(context));
  assert(!context.owns(runtime));

  inline_arena<DINGO_CONTEXT_ARENA_BUFFER_SIZE> scratch;
  runtime_transaction<Allocator> transaction(runtime, scratch);
  auto scope = context.use_transaction(transaction);
  // Void and reference results cannot fail after the callback returns, so they
  // avoid emitting a catch path for every transaction instantiation.
  if constexpr (std::is_void_v<result_type>) {
    std::forward<Fn>(fn)(context);
    transaction.commit();
  } else if constexpr (std::is_reference_v<result_type>) {
    auto &&result = std::forward<Fn>(fn)(context);
    transaction.commit();
    return static_cast<result_type>(result);
  } else {
    detail::transaction_commit_guard commit(transaction);
    try {
      return std::forward<Fn>(fn)(context);
    } catch (...) {
      commit.cancel();
      throw;
    }
  }
}

#ifdef _MSC_VER
#pragma warning(pop)
#endif

} // namespace dingo
