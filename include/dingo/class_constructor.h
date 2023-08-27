#pragma once

#include <memory>
#include <optional>

namespace dingo {
template <typename T> struct class_constructor {
    template <typename... Args> static T invoke(Args&&... args) { return T(std::forward<Args>(args)...); }
};

template <typename T> struct class_constructor<T*> {
    template <typename... Args> static T* invoke(Args&&... args) { return new T(std::forward<Args>(args)...); }

    template <typename... Args> static T* invoke(void* ptr, Args&&... args) {
        return new (ptr) T(std::forward<Args>(args)...);
    }
};

template <typename T> struct class_constructor<T&> {
    template <typename... Args> static T& invoke(Args&&... args) {
        static_assert(true, "references cannot be constructed");
    }
};

template <typename T> struct class_constructor<std::unique_ptr<T>> {
    template <typename... Args> static std::unique_ptr<T> invoke(Args&&... args) {
        return std::make_unique<T>(std::forward<Args>(args)...);
    }
};

template <typename T> struct class_constructor<std::shared_ptr<T>> {
    template <typename... Args> static std::shared_ptr<T> invoke(Args&&... args) {
        return std::make_shared<T>(std::forward<Args>(args)...);
    }
};

template <typename T> struct class_constructor<std::optional<T>> {
    template <typename... Args> static std::optional<T> invoke(Args&&... args) {
        return std::make_optional<T>(std::forward<Args>(args)...);
    }

    template <typename... Args> static std::optional<T>& Construct(std::optional<T>& ptr, Args&&... args) {
        ptr.emplace(std::forward<Args>(args)...);
        return ptr;
    }
};
} // namespace dingo