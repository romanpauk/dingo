//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/config.h>
#include <dingo/rebind_type.h>
#include <dingo/type_descriptor.h>

#include <type_traits>

namespace dingo {
namespace detail {
template <typename T, typename = void>
struct factory_result_has_value_type : std::false_type {};

template <typename T>
struct factory_result_has_value_type<T, std::void_t<typename T::value_type>>
    : std::true_type {};

template <typename T> constexpr bool factory_result_copies_from_storage() {
    if constexpr (type_traits<T>::enabled && is_pointer_like_type_v<T> &&
                  std::is_copy_constructible_v<T>) {
        return true;
    } else if constexpr (factory_result_has_value_type<T>::value) {
        return std::is_copy_constructible_v<typename T::value_type>;
    } else {
        return std::is_copy_constructible_v<T>;
    }
}
} // namespace detail

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4702)
#endif
// TODO: clean up the templates
template <typename RTTI, typename T> struct class_instance_factory_traits {
    static T convert(void* ptr) {
        if constexpr (detail::factory_result_copies_from_storage<T>()) {
            return *static_cast<T*>(ptr);
        }

        return std::move(*static_cast<T*>(ptr));
    }

    template <typename Factory, typename Context>
    static void* resolve(Factory& factory, Context& context) {
        return factory.get_value(
            context, RTTI::template get_type_index<rebind_leaf_t<T, runtime_type>>(),
            describe_type<T>());
    }
};
#ifdef _MSC_VER
#pragma warning(pop)
#endif

template <typename RTTI, typename T>
struct class_instance_factory_traits<RTTI, T&> {
    static T& convert(void* ptr) { return *static_cast<T*>(ptr); }

    template <typename Factory, typename Context>
    static void* resolve(Factory& factory, Context& context) {
        return factory.get_lvalue_reference(
            context, RTTI::template get_type_index<rebind_leaf_t<T&, runtime_type>>(),
            describe_type<T&>());
    }
};

template <typename RTTI, typename T>
struct class_instance_factory_traits<RTTI, const T&> {
    static T& convert(void* ptr) { return *static_cast<T*>(ptr); }

    template <typename Factory, typename Context>
    static void* resolve(Factory& factory, Context& context) {
        return factory.get_lvalue_reference(
            context, RTTI::template get_type_index<rebind_leaf_t<T&, runtime_type>>(),
            describe_type<const T&>());
    }
};

template <typename RTTI, typename T>
struct class_instance_factory_traits<RTTI, T&&> {
    static T&& convert(void* ptr) { return std::move(*static_cast<T*>(ptr)); }

    template <typename Factory, typename Context>
    static void* resolve(Factory& factory, Context& context) {
        return factory.get_rvalue_reference(
            context, RTTI::template get_type_index<rebind_leaf_t<T&&, runtime_type>>(),
            describe_type<T&&>());
    }
};

template <typename RTTI, typename T>
struct class_instance_factory_traits<RTTI, T*> {
    static T* convert(void* ptr) { return static_cast<T*>(ptr); }

    template <typename Factory, typename Context>
    static void* resolve(Factory& factory, Context& context) {
        return factory.get_pointer(
            context, RTTI::template get_type_index<rebind_leaf_t<T*, runtime_type>>(),
            describe_type<T*>());
    }
};

} // namespace dingo
