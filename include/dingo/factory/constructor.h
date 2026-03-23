//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/config.h>

#include <dingo/class_traits.h>
#include <dingo/decay.h>
#include <dingo/factory/constructor_detection.h>

#include <tuple>
#include <utility>

namespace dingo {

template <typename...> struct constructor;

template <typename T, typename... Args> struct constructor<T(Args...)> {
    using arguments = std::tuple<Args...>;
    using direct_argument_types = std::tuple<Args...>;
    using constructed_type = T;
    static constexpr size_t arity = sizeof...(Args);
    static constexpr bool valid =
        detail::is_list_initializable_v<T, Args...> ||
        detail::is_direct_initializable_v<T, Args...>;

    template <typename Type, typename Context, typename Container>
    static Type construct(Context& ctx, Container& container) {
        return detail::construction_traits<Type, T>::construct(
            ctx.template resolve<Args>(container)...);
    }

    template <typename Type, typename Context, typename Container>
    static void construct(void* ptr, Context& ctx, Container& container) {
        detail::construction_traits<Type, T>::construct(
            ptr, ctx.template resolve<Args>(container)...);
    }
};

template <typename T, typename... Args>
struct constructor<T, Args...> : constructor<T(Args...)> {};

namespace detail {

template <typename Factory, typename = void>
struct has_factory_argument_types : std::false_type {};

template <typename Factory>
struct has_factory_argument_types<Factory,
                                  std::void_t<typename Factory::direct_argument_types>>
    : std::true_type {};

template <typename Factory>
static constexpr bool has_factory_argument_types_v =
    has_factory_argument_types<Factory>::value;

template <typename Factory, typename = void>
struct has_factory_constructed_type : std::false_type {};

template <typename Factory>
struct has_factory_constructed_type<Factory,
                                    std::void_t<typename Factory::constructed_type>>
    : std::true_type {};

template <typename Factory>
static constexpr bool has_factory_constructed_type_v =
    has_factory_constructed_type<Factory>::value;

template <typename Type, typename Factory, typename Context, typename Container,
          std::size_t... Indices>
Type construct_factory_arguments(Context& context, Container& container,
                                 std::index_sequence<Indices...>) {
    using argument_types = typename Factory::direct_argument_types;
    if constexpr (has_factory_constructed_type_v<Factory>) {
        return construction_traits<Type, typename Factory::constructed_type>::construct(
            context.template resolve<std::tuple_element_t<Indices, argument_types>>(
                container)...);
    } else {
        return class_traits<Type>::construct(
            context.template resolve<std::tuple_element_t<Indices, argument_types>>(
                container)...);
    }
}

template <typename Type, typename Factory, typename Context, typename Container,
          std::size_t... Indices>
void construct_factory_arguments(void* ptr, Context& context, Container& container,
                                 std::index_sequence<Indices...>) {
    using argument_types = typename Factory::direct_argument_types;
    if constexpr (has_factory_constructed_type_v<Factory>) {
        construction_traits<Type, typename Factory::constructed_type>::construct(
            ptr,
            context.template resolve<std::tuple_element_t<Indices, argument_types>>(
                container)...);
    } else {
        class_traits<Type>::construct(
            ptr,
            context.template resolve<std::tuple_element_t<Indices, argument_types>>(
                container)...);
    }
}

template <typename Type, typename Factory, typename Context, typename Container>
Type construct_factory(Context& context, Container& container, Factory& factory) {
    if constexpr (has_factory_argument_types_v<Factory>) {
        return construct_factory_arguments<Type, Factory>(
            context, container,
            std::make_index_sequence<
                std::tuple_size_v<typename Factory::direct_argument_types>>{});
    } else {
        return factory.template construct<Type>(context, container);
    }
}

template <typename Type, typename Factory, typename Context, typename Container>
void construct_factory(void* ptr, Context& context, Container& container,
                       Factory& factory) {
    if constexpr (has_factory_argument_types_v<Factory>) {
        construct_factory_arguments<Type, Factory>(
            ptr, context, container,
            std::make_index_sequence<
                std::tuple_size_v<typename Factory::direct_argument_types>>{});
    } else {
        factory.template construct<Type>(ptr, context, container);
    }
}

} // namespace detail

} // namespace dingo
