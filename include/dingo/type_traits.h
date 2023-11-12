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
    static const bool is_smart_ptr = false;
    static void* get_address(T& value) { return &value; }
};

template <typename T> struct type_traits<T*> {
    static const bool is_smart_ptr = false;
    static void* get_address(T* value) { return value; }
};

template <typename T> struct type_traits<std::unique_ptr<T>> {
    static const bool is_smart_ptr = true;
    static void* get_address(std::unique_ptr<T>& ptr) { return ptr.get(); }
};

template <class T> struct type_traits<std::shared_ptr<T>> {
    static const bool is_smart_ptr = true;
    static void* get_address(std::shared_ptr<T>& ptr) { return ptr.get(); }
};
} // namespace dingo