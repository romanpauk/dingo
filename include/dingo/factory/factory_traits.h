//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/base/config.h>

#include <dingo/base/type_list.h>
#include <dingo/factory/callable.h>
#include <dingo/factory/constructor.h>
#include <dingo/factory/function.h>

#include <type_traits>

namespace dingo {

namespace detail {
template <typename, typename = void>
struct has_factory_arguments : std::false_type {};

template <typename Factory>
struct has_factory_arguments<Factory, std::void_t<typename Factory::arguments>>
    : std::true_type {};

template <typename Factory>
struct is_plain_constructor_factory : std::false_type {};

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
using callable_dependencies_t = typename callable_dependencies<Signature>::type;

} // namespace detail

template <typename Factory, typename = void> struct factory_traits {
  using dependencies = void;
  static constexpr bool is_compile_time_bindable = false;
};

template <typename Factory>
struct factory_traits<
    Factory,
    std::enable_if_t<detail::has_factory_arguments<Factory>::value &&
                     !detail::is_plain_constructor_factory<Factory>::value>> {
  using dependencies = typename Factory::arguments;
  static constexpr bool is_compile_time_bindable = true;
};

template <typename T>
struct factory_traits<constructor<T>,
                      std::enable_if_t<!std::is_function_v<T>>> {
  using dependencies = typename constructor<T>::arguments;
  static constexpr bool is_compile_time_bindable =
      constructor<T>::kind == detail::constructor_kind::concrete;
};

template <auto fn> struct factory_traits<function<fn>> {
  using dependencies = detail::callable_dependencies_t<
      detail::callable_signature_t<decltype(fn)>>;
  static constexpr bool is_compile_time_bindable = true;
};

template <typename Signature, typename T>
struct factory_traits<detail::callable_factory<Signature, T>> {
  using dependencies = detail::callable_dependencies_t<Signature>;
  static constexpr bool is_compile_time_bindable = false;
};

namespace detail {
// True only when the factory's dependency list is known to be empty. Detected
// constructors report type_list<> for arity zero and void otherwise, so a
// void list is treated as possibly having dependencies.
template <typename Factory>
inline constexpr bool factory_without_dependencies_v =
    std::is_same_v<typename factory_traits<Factory>::dependencies, type_list<>>;
} // namespace detail

} // namespace dingo
