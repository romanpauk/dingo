//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/core/config.h>

#include <dingo/factory/callable.h>
#include <dingo/factory/constructor.h>
#include <dingo/factory/function.h>
#include <dingo/type/type_list.h>

#include <type_traits>

namespace dingo {

namespace detail {
template <typename, typename = void> struct has_factory_arguments : std::false_type {};

template <typename Factory>
struct has_factory_arguments<Factory, std::void_t<typename Factory::arguments>>
    : std::true_type {};

template <typename Factory> struct is_plain_constructor_factory : std::false_type {};

template <typename T>
struct is_plain_constructor_factory<constructor<T>>
    : std::bool_constant<!std::is_function_v<T>> {};

template <typename Signature> struct callable_dependencies;

template <typename R, typename... Args>
struct callable_dependencies<R(Args...)> {
    using type = type_list<Args...>;
};

template <typename R, typename... Args>
struct callable_dependencies<R(Args...) noexcept>
    : callable_dependencies<R(Args...)> {};

template <typename Signature>
using callable_dependencies_t =
    typename callable_dependencies<Signature>::type;

template <typename Factory, typename = void>
struct factory_arguments_or_void {
    using type = void;
};

template <typename Factory>
struct factory_arguments_or_void<Factory,
                                 std::void_t<typename Factory::arguments>> {
    using type = typename Factory::arguments;
};

template <typename Factory>
using factory_arguments_or_void_t =
    typename factory_arguments_or_void<Factory>::type;
} // namespace detail

template <typename Factory, typename = void> struct factory_traits {
    using dependencies = void;
    static constexpr bool has_explicit_dependencies = false;
    static constexpr bool is_compile_time_bindable = false;
};

template <typename Factory>
struct factory_traits<
    Factory,
    std::enable_if_t<detail::has_factory_arguments<Factory>::value &&
                     !detail::is_plain_constructor_factory<Factory>::value>> {
    using dependencies = typename Factory::arguments;
    static constexpr bool has_explicit_dependencies = true;
    static constexpr bool is_compile_time_bindable = true;
};

template <typename T>
struct factory_traits<constructor<T>, std::enable_if_t<!std::is_function_v<T>>> {
    using dependencies = detail::factory_arguments_or_void_t<constructor<T>>;
    static constexpr bool has_explicit_dependencies = false;
    static constexpr bool is_compile_time_bindable =
        constructor<T>::kind == detail::constructor_kind::concrete;
};

template <typename T, T fn>
struct factory_traits<function_decl<T, fn>> {
    using dependencies =
        detail::callable_dependencies_t<detail::callable_signature_t<T>>;
    static constexpr bool has_explicit_dependencies = true;
    static constexpr bool is_compile_time_bindable = true;
};

template <auto fn>
struct factory_traits<function<fn>>
    : factory_traits<function_decl<decltype(fn), fn>> {};

template <typename Signature, typename T>
struct factory_traits<detail::callable_factory<Signature, T>> {
    using dependencies = detail::callable_dependencies_t<Signature>;
    static constexpr bool has_explicit_dependencies = true;
    static constexpr bool is_compile_time_bindable = false;
};

} // namespace dingo
