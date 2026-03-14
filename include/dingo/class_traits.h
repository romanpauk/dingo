//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/config.h>
#include <dingo/type_traits.h>

#include <type_traits>

namespace dingo {

// TODO: merge with type_traits
template <typename T, typename = void> struct class_traits {
    template <typename... Args> static T construct(Args&&... args) {
        return T{std::forward<Args>(args)...};
    }

    template <typename... Args>
    static void construct(void* ptr, Args&&... args) {
        new (ptr) T{std::forward<Args>(args)...};
    }
};

template <typename T> struct class_traits<T*> {
    template <typename... Args> static T* construct(Args&&... args) {
        return new T{std::forward<Args>(args)...};
    }

    template <typename... Args>
    static void construct(void* ptr, Args&&... args) {
        new (ptr) T{std::forward<Args>(args)...};
    }
};

template <typename T> struct class_traits<T&> {
    template <typename... Args> static T& construct(Args&&...) {
        static_assert(true, "references cannot be constructed");
    }
};

template <typename T>
struct class_traits<
    T, std::enable_if_t<type_traits<T>::enabled && !std::is_pointer_v<T>>> {
    template <typename... Args> static T construct(Args&&... args) {
        return type_traits<T>::make(std::forward<Args>(args)...);
    }

    template <typename... Args>
    static T& construct(T& ptr, Args&&... args) {
        ptr = type_traits<T>::make(std::forward<Args>(args)...);
        return ptr;
    }

    template <typename... Args>
    static void construct(void* ptr, Args&&... args) {
        new (ptr) T(type_traits<T>::make(std::forward<Args>(args)...));
    }
};

} // namespace dingo
