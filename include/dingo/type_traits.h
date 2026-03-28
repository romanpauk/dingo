//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/config.h>

#include <dingo/exceptions.h>
#include <dingo/type_list.h>

#include <memory>
#include <optional>
#include <type_traits>
#include <utility>
#include <variant>

namespace dingo {
struct unique;
struct shared;
struct external;

template <class T> struct exact_lookup;

template <typename T, typename = void> struct type_traits {
    static constexpr bool enabled = false;
    static constexpr bool is_pointer_like = false;
    static constexpr bool is_value_borrowable = false;

    template <typename>
    static constexpr bool is_handle_rebindable = false;
};

namespace detail {
template <typename T, typename = void>
struct type_traits_reference_resolvability : std::false_type {};

template <typename T>
struct type_traits_reference_resolvability<
    T, std::void_t<decltype(type_traits<T>::is_reference_resolvable)>>
    : std::bool_constant<type_traits<T>::is_reference_resolvable> {};
} // namespace detail

template <typename T>
inline constexpr bool is_reference_resolvable_v =
    detail::type_traits_reference_resolvability<T>::value;

template <typename T>
inline constexpr bool is_pointer_like_type_v =
    type_traits<T>::enabled && type_traits<T>::is_pointer_like &&
    !std::is_pointer_v<T>;

template <typename Type, typename Selected, typename = void>
struct construction_traits {
    static constexpr bool enabled = false;
};

namespace detail {
template <typename Variant, typename Selected> struct variant_alternative_count;

template <typename Selected, typename... Alternatives>
struct variant_alternative_count<std::variant<Alternatives...>, Selected>
    : std::integral_constant<size_t,
                             (0u + ... + (std::is_same_v<Selected, Alternatives> ? 1u
                                                                                 : 0u))> {};
} // namespace detail

template <typename... Alternatives, typename Selected>
struct construction_traits<
    std::variant<Alternatives...>, Selected,
    std::enable_if_t<
        detail::variant_alternative_count<std::variant<Alternatives...>,
                                          Selected>::value == 1>> {
    static constexpr bool enabled = true;

    using type = std::variant<Alternatives...>;

    template <typename Value> static type wrap(Value&& value) {
        return type(std::in_place_type<Selected>, std::forward<Value>(value));
    }
};

template <typename StorageTag, typename Type, typename U, typename = void>
struct storage_traits {
    static constexpr bool enabled = false;

    using value_types = type_list<>;
    using lvalue_reference_types = type_list<>;
    using rvalue_reference_types = type_list<>;
    using pointer_types = type_list<>;
    using conversion_types = type_list<>;
};

template <typename StorageTag, typename Type, typename U>
struct storage_traits<StorageTag, const Type, U>
    : storage_traits<StorageTag, Type, U> {};

template <typename StorageTag, typename Type, typename U>
struct storage_traits<StorageTag, Type&, U> : storage_traits<StorageTag, Type, U> {};

template <typename StorageTag, typename Type, typename U>
struct storage_traits<StorageTag, const Type&, U>
    : storage_traits<StorageTag, Type, U> {};

namespace detail {
template <typename T, typename... Args> T make_nested(Args&&... args);

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

template <typename Type, typename U, typename = void>
struct wrapper_rebind_leaf {
    using type = U;
};

template <typename Type, typename U>
struct wrapper_rebind_leaf<
    Type, U,
    std::enable_if_t<type_traits<Type>::enabled && !std::is_pointer_v<Type>>> {
    using type = typename type_traits<Type>::template rebind_t<
        typename wrapper_rebind_leaf<typename type_traits<Type>::value_type,
                                     U>::type>;
};

template <typename Type, typename U>
using wrapper_rebind_leaf_t = typename wrapper_rebind_leaf<Type, U>::type;

template <typename Type> struct wrapper_lvalue_reference_type {
    using type = type_list<Type&>;
};

template <typename Type> struct wrapper_pointer_type {
    using type = type_list<Type*>;
};

template <typename Type> struct copyable_wrapper_value_type {
    using type = std::conditional_t<std::is_copy_constructible_v<Type>,
                                    type_list<Type>, type_list<>>;
};

template <template <typename> class TypeBuilder, typename Type, typename = void>
struct recursive_wrapper_types {
    using type = typename TypeBuilder<Type>::type;
};

template <template <typename> class TypeBuilder, typename Type>
struct recursive_wrapper_types<
    TypeBuilder,
    Type, std::enable_if_t<type_traits<Type>::enabled &&
                           type_traits<Type>::is_pointer_like>> {
    using type = type_list_cat_t<
        typename TypeBuilder<Type>::type,
        typename recursive_wrapper_types<
            TypeBuilder,
            typename type_traits<Type>::value_type>::type>;
};

template <typename Handle> struct wrapper_storage_types {
    using lvalue_reference_types =
        typename recursive_wrapper_types<wrapper_lvalue_reference_type,
                                         Handle>::type;
    using pointer_types =
        typename recursive_wrapper_types<wrapper_pointer_type, Handle>::type;
    using copyable_value_types =
        typename recursive_wrapper_types<copyable_wrapper_value_type,
                                         Handle>::type;
};
} // namespace detail

template <typename Type, typename U>
struct storage_traits<shared, Type*, U> {
    static constexpr bool enabled = true;

    using value_types = type_list<>;
    using lvalue_reference_types = type_list<U&>;
    using rvalue_reference_types = type_list<>;
    using pointer_types = type_list<U*>;
    using conversion_types = type_list<>;
};

template <typename Type, typename U>
struct storage_traits<external, Type*, U> {
    static constexpr bool enabled = true;

    using value_types = type_list<U>;
    using lvalue_reference_types = type_list<U&>;
    using rvalue_reference_types = type_list<>;
    using pointer_types = type_list<U*>;
    using conversion_types = type_list<>;
};

template <typename Type, typename U>
struct storage_traits<unique, Type*, U> {
    static constexpr bool enabled = true;

    using value_types = type_list<std::unique_ptr<U>, std::shared_ptr<U>>;
    using lvalue_reference_types = type_list<>;
    using rvalue_reference_types =
        type_list<std::unique_ptr<U>&&, std::shared_ptr<U>&&>;
    using pointer_types = type_list<U*>;
    using conversion_types = type_list<std::unique_ptr<U>, std::shared_ptr<U>>;
};

template <typename T> struct type_traits<T*> {
    static constexpr bool enabled = true;
    static constexpr bool is_pointer_like = true;
    static constexpr bool is_value_borrowable = true;

    template <typename Target>
    static constexpr bool is_handle_rebindable =
        std::is_constructible_v<Target, T*>;

    using value_type = T;

    template <typename U> using rebind_t = U*;

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
                                                              registered_type);
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
                                                              registered_type);
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
                                                              registered_type);
    }
};

template <typename T, typename Deleter>
struct type_traits<std::unique_ptr<T, Deleter>> {
    static constexpr bool enabled = true;
    static constexpr bool is_pointer_like = true;
    static constexpr bool is_value_borrowable = true;
    static constexpr bool is_reference_resolvable = true;

    template <typename Target>
    static constexpr bool is_handle_rebindable =
        std::is_constructible_v<Target, std::unique_ptr<T, Deleter>>;

    using value_type = T;

    template <typename U>
    using rebind_t = std::unique_ptr<U, std::default_delete<U>>;

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
                                                              registered_type);
    }
};

template <typename T, typename Deleter, typename U>
struct storage_traits<unique, std::unique_ptr<T, Deleter>, U> {
    static constexpr bool enabled = true;

    using rebound_handle =
        detail::wrapper_rebind_leaf_t<std::unique_ptr<T, Deleter>, U>;
    using inner_handle = detail::wrapper_rebind_leaf_t<T, U>;

    using value_types = type_list<rebound_handle, std::shared_ptr<inner_handle>>;
    using lvalue_reference_types = type_list<>;
    using rvalue_reference_types =
        type_list<rebound_handle&&, std::shared_ptr<inner_handle>&&>;
    using pointer_types = type_list<>;
    using conversion_types = type_list<rebound_handle, std::shared_ptr<inner_handle>>;
};

template <typename T, typename Deleter, typename U>
struct storage_traits<shared, std::unique_ptr<T, Deleter>, U> {
    static constexpr bool enabled = true;

    using handle_type =
        detail::wrapper_rebind_leaf_t<std::unique_ptr<T, Deleter>, U>;
    using types = detail::wrapper_storage_types<handle_type>;

    using value_types = type_list<U>;
    using lvalue_reference_types = typename types::lvalue_reference_types;
    using rvalue_reference_types = type_list<>;
    using pointer_types = typename types::pointer_types;
    using conversion_types = type_list<>;
};

template <typename T, typename Deleter, typename U>
struct storage_traits<external, std::unique_ptr<T, Deleter>, U> {
    static constexpr bool enabled = true;

    using handle_type =
        detail::wrapper_rebind_leaf_t<std::unique_ptr<T, Deleter>, U>;
    using types = detail::wrapper_storage_types<handle_type>;

    using value_types = type_list<>;
    using lvalue_reference_types = typename types::lvalue_reference_types;
    using rvalue_reference_types = type_list<>;
    using pointer_types = typename types::pointer_types;
    using conversion_types = type_list<>;
};

template <typename T> struct type_traits<std::shared_ptr<T>> {
    static constexpr bool enabled = true;
    static constexpr bool is_pointer_like = true;
    static constexpr bool is_value_borrowable = true;
    static constexpr bool is_reference_resolvable = true;

    template <typename Target>
    static constexpr bool is_handle_rebindable =
        std::is_constructible_v<Target, std::shared_ptr<T>>;

    using value_type = T;

    template <typename U> using rebind_t = std::shared_ptr<U>;

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

template <typename T, typename U>
struct storage_traits<unique, std::shared_ptr<T>, U> {
    static constexpr bool enabled = true;

    using rebound_handle = detail::wrapper_rebind_leaf_t<std::shared_ptr<T>, U>;

    using value_types = type_list<rebound_handle>;
    using lvalue_reference_types = type_list<>;
    using rvalue_reference_types = type_list<rebound_handle&&>;
    using pointer_types = type_list<>;
    using conversion_types = type_list<rebound_handle>;
};

template <typename T, typename U>
struct storage_traits<shared, std::shared_ptr<T>, U> {
    static constexpr bool enabled = true;

    using handle_type = detail::wrapper_rebind_leaf_t<std::shared_ptr<T>, U>;
    using types = detail::wrapper_storage_types<handle_type>;

    using value_types = type_list_cat_t<
        type_list<U>, typename types::copyable_value_types>;
    using lvalue_reference_types = typename types::lvalue_reference_types;
    using rvalue_reference_types = type_list<>;
    using pointer_types = typename types::pointer_types;
    using conversion_types = typename types::copyable_value_types;
};

template <typename T, typename U>
struct storage_traits<external, std::shared_ptr<T>, U> {
    static constexpr bool enabled = true;

    using handle_type = detail::wrapper_rebind_leaf_t<std::shared_ptr<T>, U>;
    using types = detail::wrapper_storage_types<handle_type>;

    using value_types = typename types::copyable_value_types;
    using lvalue_reference_types = typename types::lvalue_reference_types;
    using rvalue_reference_types = type_list<>;
    using pointer_types = typename types::pointer_types;
    using conversion_types = typename types::copyable_value_types;
};

template <typename T> struct type_traits<std::optional<T>> {
    static constexpr bool enabled = true;
    static constexpr bool is_pointer_like = false;
    static constexpr bool is_value_borrowable = true;

    template <typename>
    static constexpr bool is_handle_rebindable = false;

    using value_type = T;

    template <typename U> using rebind_t = std::optional<U>;

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
                                                              registered_type);
    }
};

template <typename T, typename U>
struct storage_traits<unique, std::optional<T>, U> {
    static constexpr bool enabled = true;

    using value_types = type_list<std::optional<U>>;
    using lvalue_reference_types = type_list<>;
    using rvalue_reference_types = type_list<std::optional<U>&&>;
    using pointer_types = type_list<>;
    using conversion_types = type_list<std::optional<U>>;
};

template <typename T, typename U>
struct storage_traits<shared, std::optional<T>, U> {
    static constexpr bool enabled = true;

    using value_types = type_list<>;
    using lvalue_reference_types =
        type_list<U&, exact_lookup<std::optional<T>>&>;
    using rvalue_reference_types = type_list<>;
    using pointer_types = type_list<U*, exact_lookup<std::optional<T>>*>;
    using conversion_types = type_list<>;
};

template <typename T, typename U>
struct storage_traits<external, std::optional<T>, U> {
    static constexpr bool enabled = true;

    using value_types = type_list<>;
    using lvalue_reference_types =
        type_list<U&, exact_lookup<std::optional<T>>&>;
    using rvalue_reference_types = type_list<>;
    using pointer_types = type_list<U*, exact_lookup<std::optional<T>>*>;
    using conversion_types = type_list<>;
};

template <typename... Ts>
struct storage_traits<unique, std::variant<Ts...>, std::variant<Ts...>> {
    static constexpr bool enabled = true;

    using value_types = type_list<std::variant<Ts...>>;
    using lvalue_reference_types = type_list<>;
    using rvalue_reference_types = type_list<std::variant<Ts...>&&>;
    using pointer_types = type_list<>;
    using conversion_types = type_list<std::variant<Ts...>>;
};

namespace detail {
template <typename T, typename... Args> T make_nested(Args&&... args) {
    return type_traits<T>::make(std::forward<Args>(args)...);
}
} // namespace detail

} // namespace dingo
