//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/core/keyed.h>

#include <type_traits>

namespace dingo {

class runtime_context;

template <typename StaticRegistry, bool RuntimeDependencies>
class basic_static_context;

namespace detail {

template <typename T> struct is_static_context_argument : std::false_type {};

template <typename StaticRegistry, bool RuntimeDependencies>
struct is_static_context_argument<
    basic_static_context<StaticRegistry, RuntimeDependencies>>
    : std::true_type {};

template <typename T>
inline constexpr bool is_static_context_argument_v = is_static_context_argument<
    std::remove_cv_t<std::remove_reference_t<T>>>::value;

template <typename Container, typename Context, typename T,
          bool RemoveRvalueReferences, bool CheckCache, typename Selector,
          typename = void>
struct is_context_resolve_supported : std::false_type {};

template <typename Container, typename Context, typename T,
          bool RemoveRvalueReferences, bool CheckCache>
struct is_context_resolve_supported<
    Container, Context, T, RemoveRvalueReferences, CheckCache, void,
    std::void_t<
        decltype(std::declval<Container &>()
                     .template resolve<T, RemoveRvalueReferences, CheckCache>(
                         std::declval<Context &>()))>> : std::true_type {};

template <typename Container, typename Context, typename T,
          bool RemoveRvalueReferences, bool CheckCache, typename Selector>
struct is_context_resolve_supported<
    Container, Context, T, RemoveRvalueReferences, CheckCache, Selector,
    std::void_t<
        decltype(std::declval<Container &>()
                     .template resolve<T, RemoveRvalueReferences, CheckCache>(
                         std::declval<Context &>(), std::declval<Selector>()))>>
    : std::true_type {};

template <typename Container, typename Context, typename T,
          bool RemoveRvalueReferences, bool CheckCache, typename Selector>
inline constexpr bool is_context_resolve_supported_v =
    is_context_resolve_supported<Container, Context, T, RemoveRvalueReferences,
                                 CheckCache, Selector>::value;

} // namespace detail
} // namespace dingo
