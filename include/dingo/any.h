//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/config.h>

#include <cassert>

namespace dingo {
struct any {
    template <typename T> any(T* ptr) : ptr_(ptr) {}
    void* get() { return ptr_; }

    static void* serialize(any& instance) {
        return reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(&instance) |
                                       1);
    }

    template <typename T, bool EnableAny = true>
    static T deserialize(void* ptr) {
        if constexpr (EnableAny) {
            if (reinterpret_cast<uintptr_t>(ptr) & 1) {
                any* a = reinterpret_cast<any*>(
                    reinterpret_cast<uintptr_t>(ptr) & ~1);
                return static_cast<T>(a->get());
            }
        } else {
            assert((reinterpret_cast<uintptr_t>(ptr) & 1) == 0);
        }
        return static_cast<T>(ptr);
    }

  private:
    void* ptr_;
};

template <typename T> struct any_impl : any {
    template <typename... Args>
    any_impl(Args&&... args)
        : any(&instance_), instance_(std::forward<Args>(args)...) {}

  private:
    T instance_;
};

template <typename T> struct any_impl<T&> : any {
    any_impl(T& instance) : any(&instance) {}
};

// TODO: for unique pointers this is not true
template <typename T> struct any_impl<T*> : any {
    any_impl(T* instance) : any(instance) {}
};
} // namespace dingo