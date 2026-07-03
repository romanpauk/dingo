//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/core/context_base.h>
#include <dingo/memory/aligned_storage.h>

namespace dingo::detail {

template <typename T,
          bool DefaultConstructible = std::is_default_constructible_v<T>>
struct recursion_guard {
  template <typename Context>
  explicit recursion_guard(Context &context, const void *binding)
      : frame_guard_(context.template track_type<T>()), binding_(binding),
        prev_(head_) {
    // Track the active type path first so recursion exceptions can report
    // the full resolution chain, including the repeated type. Recursion is
    // keyed by the binding instance: re-entering the same binding on this
    // thread is a cycle, while an independent resolution of the same type
    // through a different binding (e.g. a factory delegating to another
    // container, or a child overriding and wrapping a parent binding) is
    // legitimate.
    for (const recursion_guard *guard = prev_; guard != nullptr;
         guard = guard->prev_) {
      if (guard->binding_ == binding_) {
        throw detail::make_type_recursion_exception<T>(context);
      }
    }
    head_ = this;
  }

  recursion_guard(const recursion_guard &) = delete;
  recursion_guard &operator=(const recursion_guard &) = delete;
  recursion_guard(recursion_guard &&) = delete;
  recursion_guard &operator=(recursion_guard &&) = delete;

  ~recursion_guard() { head_ = prev_; }

private:
  detail::resolving_frame frame_guard_;
  const void *binding_;
  recursion_guard *prev_;
  // Intrusive per-type stack of guards active on this thread; guards nest
  // strictly LIFO with the resolution stack.
  static thread_local recursion_guard *head_;
};

template <typename T, bool DefaultConstructible>
thread_local recursion_guard<T, DefaultConstructible>
    *recursion_guard<T, DefaultConstructible>::head_ = nullptr;

template <typename T> struct recursion_guard<T, true> {
  template <typename Context>
  explicit recursion_guard(Context &, const void *) {}

  recursion_guard(const recursion_guard &) = delete;
  recursion_guard &operator=(const recursion_guard &) = delete;
  recursion_guard(recursion_guard &&) = delete;
  recursion_guard &operator=(recursion_guard &&) = delete;
};

template <typename T> class recursion_guard_wrapper {
public:
  template <typename Context>
  recursion_guard_wrapper(Context &context, const void *binding, bool enabled) {
    if (enabled) {
      // The wrapped recursion_guard owns a self-linking resolving_frame and
      // links itself into the per-type guard stack, so it must stay at a
      // fixed address for the whole guarded scope.
      new (&storage_) recursion_guard<T>(context, binding);
      active_ = true;
    }
  }

  recursion_guard_wrapper(const recursion_guard_wrapper &) = delete;
  recursion_guard_wrapper &operator=(const recursion_guard_wrapper &) = delete;
  recursion_guard_wrapper(recursion_guard_wrapper &&) = delete;
  recursion_guard_wrapper &operator=(recursion_guard_wrapper &&) = delete;

  ~recursion_guard_wrapper() {
    if (active_) {
      get()->~recursion_guard<T>();
    }
  }

private:
  recursion_guard<T> *get() {
    return reinterpret_cast<recursion_guard<T> *>(&storage_);
  }

  aligned_storage_t<sizeof(recursion_guard<T>), alignof(recursion_guard<T>)>
      storage_;
  bool active_ = false;
};

} // namespace dingo::detail
