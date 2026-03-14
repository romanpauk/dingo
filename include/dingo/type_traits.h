//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/config.h>

#include <dingo/exceptions.h>

#include <memory>
#include <optional>
#include <type_traits>
#include <utility>

namespace dingo {
template <typename... Types> struct type_list;

template <typename T, typename = void> struct type_traits {
    static constexpr bool enabled = false;
    static constexpr bool is_pointer_like = false;
};

template <typename T>
inline constexpr bool has_type_traits_v = type_traits<T>::enabled;

template <typename T>
inline constexpr bool is_pointer_like_type_v =
    has_type_traits_v<T> && type_traits<T>::is_pointer_like &&
    !std::is_pointer_v<T>;

namespace detail {
template <typename Type, typename Pointer, typename = void>
struct has_type_from_pointer : std::false_type {};

template <typename Type, typename Pointer>
struct has_type_from_pointer<
    Type, Pointer,
    std::void_t<decltype(type_traits<Type>::from_pointer(
        std::declval<Pointer>()))>> : std::true_type {};

template <typename Type, typename Pointer>
inline constexpr bool has_type_from_pointer_v =
    has_type_from_pointer<Type, Pointer>::value;
} // namespace detail

template <typename T> struct type_traits<T*> {
    static constexpr bool enabled = true;
    static constexpr bool is_pointer_like = true;

    using value_type = T;

    template <typename U> using rebind_t = U*;

    static T* get(T* ptr) { return ptr; }
    static bool empty(T* ptr) { return ptr == nullptr; }
    static void reset(T*& ptr) { ptr = nullptr; }
    static T* from_pointer(T* ptr) { return ptr; }

    template <typename TargetType, typename Factory, typename Context>
    static TargetType resolve_type(Factory& factory, Context& context) {
        if constexpr (std::is_same_v<TargetType, rebind_t<T>>)
            return factory.resolve(context);
        else
            throw type_not_convertible_exception();
    }
};

template <typename T, typename Deleter>
struct type_traits<std::unique_ptr<T, Deleter>> {
    static constexpr bool enabled = true;
    static constexpr bool is_pointer_like = true;

    using value_type = T;

    template <typename U>
    using rebind_t = std::unique_ptr<U, std::default_delete<U>>;

    static T* get(std::unique_ptr<T, Deleter>& wrapper) {
        return wrapper.get();
    }

    static const T* get(const std::unique_ptr<T, Deleter>& wrapper) {
        return wrapper.get();
    }

    static bool empty(const std::unique_ptr<T, Deleter>& wrapper) {
        return wrapper.get() == nullptr;
    }

    static void reset(std::unique_ptr<T, Deleter>& wrapper) { wrapper.reset(); }

    template <typename... Args>
    static std::unique_ptr<T, Deleter> make(Args&&... args) {
        return std::unique_ptr<T, Deleter>(new T{std::forward<Args>(args)...});
    }

    static std::unique_ptr<T, Deleter> from_pointer(T* ptr) {
        return std::unique_ptr<T, Deleter>(ptr);
    }

    template <typename TargetType, typename Factory, typename Context>
    static TargetType& resolve_type(Factory& factory, Context& context) {
        if constexpr (std::is_same_v<TargetType, rebind_t<T>>)
            return factory.resolve(context);
        else
            throw type_not_convertible_exception();
    }
};

template <typename T> struct type_traits<std::shared_ptr<T>> {
    static constexpr bool enabled = true;
    static constexpr bool is_pointer_like = true;

    using value_type = T;

    template <typename U> using rebind_t = std::shared_ptr<U>;

    static T* get(std::shared_ptr<T>& wrapper) { return wrapper.get(); }

    static const T* get(const std::shared_ptr<T>& wrapper) {
        return wrapper.get();
    }

    static bool empty(const std::shared_ptr<T>& wrapper) {
        return wrapper.get() == nullptr;
    }

    static void reset(std::shared_ptr<T>& wrapper) { wrapper.reset(); }

    template <typename... Args> static std::shared_ptr<T> make(Args&&... args) {
        // Work around direct-initialization in std::make_shared().
        if constexpr (std::is_constructible_v<T, Args...>)
            return std::make_shared<T>(std::forward<Args>(args)...);
        else
            return std::shared_ptr<T>(new T{std::forward<Args>(args)...});
    }

    static std::shared_ptr<T> from_pointer(T* ptr) {
        return std::shared_ptr<T>(ptr);
    }

    template <typename TargetType, typename Factory, typename Context>
    static TargetType& resolve_type(Factory& factory, Context& context) {
        return factory.template resolve<TargetType>(context);
    }
};

template <typename T> struct type_traits<std::optional<T>> {
    static constexpr bool enabled = true;
    static constexpr bool is_pointer_like = false;

    using value_type = T;

    template <typename U> using rebind_t = std::optional<U>;

    static T* get(std::optional<T>& wrapper) {
        return wrapper ? std::addressof(wrapper.value()) : nullptr;
    }

    static const T* get(const std::optional<T>& wrapper) {
        return wrapper ? std::addressof(wrapper.value()) : nullptr;
    }

    static bool empty(const std::optional<T>& wrapper) {
        return !wrapper.has_value();
    }

    static void reset(std::optional<T>& wrapper) { wrapper.reset(); }

    template <typename... Args> static std::optional<T> make(Args&&... args) {
        if constexpr (std::is_constructible_v<T, Args...>)
            return std::optional<T>{std::in_place, std::forward<Args>(args)...};
        else
            return std::optional<T>{std::in_place,
                                    T{std::forward<Args>(args)...}};
    }

    template <typename TargetType, typename Factory, typename Context>
    static TargetType& resolve_type(Factory& factory, Context& context) {
        if constexpr (std::is_same_v<TargetType, rebind_t<T>>)
            return factory.resolve(context);
        else
            throw type_not_convertible_exception();
    }
};

} // namespace dingo
