//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/core/key.h>

#include <type_traits>

namespace dingo {

class runtime_context;

template <typename StaticRegistry, bool RuntimeDependencies>
class basic_static_context;

namespace detail {

template <typename T, typename = void>
struct has_lookup_definition_type : std::false_type {};

template <typename T>
struct has_lookup_definition_type<
    T, std::void_t<typename T::lookup_definition_type>> : std::true_type {};

template <typename T,
          bool HasLookupDefinition = has_lookup_definition_type<T>::value>
struct container_lookup_definition_type {};

template <typename T> struct container_lookup_definition_type<T, true> {
  using type = typename T::lookup_definition_type;
};

template <typename T>
using container_lookup_definition_type_t =
    typename container_lookup_definition_type<T>::type;

template <typename T> struct is_static_context_argument : std::false_type {};

template <typename StaticRegistry, bool RuntimeDependencies>
struct is_static_context_argument<
    basic_static_context<StaticRegistry, RuntimeDependencies>>
    : std::true_type {};

template <typename T>
inline constexpr bool is_static_context_argument_v = is_static_context_argument<
    std::remove_cv_t<std::remove_reference_t<T>>>::value;

template <typename Container, typename Context, typename T,
          bool RemoveRvalueReferences, typename Selector, typename = void>
struct is_context_resolve_supported : std::false_type {};

template <typename Container, typename Context, typename T,
          bool RemoveRvalueReferences>
struct is_context_resolve_supported<
    Container, Context, T, RemoveRvalueReferences, void,
    std::void_t<decltype(std::declval<Container &>()
                             .template resolve<T, RemoveRvalueReferences>(
                                 std::declval<Context &>()))>>
    : std::true_type {};

template <typename Container, typename Context, typename T,
          bool RemoveRvalueReferences, typename Selector>
struct is_context_resolve_supported<
    Container, Context, T, RemoveRvalueReferences, Selector,
    std::void_t<decltype(std::declval<Container &>()
                             .template resolve<T, RemoveRvalueReferences>(
                                 std::declval<Context &>(),
                                 std::declval<Selector>()))>> : std::true_type {
};

template <typename Container, typename Context, typename T,
          bool RemoveRvalueReferences, typename Selector>
inline constexpr bool is_context_resolve_supported_v =
    is_context_resolve_supported<Container, Context, T, RemoveRvalueReferences,
                                 Selector>::value;

} // namespace detail
} // namespace dingo
