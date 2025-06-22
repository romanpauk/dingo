//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/config.h>

#include <algorithm>
#include <cstdint>
#include <memory>

namespace dingo {
template <typename T, typename = void>
struct has_value_type : std::false_type {};
template <typename T>
struct has_value_type<T, typename std::void_t<typename T::value_type>>
    : std::true_type {};
template <typename T>
static constexpr bool has_value_type_v = has_value_type<T>::value;

template <typename T> struct type_traits {
    static constexpr bool is_pointer_type = false;
    static T* get_address(T& value) { return &value; }
};

template <typename T> struct type_traits<T&> {
    static constexpr bool is_pointer_type = false;
    static T* get_address(T& value) { return &value; }
};

template <typename T> struct type_traits<T*> {
    static constexpr bool is_pointer_type = true;
    static T* get_address(T* value) { return value; }
};

template <typename T> struct type_traits<std::unique_ptr<T>> {
    static constexpr bool is_pointer_type = true;
    static std::unique_ptr<T>* get_address(std::unique_ptr<T>& ptr) {
        return &ptr;
    }
};

template <class T> struct type_traits<std::shared_ptr<T>> {
    static constexpr bool is_pointer_type = true;
    static std::shared_ptr<T>* get_address(std::shared_ptr<T>& ptr) {
        return &ptr;
    }
};

template <std::size_t Len, std::size_t Alignment> struct aligned_storage {
    struct type {
        alignas(Alignment) std::uint8_t data[Len];
    };
};

template <std::size_t Len, std::size_t Alignment>
using aligned_storage_t = typename aligned_storage<Len, Alignment>::type;

template <std::size_t MinLen, typename... Ts> struct aligned_union {
    static constexpr std::size_t length = std::max({MinLen, sizeof(Ts)...});
    static constexpr std::size_t alignment = std::max({alignof(Ts)...});

    using type = typename aligned_storage<length, alignment>::type;
};

template <std::size_t MinLen, typename... Ts>
using aligned_union_t = typename aligned_union<MinLen, Ts...>::type;
} // namespace dingo
