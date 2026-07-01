//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/registration/type_registration.h>

#include <type_traits>

namespace dingo {

template <typename StaticSource, typename ParentContainer = void>
class static_container;

namespace detail {

template <typename StaticRegistry, typename ParentContainer = void>
class container_with_static_bindings;

template <typename T> struct is_static_registry : std::false_type {};

template <typename... Registrations>
struct is_static_registry<static_registry<Registrations...>> : std::true_type {
};

template <typename T>
inline constexpr bool is_static_registry_v = is_static_registry<T>::value;

template <typename T> struct is_bindings_wrapper : std::false_type {};

template <typename... Args>
struct is_bindings_wrapper<::dingo::bindings<Args...>> : std::true_type {};

template <typename T>
inline constexpr bool is_bindings_wrapper_v = is_bindings_wrapper<T>::value;

template <typename T> struct bindings_wrapper_registry;

template <typename... Args>
struct bindings_wrapper_registry<::dingo::bindings<Args...>> {
  using type = typename ::dingo::bindings<Args...>::type;
};

template <typename T>
using bindings_wrapper_registry_t = typename bindings_wrapper_registry<T>::type;

} // namespace detail
} // namespace dingo
