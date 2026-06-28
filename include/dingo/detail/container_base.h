//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/runtime/container_traits.h>
#include <dingo/runtime_container.h>
#include <dingo/static/container_traits.h>

#include <type_traits>

namespace dingo::detail {

template <typename... Ts>
inline constexpr bool container_dependent_false_v = false;

template <typename... Params> struct container_base;
template <typename Param, typename Enable = void>
struct container_base_from_parameter;
template <typename Param, typename Parent, typename Enable = void>
struct container_base_from_static_parent;
struct container_base_runtime_two_parameter_tag {};
struct container_base_static_parent_tag {};
struct container_base_invalid_two_parameter_tag {};

template <typename First>
using container_base_two_parameter_tag_t = std::conditional_t<
    is_runtime_container_traits_v<First>, container_base_runtime_two_parameter_tag,
    std::conditional_t<is_static_registry_v<First> ||
                           is_bindings_wrapper_v<First>,
                       container_base_static_parent_tag,
                       container_base_invalid_two_parameter_tag>>;

template <> struct container_base<> {
    using type = runtime_container<dynamic_container_traits>;
};

template <typename Param> struct container_base<Param> {
    using type = typename container_base_from_parameter<Param>::type;
};

template <typename First, typename Second>
struct container_base<First, Second> {
    using type = typename container_base<
        container_base_two_parameter_tag_t<First>, First, Second>::type;
};

template <typename First, typename Second>
struct container_base<container_base_runtime_two_parameter_tag, First,
                      Second> {
    using type = runtime_container<First, Second, void>;
};

template <typename First, typename Second>
struct container_base<container_base_static_parent_tag, First, Second> {
    using type = typename container_base_from_static_parent<First, Second>::type;
};

template <typename First, typename Second>
struct container_base<container_base_invalid_two_parameter_tag, First,
                      Second> {
    static_assert(
        container_dependent_false_v<First, Second>,
        "container<T, U> requires runtime traits + allocator or "
        "compile-time bindings + parent container");
};

template <typename First, typename Second, typename Third>
struct container_base<First, Second, Third> {
    static_assert(
        is_runtime_container_traits_v<First>,
        "container<T, U, V> requires runtime traits + allocator + parent");
    using type = runtime_container<First, Second, Third>;
};

template <typename First, typename Second, typename Third, typename... Rest>
struct container_base<First, Second, Third, Rest...> {
    static_assert(container_dependent_false_v<First, Second, Third, Rest...>,
                  "container<...> expects runtime traits or a single static "
                  "bindings source");
};

template <typename... Params>
using container_base_t = typename container_base<Params...>::type;

template <typename Param, typename Enable>
struct container_base_from_parameter {
    static_assert(container_dependent_false_v<Param>,
                  "container<T> requires runtime container traits or "
                  "compile-time bindings<...>");
};

template <typename Param>
struct container_base_from_parameter<
    Param, std::enable_if_t<is_runtime_container_traits_v<Param>>> {
    using type = runtime_container<Param, typename Param::allocator_type, void>;
};

template <typename Param>
struct container_base_from_parameter<
    Param, std::enable_if_t<is_static_registry_v<Param>>> {
    using type = container_with_static_bindings<Param>;
};

template <typename Param>
struct container_base_from_parameter<
    Param, std::enable_if_t<is_bindings_wrapper_v<Param>>> {
    using static_registry_type = bindings_wrapper_registry_t<Param>;

    static_assert(is_static_registry_v<static_registry_type>,
                  "container<bindings<...>> requires a valid compile-time "
                  "bindings source");

    using type = container_with_static_bindings<static_registry_type>;
};

template <typename Param, typename Parent, typename Enable>
struct container_base_from_static_parent {
    static_assert(container_dependent_false_v<Param, Parent>,
                  "container<bindings<...>, Parent> requires a valid "
                  "compile-time bindings source");
};

template <typename Param, typename Parent>
struct container_base_from_static_parent<
    Param, Parent, std::enable_if_t<is_static_registry_v<Param>>> {
    using type = container_with_static_bindings<Param, Parent>;
};

template <typename Param, typename Parent>
struct container_base_from_static_parent<
    Param, Parent, std::enable_if_t<is_bindings_wrapper_v<Param>>> {
    using static_registry_type = bindings_wrapper_registry_t<Param>;

    static_assert(is_static_registry_v<static_registry_type>,
                  "container<bindings<...>, Parent> requires a valid "
                  "compile-time bindings source");

    using type = container_with_static_bindings<static_registry_type, Parent>;
};

} // namespace dingo::detail
