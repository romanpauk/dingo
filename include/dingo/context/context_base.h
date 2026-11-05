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
#include <dingo/memory/aligned_storage.h>
#include <dingo/resolution/resolving_frame_fwd.h>
#include <dingo/type/annotated.h>
#include <dingo/type/key.h>

#include <array>
#include <cassert>
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

class context_path_state {
public:
  template <typename T> detail::resolving_frame track_type();

  bool has_type_path() const;

  const type_descriptor *active_type() const;

  const type_descriptor *parent_type() const;

  void append_type_path(std::string &message) const;

protected:
  friend class resolving_frame;

  detail::resolving_frame *active_resolving_frame_ = nullptr;
};

} // namespace detail
} // namespace dingo

#ifdef _MSC_VER
#pragma warning(pop)
#endif

#include <dingo/resolution/resolving_frame.h>

namespace dingo {
namespace detail {

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

inline const type_descriptor *context_path_state::parent_type() const {
  return active_resolving_frame_ != nullptr &&
                 active_resolving_frame_->parent_ != nullptr
             ? &active_resolving_frame_->parent_->type_
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
  return detail::resolving_frame(*this, describe_type<T>());
}

} // namespace detail
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4702)
#endif

// The resolution context of both containers. The backend provides the memory
// of the instances a resolution constructs, construct_storage<T>(scope, fn),
// and the hooks a transactional resolution reports to: on_rollback and
// add_runtime_destructor. A backend without a transaction ignores them.
template <typename Backend>
class resolution_context : public detail::context_path_state, public Backend {
public:
  using backend_type = Backend;
  using Backend::Backend;

  resolution_context() = default;
  resolution_context(const resolution_context &) = delete;
  resolution_context &operator=(const resolution_context &) = delete;

  template <typename T, typename... Args>
  T &construct(construction_scope scope, Args &&...args) {
    return Backend::template construct_storage<T>(
        scope, [&](void *ptr) { new (ptr) T(std::forward<Args>(args)...); });
  }

  template <typename T, typename DetectionMode, typename Container>
  T construct(construction_scope scope, Container &container) {
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
};

namespace detail {
template <typename Backend>
struct is_runtime_context<resolution_context<Backend>>
    : std::bool_constant<Backend::is_transactional> {};

} // namespace detail

#ifdef _MSC_VER
#pragma warning(pop)
#endif

} // namespace dingo
