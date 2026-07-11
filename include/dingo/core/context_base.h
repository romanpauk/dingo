//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/core/exceptions.h>
#include <dingo/core/key.h>
#include <dingo/factory/constructor_detection.h>
#include <dingo/memory/aligned_storage.h>
#include <dingo/registration/annotated.h>
#include <dingo/resolution/resolving_frame_fwd.h>

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
template <typename StaticRegistry, bool RuntimeDependencies>
class basic_static_context;

namespace detail {

template <typename T> struct is_basic_static_context : std::false_type {};

template <typename StaticRegistry, bool RuntimeDependencies>
struct is_basic_static_context<
    basic_static_context<StaticRegistry, RuntimeDependencies>>
    : std::true_type {};

template <typename T>
inline constexpr bool is_basic_static_context_v = is_basic_static_context<
    std::remove_cv_t<std::remove_reference_t<T>>>::value;

struct context_destructible {
  void *instance;
  void (*dtor)(void *);
};

template <typename T, typename Context, typename Container>
T resolve_context_request(Context &context, Container &container) {
  if constexpr (is_selected_v<T>) {
    using request_type = selected_type_t<T>;
    using selector_type = selected_selector_t<T>;
    if constexpr (is_type_selector_v<selector_type>) {
      using selector_key_type = type_selector_type_t<selector_type>;
      return T(container.template resolve<request_type, false>(
          context,
          detail::make_lookup_key(type_selector<selector_key_type>{})));
    } else {
      static_assert(is_value_selector_v<selector_type>,
                    "dingo::dependency<T, Selector> requires "
                    "dingo::key_type<Key> or dingo::key_type<Key, Value>");
      return T(container.template resolve<request_type, false>(
          context, detail::make_lookup_key(selector_type{})));
    }
  } else {
    return container.template resolve<T, false>(context,
                                                detail::no_lookup_key());
  }
}

struct static_context_frame_base {
  virtual ~static_context_frame_base() = default;
  virtual void reset() = 0;
  virtual void add_destructor(void *instance, void (*dtor)(void *)) = 0;
  virtual void *try_allocate_temporary(std::size_t size,
                                       std::size_t alignment) = 0;
};

template <std::size_t DestructibleCapacity,
          std::size_t TemporarySlotCapacity = 0,
          std::size_t TemporarySlotSize = 1,
          std::size_t TemporarySlotAlign = alignof(std::max_align_t)>
struct static_context_frame {
  static constexpr std::size_t destructible_capacity_ = DestructibleCapacity;
  static constexpr std::size_t temporary_slot_capacity_ = TemporarySlotCapacity;
  static constexpr std::size_t temporary_storage_capacity_ =
      temporary_slot_capacity_ == 0 ? 1 : temporary_slot_capacity_;

  static_context_frame() = default;
  ~static_context_frame() { reset(); }

  static_context_frame(const static_context_frame &) = delete;
  static_context_frame &operator=(const static_context_frame &) = delete;

  void reset() {
    while (destructible_count_ != 0) {
      auto &destructible = destructibles_[--destructible_count_];
      destructible.dtor(destructible.instance);
    }
    temporary_count_ = 0;
  }

  void add_destructor(void *instance, void (*dtor)(void *)) {
    assert(destructible_count_ < destructible_capacity_);
    destructibles_[destructible_count_++] = {instance, dtor};
  }

  void *try_allocate_temporary(std::size_t size, std::size_t alignment) {
    if constexpr (temporary_slot_capacity_ == 0) {
      return nullptr;
    } else {
      if (size > TemporarySlotSize || alignment > TemporarySlotAlign) {
        return nullptr;
      }
      if (temporary_count_ >= temporary_slot_capacity_) {
        return nullptr;
      }
      return &temporary_slots_[temporary_count_++];
    }
  }

  template <typename T> T *try_allocate_temporary() {
    return reinterpret_cast<T *>(try_allocate_temporary(sizeof(T), alignof(T)));
  }

  std::array<context_destructible, destructible_capacity_> destructibles_{};
  std::size_t destructible_count_ = 0;
  std::array<aligned_storage_t<TemporarySlotSize, TemporarySlotAlign>,
             temporary_storage_capacity_>
      temporary_slots_{};
  std::size_t temporary_count_ = 0;
};

template <std::size_t DestructibleCapacity,
          std::size_t TemporarySlotCapacity = 0,
          std::size_t TemporarySlotSize = 1,
          std::size_t TemporarySlotAlign = alignof(std::max_align_t)>
struct fixed_static_context_frame : static_context_frame_base {
  static constexpr std::size_t destructible_capacity_ = DestructibleCapacity;
  static constexpr std::size_t temporary_slot_capacity_ = TemporarySlotCapacity;
  static constexpr std::size_t temporary_storage_capacity_ =
      temporary_slot_capacity_ == 0 ? 1 : temporary_slot_capacity_;

  fixed_static_context_frame() = default;
  ~fixed_static_context_frame() { reset_impl(); }

  fixed_static_context_frame(const fixed_static_context_frame &) = delete;
  fixed_static_context_frame &
  operator=(const fixed_static_context_frame &) = delete;

  void reset() override { reset_impl(); }

  void reset_impl() {
    while (destructible_count_ != 0) {
      auto &destructible = destructibles_[--destructible_count_];
      destructible.dtor(destructible.instance);
    }
    temporary_count_ = 0;
  }

  void add_destructor(void *instance, void (*dtor)(void *)) override {
    assert(destructible_count_ < destructible_capacity_);
    destructibles_[destructible_count_++] = {instance, dtor};
  }

  void *try_allocate_temporary(std::size_t size,
                               std::size_t alignment) override {
    if constexpr (temporary_slot_capacity_ == 0) {
      return nullptr;
    } else {
      if (size > TemporarySlotSize || alignment > TemporarySlotAlign) {
        return nullptr;
      }
      if (temporary_count_ >= temporary_slot_capacity_) {
        return nullptr;
      }
      return &temporary_slots_[temporary_count_++];
    }
  }

  template <typename T> T *try_allocate_temporary() {
    return reinterpret_cast<T *>(try_allocate_temporary(sizeof(T), alignof(T)));
  }

  std::array<context_destructible, destructible_capacity_> destructibles_{};
  std::size_t destructible_count_ = 0;
  std::array<aligned_storage_t<TemporarySlotSize, TemporarySlotAlign>,
             temporary_storage_capacity_>
      temporary_slots_{};
  std::size_t temporary_count_ = 0;
};

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
} // namespace dingo
