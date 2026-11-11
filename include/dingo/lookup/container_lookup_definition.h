//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/type/key.h>

#include <type_traits>

namespace dingo {

namespace detail {

template <typename T>
using container_lookup_definition_type_t = typename T::lookup_definition_type;

// Containers that keep their state behind a movable handle are used by their
// implementation wherever another container or a binding refers to them.
template <typename T, typename = void> struct container_implementation {
  using type = T;

  static T *get(T *container) noexcept { return container; }
};

template <typename T>
struct container_implementation<T,
                                std::void_t<typename T::implementation_type>> {
  using type = typename T::implementation_type;

  static type *get(T *container) noexcept {
    return container != nullptr ? container->impl_ : nullptr;
  }
};

template <typename T>
using container_implementation_t = typename container_implementation<T>::type;

template <typename T>
container_implementation_t<T> *container_implementation_of(T *container) {
  return container_implementation<T>::get(container);
}

template <typename T, typename = void>
inline constexpr bool names_container_traits_v = false;

template <typename T>
inline constexpr bool names_container_traits_v<
    T, std::void_t<typename T::container_traits_type>> = true;

// Whether T is a container traits type: it names an allocator, an rtti and
// optionally lookup definitions. Both containers take one. A container names
// these too, and its traits as well: it is not a traits type.
template <typename T, typename = void>
struct is_container_traits : std::false_type {};

template <typename T>
struct is_container_traits<
    T, std::void_t<typename T::rtti_type, typename T::allocator_type,
                   container_lookup_definition_type_t<T>>>
    : std::bool_constant<!names_container_traits_v<T>> {};

template <typename T>
inline constexpr bool is_container_traits_v = is_container_traits<T>::value;

} // namespace detail
} // namespace dingo
