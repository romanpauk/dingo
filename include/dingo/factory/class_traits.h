//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/config.h>
#include <dingo/type/type_traits.h>

#include <type_traits>

namespace dingo {

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

template <typename T, size_t N> struct class_traits<T[N]> {
    template <typename... Args> static T* construct(Args&&... args) {
        return detail::make_bounded_array<T, N>(std::forward<Args>(args)...);
    }

    template <typename... Args>
    static void construct(void* ptr, Args&&... args) {
        detail::construct_bounded_array<T, N>(ptr,
                                              std::forward<Args>(args)...);
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

namespace detail {
template <typename Type, typename Selected, typename = void>
struct construction_dispatch {
    template <typename... Args> static auto construct(Args&&... args) {
        return class_traits<Type>::construct(std::forward<Args>(args)...);
    }

    template <typename... Args>
    static void construct(void* ptr, Args&&... args) {
        class_traits<Type>::construct(ptr, std::forward<Args>(args)...);
    }
};

template <typename Type, typename Selected>
struct construction_dispatch<
    Type, Selected,
    std::enable_if_t<construction_traits<Type, Selected>::enabled>> {
    using type = typename construction_traits<Type, Selected>::type;

    template <typename... Args> static auto construct(Args&&... args) {
        return construction_traits<Type, Selected>::wrap(
            class_traits<Selected>::construct(std::forward<Args>(args)...));
    }

    template <typename... Args>
    static void construct(void* ptr, Args&&... args) {
        new (ptr) type(construction_traits<Type, Selected>::wrap(
            class_traits<Selected>::construct(std::forward<Args>(args)...)));
    }
};

template <typename Type, typename Selected>
struct construction_dispatch<
    Type*, Selected,
    std::enable_if_t<construction_traits<Type, Selected>::enabled>> {
    using type = typename construction_traits<Type, Selected>::type;

    template <typename... Args> static auto construct(Args&&... args) {
        return new type(construction_traits<Type, Selected>::wrap(
            class_traits<Selected>::construct(std::forward<Args>(args)...)));
    }

    template <typename... Args>
    static void construct(void* ptr, Args&&... args) {
        new (ptr) type(construction_traits<Type, Selected>::wrap(
            class_traits<Selected>::construct(std::forward<Args>(args)...)));
    }
};
} // namespace detail

} // namespace dingo
