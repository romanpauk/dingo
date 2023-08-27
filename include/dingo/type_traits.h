#pragma once

#include <memory>

namespace dingo {
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