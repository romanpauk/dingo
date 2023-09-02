#pragma once

#include <dingo/config.h>

#include <memory>

namespace dingo {
// TODO: clean up the templates
template <typename RTTI, typename T> struct class_instance_traits {
    template <typename Factory, typename Context> static T resolve(Factory& factory, Context& context) {
        return *static_cast<T*>(
            factory.get_value(context, RTTI::template get_type_index<rebind_type_t<T, runtime_type>>()));
    }
};

template <typename RTTI, typename T> struct class_instance_traits<RTTI, T&> {
    template <typename Factory, typename Context> static T& resolve(Factory& factory, Context& context) {
        return *static_cast<T*>(
            factory.get_lvalue_reference(context, RTTI::template get_type_index<rebind_type_t<T&, runtime_type>>()));
    }
};

template <typename RTTI, typename T> struct class_instance_traits<RTTI, T&&> {
    template <typename Factory, typename Context> static T&& resolve(Factory& factory, Context& context) {
        return std::move(*static_cast<T*>(
            factory.get_rvalue_reference(context, RTTI::template get_type_index<rebind_type_t<T&&, runtime_type>>())));
    }
};

template <typename RTTI, typename T> struct class_instance_traits<RTTI, T*> {
    template <typename Factory, typename Context> static T* resolve(Factory& factory, Context& context) {
        return static_cast<T*>(
            factory.get_pointer(context, RTTI::template get_type_index<rebind_type_t<T*, runtime_type>>()));
    }
};

template <typename RTTI, typename T> struct class_instance_traits<RTTI, std::unique_ptr<T>> {
    template <typename Factory, typename Context>
    static std::unique_ptr<T> resolve(Factory& factory, Context& context) {
        return std::move(*static_cast<std::unique_ptr<T>*>(factory.get_value(
            context, RTTI::template get_type_index<rebind_type_t<std::unique_ptr<T>, runtime_type>>())));
    }
};
} // namespace dingo