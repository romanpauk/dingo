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
#include <variant>

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

namespace detail {
template <typename Variant, typename Selected> struct variant_alternative_count;

template <typename Selected, typename... Alternatives>
struct variant_alternative_count<std::variant<Alternatives...>, Selected>
    : std::integral_constant<size_t,
                             (0u + ... + (std::is_same_v<Selected, Alternatives> ? 1u
                                                                                 : 0u))> {};

template <typename Type, typename Selected, typename = void>
struct construction_traits {
    template <typename... Args> static Type construct(Args&&... args) {
        return class_traits<Type>::construct(std::forward<Args>(args)...);
    }

    template <typename... Args>
    static void construct(void* ptr, Args&&... args) {
        class_traits<Type>::construct(ptr, std::forward<Args>(args)...);
    }
};

template <typename... Alternatives, typename Selected>
struct construction_traits<
    std::variant<Alternatives...>, Selected,
    std::enable_if_t<
        variant_alternative_count<std::variant<Alternatives...>, Selected>::value == 1>> {
    using type = std::variant<Alternatives...>;

    template <typename... Args> static type construct(Args&&... args) {
        return type(std::in_place_type<Selected>,
                    class_traits<Selected>::construct(
                        std::forward<Args>(args)...));
    }

    template <typename... Args>
    static void construct(void* ptr, Args&&... args) {
        new (ptr) type(std::in_place_type<Selected>,
                       class_traits<Selected>::construct(
                           std::forward<Args>(args)...));
    }
};

template <typename... Alternatives, typename Selected>
struct construction_traits<
    std::variant<Alternatives...>*, Selected,
    std::enable_if_t<
        variant_alternative_count<std::variant<Alternatives...>, Selected>::value == 1>> {
    using type = std::variant<Alternatives...>;

    template <typename... Args> static type* construct(Args&&... args) {
        return new type(std::in_place_type<Selected>,
                        class_traits<Selected>::construct(
                            std::forward<Args>(args)...));
    }

    template <typename... Args>
    static void construct(void* ptr, Args&&... args) {
        new (ptr) type(std::in_place_type<Selected>,
                       class_traits<Selected>::construct(
                           std::forward<Args>(args)...));
    }
};
} // namespace detail

} // namespace dingo
