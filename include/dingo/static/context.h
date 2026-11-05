//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/context/context_base.h>
#include <dingo/static/graph.h>

#include <array>
#include <cassert>
#include <cstddef>
#include <new>

namespace dingo {

template <typename StaticRegistry> class static_context_backend {
  using execution_traits =
      detail::basic_static_execution_traits<StaticRegistry>;
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

public:
  static constexpr bool is_transactional = false;

  // Retained resolutions construct a local frame of this type, sized for the
  // whole context graph, so a registry resolving through a context whose
  // bindings extend its own (a static parent under a child) pushes a frame of
  // exactly the type the context stores.
  using frame_type =
      detail::static_context_frame<destructible_capacity_,
                                   temporary_slot_capacity_,
                                   temporary_slot_size_, temporary_slot_align_>;

  static_context_backend() { frames_[0] = &frame_; }

  ~static_context_backend() {
    while (frame_count_ != 0) {
      frames_[--frame_count_]->reset();
    }
  }

  static_context_backend(const static_context_backend &) = delete;
  static_context_backend &operator=(const static_context_backend &) = delete;

  // Nothing a static resolution constructs outlives its context, so there is
  // no transaction to roll back.
  void on_rollback(void (*)(void *, void *) noexcept, void *,
                   void * = nullptr) noexcept {}

  void add_runtime_destructor(void *, void (*)(void *) noexcept) noexcept {}

  void push_frame(frame_type *frame) {
    assert(frame_count_ < frames_.size());
    frames_[frame_count_++] = frame;
  }

  void pop_frame() {
    assert(frame_count_ != 0);
    --frame_count_;
  }

protected:
  template <typename T, typename ConstructFn>
  T &construct_storage(construction_scope, ConstructFn &&construct_fn) {
    auto *instance = allocate_temporary_storage<T>();
    std::forward<ConstructFn>(construct_fn)(instance);
    if constexpr (!std::is_trivially_destructible_v<T>) {
      register_destructor(instance);
    }
    return *instance;
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

    auto *fixed = active_frame().template try_allocate_temporary<T>();
    assert(fixed != nullptr);
    return fixed;
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

  std::array<frame_type *, frame_capacity_> frames_{};
  std::size_t frame_count_ = 1;
  frame_type frame_;
};

template <typename StaticRegistry>
using static_context =
    resolution_context<static_context_backend<StaticRegistry>>;

} // namespace dingo
