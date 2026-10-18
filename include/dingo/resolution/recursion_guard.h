//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/core/context_base.h>
#include <dingo/core/factory_traits.h>
#include <dingo/factory/constructor.h>
#include <dingo/memory/aligned_storage.h>

namespace dingo {

template <typename StaticRegistry> class static_context;

namespace detail {

struct no_dependency_context;

// Constructors are the only factories that provably cannot recurse when they
// have no dependencies; a callable or function can call back into a container.
template <typename Factory> inline constexpr bool is_constructor_v = false;

template <typename... Ts>
inline constexpr bool is_constructor_v<constructor<Ts...>> = true;

// Static containers reject cycles through declared dependencies at compile
// time, so a runtime guard is needed there only when the factory's dependencies
// are unknown (auto-detected constructors). A context that cannot resolve
// dependencies is only used with factories that have none.
template <typename Context>
inline constexpr bool checks_declared_cycles_v = false;

template <typename StaticRegistry>
inline constexpr bool checks_declared_cycles_v<static_context<StaticRegistry>> =
    true;

template <>
inline constexpr bool checks_declared_cycles_v<no_dependency_context> = true;

template <typename Factory, typename Context>
inline constexpr bool recursion_guard_enabled_v =
    checks_declared_cycles_v<Context>
        ? std::is_void_v<typename factory_traits<Factory>::dependencies>
        : !(is_constructor_v<Factory> &&
            factory_without_dependencies_v<Factory>);

template <typename T> struct recursion_guard {
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

template <typename T>
thread_local recursion_guard<T> *recursion_guard<T>::head_ = nullptr;

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

} // namespace detail
} // namespace dingo
