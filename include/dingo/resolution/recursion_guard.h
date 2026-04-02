//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/memory/aligned_storage.h>
#include <dingo/resolution/resolving_context.h>

namespace dingo::detail {

template <typename T,
          bool DefaultConstructible = std::is_default_constructible_v<T>>
struct recursion_guard {
    explicit recursion_guard(resolving_context& context)
        : frame_guard_(context.template track_type<T>()) {
        // Track the active type path first so recursion exceptions can report
        // the full resolution chain, including the repeated type.
        if (visited_) {
            throw detail::make_type_recursion_exception<T>(context);
        }
        visited_ = true;
    }

    recursion_guard(const recursion_guard&) = delete;
    recursion_guard& operator=(const recursion_guard&) = delete;
    recursion_guard(recursion_guard&&) = delete;
    recursion_guard& operator=(recursion_guard&&) = delete;

    ~recursion_guard() {
        if (active_) {
            visited_ = false;
        }
    }

  private:
    detail::resolving_frame frame_guard_;
    bool active_ = true;
    static thread_local bool visited_;
};

template <typename T, bool DefaultConstructible>
thread_local bool recursion_guard<T, DefaultConstructible>::visited_ = false;

template <typename T> struct recursion_guard<T, true> {
    explicit recursion_guard(resolving_context&) {}

    recursion_guard(const recursion_guard&) = delete;
    recursion_guard& operator=(const recursion_guard&) = delete;
    recursion_guard(recursion_guard&&) = delete;
    recursion_guard& operator=(recursion_guard&&) = delete;
};

template <typename T> class recursion_guard_wrapper {
  public:
    recursion_guard_wrapper(resolving_context& context, bool enabled) {
        if (enabled) {
            // The wrapped recursion_guard owns a self-linking resolving_frame,
            // so it must stay at a fixed address for the whole guarded scope.
            new (&storage_) recursion_guard<T>(context);
            active_ = true;
        }
    }

    recursion_guard_wrapper(const recursion_guard_wrapper&) = delete;
    recursion_guard_wrapper& operator=(const recursion_guard_wrapper&) = delete;
    recursion_guard_wrapper(recursion_guard_wrapper&&) = delete;
    recursion_guard_wrapper& operator=(recursion_guard_wrapper&&) = delete;

    ~recursion_guard_wrapper() {
        if (active_) {
            get()->~recursion_guard<T>();
        }
    }

  private:
    recursion_guard<T>* get() {
        return reinterpret_cast<recursion_guard<T>*>(&storage_);
    }

    aligned_storage_t<sizeof(recursion_guard<T>), alignof(recursion_guard<T>)>
        storage_;
    bool active_ = false;
};

} // namespace dingo::detail
