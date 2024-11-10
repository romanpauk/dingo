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

template <typename T> struct class_traits<std::unique_ptr<T>> {
    template <typename... Args>
    static std::unique_ptr<T> construct(Args&&... args) {
        return std::unique_ptr<T>{new T{std::forward<Args>(args)...}};
    }

    template <typename... Args>
    static void construct(void* ptr, Args&&... args) {
        new (ptr) std::unique_ptr<T>{new T{std::forward<Args>(args)...}};
    }
};

template <typename T> struct class_traits<std::shared_ptr<T>> {
    template <typename... Args>
    static std::shared_ptr<T> construct(Args&&... args) {
        // TODO: work around direct-initialization in std::make_shared().
        // https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2015/n4462.html
        if constexpr(std::is_constructible_v<T, Args...>)
            return std::make_shared<T>(std::forward<Args>(args)...);
        else
            return std::shared_ptr<T>{new T{std::forward<Args>(args)...}};
    }

    template <typename... Args>
    static void construct(void* ptr, Args&&... args) {
        new (ptr) std::shared_ptr<T>{new T{std::forward<Args>(args)...}};
    }
};

template <typename T> struct class_traits<std::optional<T>> {
    template <typename... Args>
    static std::optional<T> construct(Args&&... args) {
        if constexpr(std::is_constructible_v<T, Args...>)
            return std::optional<T>{std::in_place, std::forward<Args>(args)...};
        else
            return std::optional<T>{std::in_place, T{std::forward<Args>(args)...}};
    }

    template <typename... Args>
    static std::optional<T>& construct(std::optional<T>& ptr, Args&&... args) {
        if constexpr (std::is_constructible_v<T, Args...>)
            ptr.emplace(std::forward<Args>(args)...);
        else
            ptr.emplace(T{std::forward<Args>(args)...});
        return ptr;
    }

    template <typename... Args>
    static void construct(void* ptr, Args&&... args) {
        if constexpr(std::is_constructible_v<T, Args...>)
            new (ptr) std::optional<T>{std::in_place, std::forward<Args>(args)...};
        else
            new (ptr) std::optional<T>{std::in_place, T{std::forward<Args>(args)...}};
    }
};

} // namespace dingo
