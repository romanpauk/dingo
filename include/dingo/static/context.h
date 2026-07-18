//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/core/context_base.h>
#include <dingo/static/graph.h>

#include <array>
#include <cassert>
#include <cstddef>
#include <new>

namespace dingo {

template <typename StaticRegistry, bool RuntimeDependencies>
class basic_static_context;

namespace detail {

template <typename StaticRegistry, bool RuntimeDependencies>
struct static_context_frame_choice;

template <typename StaticRegistry>
struct static_context_frame_choice<StaticRegistry, false> {
  using execution_traits =
      detail::basic_static_execution_traits<StaticRegistry, false>;
  using type =
      detail::static_context_frame<execution_traits::max_destructible_slots,
                                   execution_traits::max_temporary_slots,
                                   execution_traits::max_temporary_size == 0
                                       ? 1
                                       : execution_traits::max_temporary_size,
                                   execution_traits::max_temporary_align == 0
                                       ? alignof(std::max_align_t)
                                       : execution_traits::max_temporary_align>;
};

template <typename StaticRegistry>
struct static_context_frame_choice<StaticRegistry, true> {
  using execution_traits =
      detail::basic_static_execution_traits<StaticRegistry, true>;
  using type = detail::fixed_static_context_frame<
      execution_traits::max_destructible_slots,
      execution_traits::max_temporary_slots,
      execution_traits::max_temporary_size == 0
          ? 1
          : execution_traits::max_temporary_size,
      execution_traits::max_temporary_align == 0
          ? alignof(std::max_align_t)
          : execution_traits::max_temporary_align>;
};

template <typename StaticRegistry>
using binding_context = basic_static_context<StaticRegistry, true>;

} // namespace detail

template <typename StaticRegistry, bool RuntimeDependencies = false>
class basic_static_context : public detail::context_path_state {
  using execution_traits =
      detail::basic_static_execution_traits<StaticRegistry,
                                            RuntimeDependencies>;
  static constexpr std::size_t frame_capacity_ =
      execution_traits::max_retained_frame_depth + 1;
  static constexpr std::size_t destructible_capacity_ =
      execution_traits::max_destructible_slots;
  static constexpr std::size_t temporary_slot_capacity_ =
      execution_traits::max_temporary_slots;
  static constexpr std::size_t temporary_slot_size_ =
      execution_traits::max_temporary_size == 0
          ? 1
          : execution_traits::max_temporary_size;
  static constexpr std::size_t temporary_slot_align_ =
      execution_traits::max_temporary_align == 0
          ? alignof(std::max_align_t)
          : execution_traits::max_temporary_align;
  using frame_type =
      typename detail::static_context_frame_choice<StaticRegistry,
                                                   RuntimeDependencies>::type;
  // Pure static contexts keep concrete frame pointers so optimized static
  // resolution does not emit virtual reset/allocation code. Runtime-dependent
  // static resolution needs the base pointer because nested local bindings can
  // share activation frames across static context types.
  using frame_pointer_type =
      std::conditional_t<RuntimeDependencies,
                         detail::static_context_frame_base *, frame_type *>;

public:
  basic_static_context() { frames_[0] = &frame_; }

  ~basic_static_context() {
    while (frame_count_ != 0) {
      frames_[--frame_count_]->reset();
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

  template <typename T, typename DetectionMode, typename Container>
  T construct(construction_scope scope, Container &container) {
    using temporary_type = normalized_type_t<T>;

    auto *instance = allocate_temporary_storage<temporary_type>();
    constructor_detection<temporary_type, DetectionMode>()
        .template construct<temporary_type>(instance, scope, *this, container);
    if constexpr (!std::is_trivially_destructible_v<temporary_type>) {
      register_destructor(instance);
    }

    if constexpr (std::is_lvalue_reference_v<T>) {
      return *instance;
    } else {
      return std::move(*instance);
    }
  }

  template <typename T, typename... Args>
  T &construct(construction_scope, Args &&...args) {
    auto *instance = allocate_temporary_storage<T>();
    new (instance) T(std::forward<Args>(args)...);
    if constexpr (!std::is_trivially_destructible_v<T>) {
      register_destructor(instance);
    }
    return *instance;
  }

  template <typename T> T *allocate() {
    return allocate_temporary_storage<T>();
  }

  void push_frame(frame_type *frame) {
    assert(!contains_frame(frame));
    assert(frame_count_ < frames_.size());
    frames_[frame_count_++] = frame;
  }

  // Only runtime-dependent static contexts accept erased static frames.
  template <bool Enabled = RuntimeDependencies,
            std::enable_if_t<Enabled, int> = 0>
  void push_frame(detail::static_context_frame_base *frame) {
    assert(!contains_frame(frame));
    assert(frame_count_ < frames_.size());
    frames_[frame_count_++] = frame;
  }

  void pop_frame() {
    assert(frame_count_ != 0);
    --frame_count_;
  }

  bool contains_frame(frame_pointer_type candidate) const {
    for (std::size_t index = 0; index < frame_count_; ++index) {
      if (frames_[index] == candidate) {
        return true;
      }
    }
    return false;
  }

private:
  template <typename T> T *allocate_temporary_storage() {
    static_assert(sizeof(T) <= temporary_slot_size_,
                  "static_context temporary size must fit the compile-time "
                  "temporary slot bound");
    static_assert(alignof(T) <= temporary_slot_align_,
                  "static_context temporary alignment must fit the "
                  "compile-time temporary slot bound");
    static_assert(temporary_slot_capacity_ != 0,
                  "static_context requires at least one compile-time "
                  "temporary slot for this resolution path");

    if constexpr (RuntimeDependencies) {
      auto *fixed =
          active_frame().try_allocate_temporary(sizeof(T), alignof(T));
      assert(fixed != nullptr);
      return reinterpret_cast<T *>(fixed);
    } else {
      auto *fixed = active_frame().template try_allocate_temporary<T>();
      assert(fixed != nullptr);
      return fixed;
    }
  }

  auto &active_frame() {
    assert(frame_count_ != 0);
    return *frames_[frame_count_ - 1];
  }

  template <typename T> void register_destructor(T *instance) {
    static_assert(!std::is_trivially_destructible_v<T>);
    active_frame().add_destructor(instance, &destructor<T>);
  }

  template <typename T> static void destructor(void *ptr) {
    reinterpret_cast<T *>(ptr)->~T();
  }

  std::array<frame_pointer_type, frame_capacity_> frames_{};
  std::size_t frame_count_ = 1;
  frame_type frame_;
};

template <typename StaticRegistry>
using static_context = basic_static_context<StaticRegistry, false>;

} // namespace dingo
