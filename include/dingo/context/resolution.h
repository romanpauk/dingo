//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/base/construction_scope.h>
#include <dingo/base/exceptions.h>
#include <dingo/factory/constructor_detection.h>
#include <dingo/lookup/lookup_key.h>
#include <dingo/memory/aligned_storage.h>
#include <dingo/type/annotated.h>

#include <array>
#include <cassert>
#include <cstddef>
#include <type_traits>
#include <utility>
#include <vector>

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4702)
#endif

namespace dingo {
namespace detail {

template <typename T> struct is_runtime_context : std::false_type {};

template <typename T>
inline constexpr bool is_runtime_context_v =
    is_runtime_context<std::remove_cv_t<std::remove_reference_t<T>>>::value;

// Whether a request that a static binding cannot resolve is rejected as it
// runs, as a runtime container rejects it, rather than as it compiles: in a
// runtime context, and in one a backend that resolves for a runtime container
// declares with rejects_at_run_time.
template <typename T> struct rejects_at_run_time : std::false_type {};

template <typename T>
inline constexpr bool rejects_at_run_time_v =
    rejects_at_run_time<std::remove_cv_t<std::remove_reference_t<T>>>::value;

class context_path_state;

// A frame of the resolution path: the type being constructed, linked to the
// frame of the type whose construction requested it.
class resolving_frame {
public:
  resolving_frame(context_path_state &context, type_descriptor type);

  resolving_frame(const resolving_frame &) = delete;
  resolving_frame &operator=(const resolving_frame &) = delete;
  resolving_frame(resolving_frame &&) = delete;
  resolving_frame &operator=(resolving_frame &&) = delete;

  ~resolving_frame();

private:
  friend class context_path_state;

  context_path_state *context_;
  resolving_frame *parent_ = nullptr;
  type_descriptor type_;
};

class context_path_state {
public:
  template <typename T> detail::resolving_frame track_type();

  bool has_type_path() const;

  const type_descriptor *active_type() const;

  void append_type_path(std::string &message) const;

protected:
  friend class resolving_frame;

  detail::resolving_frame *active_resolving_frame_ = nullptr;
};

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdangling-pointer"
#endif
inline resolving_frame::resolving_frame(context_path_state &context,
                                        type_descriptor type)
    : context_(&context), parent_(context.active_resolving_frame_),
      type_(type) {
  // The frame object itself is the linked stack node for the active
  // resolution path, so entering the scope just makes this the new head.
  // GCC can warn here when the frame lives inside a stack RAII guard even
  // though the destructor restores parent_ before that guard leaves scope.
  context_->active_resolving_frame_ = this;
}
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif

inline resolving_frame::~resolving_frame() {
  if (context_ != nullptr) {
    // Frames unwind strictly LIFO; restoring parent_ pops this node from
    // the active type path without touching any arena-backed state.
    assert(context_->active_resolving_frame_ == this);
    context_->active_resolving_frame_ = parent_;
  }
}

inline bool context_path_state::has_type_path() const {
  return active_resolving_frame_ != nullptr;
}

inline const type_descriptor *context_path_state::active_type() const {
  return active_resolving_frame_ != nullptr ? &active_resolving_frame_->type_
                                            : nullptr;
}

inline void context_path_state::append_type_path(std::string &message) const {
  std::vector<type_descriptor> names;
  for (auto *frame = active_resolving_frame_; frame != nullptr;
       frame = frame->parent_) {
    names.emplace_back(frame->type_);
  }

  for (auto it = names.rbegin(); it != names.rend(); ++it) {
    if (it != names.rbegin()) {
      message += " -> ";
    }
    append_type_name(message, *it);
  }
}

template <typename T> detail::resolving_frame context_path_state::track_type() {
  return detail::resolving_frame(*this, detail::described_type_v<T>);
}

} // namespace detail

// The resolution context of both containers. The backend provides the memory
// of the instances a resolution constructs, construct_storage<T>(scope, fn),
// and on_rollback, the hook a transactional resolution reports to; a backend
// without a transaction ignores it. A runtime backend also keeps the
// destructors a resolution adds with add_runtime_destructor.
template <typename Backend>
class resolution_context : public detail::context_path_state, public Backend {
public:
  using Backend::Backend;

  resolution_context() = default;
  resolution_context(const resolution_context &) = delete;
  resolution_context &operator=(const resolution_context &) = delete;

  template <typename T, typename Scope, typename... Args>
  T &construct(Scope scope, Args &&...args) {
    return Backend::template construct_storage<T>(
        scope, [&](void *ptr) { new (ptr) T(std::forward<Args>(args)...); });
  }

  // Constructs a T at the address construct_fn is given.
  template <typename T, typename Scope, typename ConstructFn>
  T &construct_in_place(Scope scope, ConstructFn &&construct_fn) {
    return Backend::template construct_storage<T>(
        scope, std::forward<ConstructFn>(construct_fn));
  }

  // Constructs an object of a type known only by its size and its alignment
  // at the address construct_fn is given, which dtor destroys; none where
  // it has no destructor. Only a backend that places such objects has it.
  template <typename Scope, typename ConstructFn>
  void *construct_erased(Scope scope, std::size_t size, std::size_t alignment,
                         void (*dtor)(void *) noexcept,
                         ConstructFn &&construct_fn) {
    return Backend::construct_erased_storage(
        scope, size, alignment, dtor, std::forward<ConstructFn>(construct_fn));
  }

  // Constructs the T that make returns in place, so that a T that can be
  // neither copied nor moved is placed too.
  template <typename T, typename Scope, typename Make>
  T &construct_from(Scope scope, Make &&make) {
    return Backend::template construct_storage<T>(
        scope, [&](void *ptr) { new (ptr) T(std::forward<Make>(make)()); });
  }

  template <typename T, typename DetectionMode, typename Scope,
            typename Container>
  T construct(Scope scope, Container &container) {
    using temporary_type = normalized_type_t<T>;
    auto &instance = Backend::template construct_storage<temporary_type>(
        scope, [&](void *ptr) {
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

  template <typename T, typename Scope, typename Container>
  T resolve(Scope scope, Container &container) {
    if constexpr (detail::is_selected_v<T>) {
      using request_type = detail::selected_type_t<T>;
      using selector_type = detail::selected_selector_t<T>;
      auto resolve_selected = [&]() -> request_type {
        if constexpr (detail::is_type_selector_v<selector_type>) {
          using selector_key_type = detail::type_selector_type_t<selector_type>;
          return container.template resolve<request_type, false>(
              scope, *this,
              detail::make_lookup_key(
                  detail::type_selector<selector_key_type>{}));
        } else {
          static_assert(detail::is_value_selector_v<selector_type>);
          return container.template resolve<request_type, false>(
              scope, *this, detail::make_lookup_key(selector_type{}));
        }
      };
      if constexpr (!std::is_reference_v<request_type> &&
                    (std::is_move_constructible_v<request_type> ||
                     std::is_copy_constructible_v<request_type>)) {
        return detail::emplace_result<T>(resolve_selected);
      } else {
        return T(resolve_selected());
      }
    } else {
      // An annotated value is made in place of what its interface resolves.
      return detail::emplace_result<T>([&]() -> decltype(auto) {
        return container.template resolve<T, false>(scope, *this,
                                                    detail::no_lookup_key());
      });
    }
  }
};

namespace detail {
template <typename Backend>
struct is_runtime_context<resolution_context<Backend>>
    : std::bool_constant<Backend::is_transactional> {};

template <typename Backend, typename = void>
struct backend_rejects_at_run_time
    : std::bool_constant<Backend::is_transactional> {};

template <typename Backend>
struct backend_rejects_at_run_time<
    Backend, std::void_t<decltype(Backend::rejects_at_run_time)>>
    : std::bool_constant<Backend::rejects_at_run_time> {};

template <typename Backend>
struct rejects_at_run_time<resolution_context<Backend>>
    : backend_rejects_at_run_time<Backend> {};

} // namespace detail

#ifdef _MSC_VER
#pragma warning(pop)
#endif

} // namespace dingo
