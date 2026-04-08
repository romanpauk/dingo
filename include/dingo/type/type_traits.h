//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/config.h>

#include <dingo/exceptions.h>
#include <dingo/type/type_list.h>

#include <memory>
#include <optional>
#include <type_traits>
#include <utility>
#include <variant>

namespace dingo {
template <typename T, typename = void> struct type_traits {
    static constexpr bool enabled = false;
    static constexpr bool is_pointer_like = false;
    static constexpr bool is_value_borrowable = false;

    template <typename>
    static constexpr bool is_handle_rebindable = false;

    template <typename>
    static constexpr bool is_rebindable = false;
};
template <typename T>
inline constexpr bool is_pointer_like_type_v =
    type_traits<T>::enabled && type_traits<T>::is_pointer_like &&
    !std::is_pointer_v<T>;

template <typename Type, typename Selected, typename = void>
struct construction_traits {
    static constexpr bool enabled = false;
};

template <typename T, typename = void> struct alternative_type_traits {
    static constexpr bool enabled = false;
};

template <typename T>
inline constexpr bool is_alternative_type_v =
    alternative_type_traits<std::remove_cv_t<T>>::enabled;

namespace detail {
template <typename List, typename Selected> struct type_list_count;

template <typename Selected, typename... Alternatives>
struct type_list_count<type_list<Alternatives...>, Selected>
    : std::integral_constant<size_t,
                             (0u + ... + (std::is_same_v<Selected, Alternatives> ? 1u
                                                                                 : 0u))> {};

template <typename List> struct type_list_has_duplicates;

template <typename... Alternatives>
struct type_list_has_duplicates<type_list<Alternatives...>>
    : std::bool_constant<
          ((type_list_count<type_list<Alternatives...>, Alternatives>::value > 1) ||
           ...)> {};

template <typename Type, typename = void>
struct alternative_type_alternatives {};

template <typename Type>
struct alternative_type_alternatives<
    Type,
    std::enable_if_t<alternative_type_traits<std::remove_cv_t<Type>>::enabled>> {
    using type = typename alternative_type_traits<std::remove_cv_t<Type>>::alternatives;

    static_assert(
        !type_list_has_duplicates<type>::value,
        "alternative_type_traits<T>::alternatives must not contain duplicate types");
};

template <typename Type>
using alternative_type_alternatives_t =
    typename alternative_type_alternatives<Type>::type;

template <typename Type, typename Selected, typename = void>
struct alternative_type_count : std::integral_constant<size_t, 0> {};

template <typename Type, typename Selected>
struct alternative_type_count<
    Type, Selected,
    std::enable_if_t<alternative_type_traits<std::remove_cv_t<Type>>::enabled>>
    : type_list_count<alternative_type_alternatives_t<Type>,
                      std::remove_cv_t<Selected>> {};

template <typename Type, typename = void>
struct alternative_type_interface_types {};

template <typename Type>
struct alternative_type_interface_types<
    Type,
    std::enable_if_t<alternative_type_traits<std::remove_cv_t<Type>>::enabled>> {
    using type =
        type_list_cat_t<type_list<std::remove_cv_t<Type>>,
                        alternative_type_alternatives_t<Type>>;
};
} // namespace detail

template <typename... Alternatives>
struct alternative_type_traits<std::variant<Alternatives...>> {
    static constexpr bool enabled = true;

    using alternatives = type_list<Alternatives...>;

    template <typename Selected, typename Value>
    static std::variant<Alternatives...> wrap(Value&& value) {
        return std::variant<Alternatives...>(std::in_place_type<Selected>,
                                             std::forward<Value>(value));
    }

    template <typename Selected>
    static Selected* get(std::variant<Alternatives...>& value) {
        return std::get_if<Selected>(&value);
    }

    template <typename Selected>
    static const Selected* get(const std::variant<Alternatives...>& value) {
        return std::get_if<Selected>(&value);
    }
};

template <typename Type, typename Interface>
inline constexpr bool is_alternative_type_interface_compatible_v =
    detail::alternative_type_count<std::remove_cv_t<Type>,
                                   std::remove_cv_t<Interface>>::value == 1;

template <typename Type, typename Selected>
struct construction_traits<
    Type, Selected,
    std::enable_if_t<is_alternative_type_v<Type> &&
                     (detail::alternative_type_count<Type, Selected>::value ==
                      1)>> {
    static constexpr bool enabled = true;

    using type = std::remove_cv_t<Type>;

    template <typename Value> static type wrap(Value&& value) {
        return alternative_type_traits<type>::template wrap<Selected>(
            std::forward<Value>(value));
    }
};

template <typename Type, typename Selected>
struct construction_traits<
    Type, Selected,
    std::enable_if_t<type_traits<Type>::enabled && !std::is_pointer_v<Type> &&
                     construction_traits<typename type_traits<Type>::value_type,
                                         Selected>::enabled>> {
    static constexpr bool enabled = true;

    using value_type = typename type_traits<Type>::value_type;
    using type = Type;

    template <typename Value> static type wrap(Value&& value) {
        return type_traits<Type>::make(
            construction_traits<value_type, Selected>::wrap(
                std::forward<Value>(value)));
    }
};

namespace detail {
template <typename T, typename... Args> T make_nested(Args&&... args);

template <typename T, size_t N, typename... Args>
T* make_bounded_array(Args&&... args) {
    static_assert(sizeof...(Args) <= N,
                  "too many initializers for bounded array construction");
    return new T[N]{std::forward<Args>(args)...};
}

template <typename T, size_t N, typename... Args>
void construct_bounded_array(void* ptr, Args&&... args) {
    static_assert(sizeof...(Args) <= N,
                  "too many initializers for bounded array construction");
    new (ptr) T[N]{std::forward<Args>(args)...};
}

template <typename T, typename... Args> T* make_dynamic_array(Args&&... args) {
    static_assert(sizeof...(Args) > 0,
                  "dynamic arrays require an explicit size via a custom "
                  "factory or element initializers");
    return new T[sizeof...(Args)]{std::forward<Args>(args)...};
}

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

template <typename Type, typename = void>
struct is_array_like_type : std::is_array<Type> {};

template <typename Type>
inline constexpr bool is_array_like_type_v = is_array_like_type<Type>::value;

template <typename Type, typename = void>
struct array_like_exact_interface_type {
    using type = Type;
};

template <typename Type>
using array_like_exact_interface_type_t =
    typename array_like_exact_interface_type<Type>::type;

template <typename Array, typename Deleter>
struct is_array_like_type<
    std::unique_ptr<Array, Deleter>,
    std::enable_if_t<std::is_array_v<Array>>> : std::true_type {};

template <typename Array, typename Deleter>
struct array_like_exact_interface_type<
    std::unique_ptr<Array, Deleter>,
    std::enable_if_t<std::is_array_v<Array>>> {
    using type = typename type_traits<std::unique_ptr<Array, Deleter>>::value_type;
};

template <typename Array>
struct is_array_like_type<std::shared_ptr<Array>,
                          std::enable_if_t<std::is_array_v<Array>>>
    : std::true_type {};

template <typename Array>
struct array_like_exact_interface_type<
    std::shared_ptr<Array>, std::enable_if_t<std::is_array_v<Array>>> {
    using type = typename type_traits<std::shared_ptr<Array>>::value_type;
};
} // namespace detail

template <typename T> struct type_traits<T*> {
    static constexpr bool enabled = true;
    static constexpr bool is_pointer_like = true;
    static constexpr bool is_value_borrowable = true;

    template <typename Target>
    static constexpr bool is_handle_rebindable =
        std::is_constructible_v<Target, T*>;

    using value_type = T;

    template <typename U> using rebind_t = U*;

    template <typename Target>
    static constexpr bool is_rebindable = std::is_same_v<Target, rebind_t<T>>;

    static T* get(T* ptr) { return ptr; }
    static T& borrow(T* ptr) { return *ptr; }
    static bool empty(T* ptr) { return ptr == nullptr; }
    static void reset(T*& ptr) { ptr = nullptr; }
    static T* from_pointer(T* ptr) { return ptr; }

    template <typename TargetType, typename Factory, typename Context>
    static TargetType resolve_type(Factory& factory, Context& context,
                                   type_descriptor requested_type,
                                   type_descriptor registered_type) {
        if constexpr (std::is_same_v<TargetType, rebind_t<T>>)
            return factory.resolve(context);
        else
            throw detail::make_type_not_convertible_exception(requested_type,
                                                              registered_type,
                                                              context);
    }
};

template <> struct type_traits<void*> {
    static constexpr bool enabled = true;
    static constexpr bool is_pointer_like = true;
    static constexpr bool is_value_borrowable = false;

    template <typename Target>
    static constexpr bool is_handle_rebindable =
        std::is_constructible_v<Target, void*>;

    using value_type = void;

    template <typename U> using rebind_t = U*;

    template <typename Target>
    static constexpr bool is_rebindable =
        std::is_same_v<Target, rebind_t<void>>;

    static void* get(void* ptr) { return ptr; }
    static bool empty(void* ptr) { return ptr == nullptr; }
    static void reset(void*& ptr) { ptr = nullptr; }
    static void* from_pointer(void* ptr) { return ptr; }

    template <typename TargetType, typename Factory, typename Context>
    static TargetType resolve_type(Factory& factory, Context& context,
                                   type_descriptor requested_type,
                                   type_descriptor registered_type) {
        if constexpr (std::is_same_v<TargetType, rebind_t<void>>)
            return factory.resolve(context);
        else
            throw detail::make_type_not_convertible_exception(requested_type,
                                                              registered_type,
                                                              context);
    }
};

template <> struct type_traits<const void*> {
    static constexpr bool enabled = true;
    static constexpr bool is_pointer_like = true;
    static constexpr bool is_value_borrowable = false;

    template <typename Target>
    static constexpr bool is_handle_rebindable =
        std::is_constructible_v<Target, const void*>;

    using value_type = const void;

    template <typename U> using rebind_t = U*;

    template <typename Target>
    static constexpr bool is_rebindable =
        std::is_same_v<Target, rebind_t<const void>>;

    static const void* get(const void* ptr) { return ptr; }
    static bool empty(const void* ptr) { return ptr == nullptr; }
    static void reset(const void*& ptr) { ptr = nullptr; }
    static const void* from_pointer(const void* ptr) { return ptr; }

    template <typename TargetType, typename Factory, typename Context>
    static TargetType resolve_type(Factory& factory, Context& context,
                                   type_descriptor requested_type,
                                   type_descriptor registered_type) {
        if constexpr (std::is_same_v<TargetType, rebind_t<const void>>)
            return factory.resolve(context);
        else
            throw detail::make_type_not_convertible_exception(requested_type,
                                                              registered_type,
                                                              context);
    }
};

template <typename Array, typename Deleter>
struct type_traits<
    std::unique_ptr<Array, Deleter>,
    std::enable_if_t<std::is_array_v<Array> && (std::extent_v<Array, 0> == 0)>> {
    static constexpr bool enabled = true;
    static constexpr bool is_pointer_like = true;
    static constexpr bool is_value_borrowable = false;

    template <typename Target>
    static constexpr bool is_handle_rebindable =
        std::is_constructible_v<Target, std::unique_ptr<Array, Deleter>>;

    using value_type = std::remove_extent_t<Array>;

    template <typename U>
    using rebind_t = std::unique_ptr<U[], std::default_delete<U[]>>;

    template <typename Target>
    static constexpr bool is_rebindable =
        std::is_same_v<Target, rebind_t<value_type>>;

    static value_type* get(std::unique_ptr<Array, Deleter>& wrapper) {
        return wrapper.get();
    }

    static const value_type* get(const std::unique_ptr<Array, Deleter>& wrapper) {
        return wrapper.get();
    }

    static bool empty(const std::unique_ptr<Array, Deleter>& wrapper) {
        return wrapper.get() == nullptr;
    }

    static void reset(std::unique_ptr<Array, Deleter>& wrapper) {
        wrapper.reset();
    }

    template <typename... Args>
    static std::unique_ptr<Array, Deleter> make(Args&&... args) {
        return std::unique_ptr<Array, Deleter>(detail::make_dynamic_array<value_type>(
            std::forward<Args>(args)...));
    }

    static std::unique_ptr<Array, Deleter> from_pointer(value_type* ptr) {
        return std::unique_ptr<Array, Deleter>(ptr);
    }

    template <typename TargetType, typename Factory, typename Context>
    static TargetType& resolve_type(Factory& factory, Context& context,
                                    type_descriptor requested_type,
                                    type_descriptor registered_type) {
        if constexpr (std::is_same_v<TargetType, rebind_t<value_type>>)
            return factory.resolve(context);
        else
            throw detail::make_type_not_convertible_exception(requested_type,
                                                              registered_type,
                                                              context);
    }
};

template <typename T, typename Deleter>
struct type_traits<std::unique_ptr<T, Deleter>,
                   std::enable_if_t<!std::is_array_v<T>>> {
    static constexpr bool enabled = true;
    static constexpr bool is_pointer_like = true;
    static constexpr bool is_value_borrowable = true;

    template <typename Target>
    static constexpr bool is_handle_rebindable =
        std::is_constructible_v<Target, std::unique_ptr<T, Deleter>>;

    using value_type = T;

    template <typename U>
    using rebind_t = std::unique_ptr<U, std::default_delete<U>>;

    template <typename Target>
    static constexpr bool is_rebindable =
        std::is_same_v<Target, rebind_t<T>>;

    static T* get(std::unique_ptr<T, Deleter>& wrapper) {
        return wrapper.get();
    }

    static const T* get(const std::unique_ptr<T, Deleter>& wrapper) {
        return wrapper.get();
    }

    static T& borrow(std::unique_ptr<T, Deleter>& wrapper) {
        return *wrapper;
    }

    static const T& borrow(const std::unique_ptr<T, Deleter>& wrapper) {
        return *wrapper;
    }

    static bool empty(const std::unique_ptr<T, Deleter>& wrapper) {
        return wrapper.get() == nullptr;
    }

    static void reset(std::unique_ptr<T, Deleter>& wrapper) { wrapper.reset(); }

    template <typename... Args>
    static std::unique_ptr<T, Deleter> make(Args&&... args) {
        if constexpr (type_traits<T>::enabled && !std::is_pointer_v<T>)
            return std::unique_ptr<T, Deleter>(
                new T(detail::make_nested<T>(std::forward<Args>(args)...)));
        // Work around direct-initialization in smart-pointer-backed factories.
        else if constexpr (std::is_constructible_v<T, Args...>)
            return std::unique_ptr<T, Deleter>(
                new T(std::forward<Args>(args)...));
        else
            return std::unique_ptr<T, Deleter>(
                new T{std::forward<Args>(args)...});
    }

    static std::unique_ptr<T, Deleter> from_pointer(T* ptr) {
        return std::unique_ptr<T, Deleter>(ptr);
    }

    template <typename TargetType, typename Factory, typename Context>
    static TargetType& resolve_type(Factory& factory, Context& context,
                                    type_descriptor requested_type,
                                    type_descriptor registered_type) {
        if constexpr (std::is_same_v<TargetType, rebind_t<T>>)
            return factory.resolve(context);
        else
            throw detail::make_type_not_convertible_exception(requested_type,
                                                              registered_type,
                                                              context);
    }
};

template <typename Array>
struct type_traits<
    std::shared_ptr<Array>,
    std::enable_if_t<std::is_array_v<Array> && (std::extent_v<Array, 0> == 0)>> {
    static constexpr bool enabled = true;
    static constexpr bool is_pointer_like = true;
    static constexpr bool is_value_borrowable = false;

    template <typename Target>
    static constexpr bool is_handle_rebindable =
        std::is_constructible_v<Target, std::shared_ptr<Array>>;

    using value_type = std::remove_extent_t<Array>;

    template <typename U> using rebind_t = std::shared_ptr<U[]>;

    template <typename Target>
    static constexpr bool is_rebindable =
        std::is_constructible_v<std::remove_cv_t<Target>, std::shared_ptr<Array>>;

    static value_type* get(std::shared_ptr<Array>& wrapper) { return wrapper.get(); }

    static const value_type* get(const std::shared_ptr<Array>& wrapper) {
        return wrapper.get();
    }

    static bool empty(const std::shared_ptr<Array>& wrapper) {
        return wrapper.get() == nullptr;
    }

    static void reset(std::shared_ptr<Array>& wrapper) { wrapper.reset(); }

    template <typename... Args> static std::shared_ptr<Array> make(Args&&... args) {
        return std::shared_ptr<Array>(detail::make_dynamic_array<value_type>(
            std::forward<Args>(args)...));
    }

    static std::shared_ptr<Array> from_pointer(value_type* ptr) {
        return std::shared_ptr<Array>(ptr);
    }

    template <typename TargetType, typename Factory, typename Context>
    static TargetType& resolve_type(Factory& factory, Context& context,
                                    type_descriptor, type_descriptor) {
        return factory.template resolve<TargetType>(context);
    }
};

template <typename T>
struct type_traits<std::shared_ptr<T>, std::enable_if_t<!std::is_array_v<T>>> {
    static constexpr bool enabled = true;
    static constexpr bool is_pointer_like = true;
    static constexpr bool is_value_borrowable = true;

    template <typename Target>
    static constexpr bool is_handle_rebindable =
        std::is_constructible_v<Target, std::shared_ptr<T>>;

    using value_type = T;

    template <typename U> using rebind_t = std::shared_ptr<U>;

    template <typename Target>
    static constexpr bool is_rebindable =
        std::is_constructible_v<std::remove_cv_t<Target>, std::shared_ptr<T>>;

    static T* get(std::shared_ptr<T>& wrapper) { return wrapper.get(); }

    static const T* get(const std::shared_ptr<T>& wrapper) {
        return wrapper.get();
    }

    static T& borrow(std::shared_ptr<T>& wrapper) { return *wrapper; }

    static const T& borrow(const std::shared_ptr<T>& wrapper) {
        return *wrapper;
    }

    static bool empty(const std::shared_ptr<T>& wrapper) {
        return wrapper.get() == nullptr;
    }

    static void reset(std::shared_ptr<T>& wrapper) { wrapper.reset(); }

    template <typename... Args> static std::shared_ptr<T> make(Args&&... args) {
        if constexpr (type_traits<T>::enabled && !std::is_pointer_v<T>)
            return std::make_shared<T>(
                detail::make_nested<T>(std::forward<Args>(args)...));
        // Work around direct-initialization in std::make_shared().
        else if constexpr (std::is_constructible_v<T, Args...>)
            return std::make_shared<T>(std::forward<Args>(args)...);
        else
            return std::shared_ptr<T>(new T{std::forward<Args>(args)...});
    }

    static std::shared_ptr<T> from_pointer(T* ptr) {
        return std::shared_ptr<T>(ptr);
    }

    template <typename TargetType, typename Factory, typename Context>
    static TargetType& resolve_type(Factory& factory, Context& context,
                                    type_descriptor, type_descriptor) {
        return factory.template resolve<TargetType>(context);
    }
};

template <typename T> struct type_traits<std::optional<T>> {
    static constexpr bool enabled = true;
    static constexpr bool is_pointer_like = false;
    static constexpr bool is_value_borrowable = true;

    template <typename>
    static constexpr bool is_handle_rebindable = false;

    using value_type = T;

    template <typename U> using rebind_t = std::optional<U>;

    template <typename Target>
    static constexpr bool is_rebindable =
        std::is_same_v<Target, rebind_t<T>>;

    static T* get(std::optional<T>& wrapper) {
        return wrapper ? std::addressof(wrapper.value()) : nullptr;
    }

    static const T* get(const std::optional<T>& wrapper) {
        return wrapper ? std::addressof(wrapper.value()) : nullptr;
    }

    static T& borrow(std::optional<T>& wrapper) { return wrapper.value(); }

    static const T& borrow(const std::optional<T>& wrapper) {
        return wrapper.value();
    }

    static bool empty(const std::optional<T>& wrapper) {
        return !wrapper.has_value();
    }

    static void reset(std::optional<T>& wrapper) { wrapper.reset(); }

    template <typename... Args> static std::optional<T> make(Args&&... args) {
        if constexpr (type_traits<T>::enabled && !std::is_pointer_v<T>)
            return std::optional<T>{std::in_place,
                                    detail::make_nested<T>(
                                        std::forward<Args>(args)...)};
        else if constexpr (std::is_constructible_v<T, Args...>)
            return std::optional<T>{std::in_place, std::forward<Args>(args)...};
        else
            return std::optional<T>{std::in_place,
                                    T{std::forward<Args>(args)...}};
    }

    template <typename TargetType, typename Factory, typename Context>
    static TargetType& resolve_type(Factory& factory, Context& context,
                                    type_descriptor requested_type,
                                    type_descriptor registered_type) {
        if constexpr (std::is_same_v<TargetType, rebind_t<T>>)
            return factory.resolve(context);
        else
            throw detail::make_type_not_convertible_exception(requested_type,
                                                              registered_type,
                                                              context);
    }
};

namespace detail {
template <typename T, typename... Args> T make_nested(Args&&... args) {
    return type_traits<T>::make(std::forward<Args>(args)...);
}
} // namespace detail

} // namespace dingo
