//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/config.h>

#include <memory>
#include <optional>
#include <type_traits>

namespace dingo {

// TODO: merge with type_traits
template <typename T> struct class_traits {
    template <typename... Args> static T construct(Args&&... args) {
        return T{std::forward<Args>(args)...};
    }
};

template <typename T> struct class_traits<T*> {
    template <typename... Args> static T* construct(Args&&... args) {
        return new T{std::forward<Args>(args)...};
    }

    template <typename... Args> static T* construct(void* ptr, Args&&... args) {
        return new (ptr) T{std::forward<Args>(args)...};
    }
};

template <typename T> struct class_traits<T&> {
    template <typename... Args> static T& construct(Args&&...) {
        static_assert(true, "references cannot be constructed");
    }
};

template <typename T> struct class_traits<std::unique_ptr<T>> {
    template <typename... Args>
    static std::unique_ptr<T> construct(Args&&... args) {
        return std::make_unique<T>(std::forward<Args>(args)...);
    }
};

template <typename T> struct class_traits<std::shared_ptr<T>> {
    template <typename... Args>
    static std::shared_ptr<T> construct(Args&&... args) {
        return std::make_shared<T>(std::forward<Args>(args)...);
    }
};

template <typename T> struct class_traits<std::optional<T>> {
    template <typename... Args>
    static std::optional<T> construct(Args&&... args) {
        return std::make_optional<T>(std::forward<Args>(args)...);
    }

    template <typename... Args>
    static std::optional<T>& constructE(std::optional<T>& ptr, Args&&... args) {
        ptr.emplace(std::forward<Args>(args)...);
        return ptr;
    }
};

} // namespace dingo
