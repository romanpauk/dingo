//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/config.h>
#include <dingo/type_traits.h>

#include <memory>
#include <vector>

namespace dingo {

// TODO: clean up the templates
template <typename RTTI, typename T> struct class_instance_factory_traits {
    template <typename Factory, typename Context>
    static T resolve(Factory& factory, Context& context) {
        // TODO: this assumes that when resolve a value category that can't be
        // copied, we can move it, as the storage is unique anyway, so the
        // destruction by move does not matter. But at this place we have no
        // clue if the storage is unique or not.
        // TODO: test shared storage with non-copyable type requested as value
        if constexpr (has_value_type_v<T>) {
            if constexpr (std::is_copy_constructible_v<
                              typename T::value_type>) {
                return *static_cast<T*>(factory.get_value(
                    context, RTTI::template get_type_index<
                                 rebind_type_t<T, runtime_type>>()));
            }
        } else if constexpr (std::is_copy_constructible_v<T>) {
            return *static_cast<T*>(factory.get_value(
                context, RTTI::template get_type_index<
                             rebind_type_t<T, runtime_type>>()));
        }

        return std::move(*static_cast<T*>(factory.get_value(
            context,
            RTTI::template get_type_index<rebind_type_t<T, runtime_type>>())));
    }
};

template <typename RTTI, typename T>
struct class_instance_factory_traits<RTTI, T&> {
    template <typename Factory, typename Context>
    static T& resolve(Factory& factory, Context& context) {
        return *static_cast<T*>(factory.get_lvalue_reference(
            context,
            RTTI::template get_type_index<rebind_type_t<T&, runtime_type>>()));
    }
};

template <typename RTTI, typename T>
struct class_instance_factory_traits<RTTI, T&&> {
    template <typename Factory, typename Context>
    static T&& resolve(Factory& factory, Context& context) {
        return std::move(*static_cast<T*>(factory.get_rvalue_reference(
            context, RTTI::template get_type_index<
                         rebind_type_t<T&&, runtime_type>>())));
    }
};

template <typename RTTI, typename T>
struct class_instance_factory_traits<RTTI, T*> {
    template <typename Factory, typename Context>
    static T* resolve(Factory& factory, Context& context) {
        return static_cast<T*>(factory.get_pointer(
            context,
            RTTI::template get_type_index<rebind_type_t<T*, runtime_type>>()));
    }
};

} // namespace dingo