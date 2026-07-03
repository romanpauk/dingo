//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/detail/container_traits.h>
#include <dingo/registration/type_registration.h>
#include <dingo/rtti/static_provider.h>

#include <memory>
#include <tuple>
#include <type_traits>

namespace dingo {

template <typename Tag = void> struct static_container_traits {
  template <typename TagT> using rebind_t = static_container_traits<TagT>;

  using tag_type = Tag;
  using rtti_type = rtti<static_provider>;
  using allocator_type = std::allocator<char>;
  using lookup_definition_type = std::tuple<>;
};

template <typename StaticSource, typename ParentContainer = void>
class static_container;

namespace detail {

template <typename StaticBindings, typename ParentContainer = void>
class container_with_static_bindings;

template <typename T> struct is_static_bindings : std::false_type {};

template <typename... Registrations>
struct is_static_bindings<static_bindings<Registrations...>> : std::true_type {
};

template <typename T>
inline constexpr bool is_static_bindings_v = is_static_bindings<T>::value;

template <typename T, typename = void>
struct is_static_container_traits : std::false_type {};

template <typename T>
struct is_static_container_traits<
    T, std::void_t<container_lookup_definition_type_t<T>,
                   typename T::allocator_type, typename T::rtti_type>>
    : std::true_type {};

template <typename T>
inline constexpr bool is_static_container_traits_v =
    is_static_container_traits<T>::value;

template <typename T, typename = void> struct static_container_traits {
  using type = ::dingo::static_container_traits<>;
};

template <typename T>
struct static_container_traits<T,
                               std::void_t<typename T::container_traits_type>> {
  using type = typename T::container_traits_type;
};

template <typename T>
using static_container_traits_t = typename static_container_traits<T>::type;

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
