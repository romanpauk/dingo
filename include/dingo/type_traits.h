//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/config.h>

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
} // namespace dingo