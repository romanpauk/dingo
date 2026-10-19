//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/dependency/normalized_type.h>
#include <dingo/dependency/request.h>
#include <dingo/factory/auto_constructible.h>
#include <dingo/key/key.h>
#include <dingo/lookup/lookup.h>

#include <type_traits>

namespace dingo::detail {

template <typename Container, typename Request, typename LookupKey,
          typename = void>
struct has_key_value_lookup_definition : std::false_type {};

template <typename Container, typename Request, typename LookupKey>
struct has_key_value_lookup_definition<
    Container, Request, LookupKey,
    std::void_t<typename Container::lookup_definition_type>>
    : std::bool_constant<!std::is_void_v<selected_lookup_entry_t<
          normalized_type_t<Request>,
          typename std::decay_t<LookupKey>::definition_type,
          normalize_lookup_definitions_t<
              typename Container::lookup_definition_type>>>> {};

template <typename Container, typename Request, typename LookupKey>
inline constexpr bool has_key_value_lookup_definition_v =
    has_key_value_lookup_definition<Container, Request, LookupKey>::value;

// Keep constructibility queries in the non-reference specialization. MSVC
// otherwise evaluates its constructibility intrinsics for incomplete reference
// targets while instantiating the dispatch trait.
template <typename T,
          bool IsNormalized =
              std::is_same_v<request_value_t<T>, std::decay_t<T>>,
          bool IsReference = std::is_reference_v<T>>
struct is_runtime_auto_constructible_dependency : std::false_type {};

template <typename T>
struct is_runtime_auto_constructible_dependency<T, true, false>
    : std::bool_constant<std::is_move_constructible_v<std::decay_t<T>> ||
                         std::is_copy_constructible_v<std::decay_t<T>>> {};

template <typename T>
struct is_runtime_auto_constructible_dependency<T, true, true>
    : std::bool_constant<std::is_lvalue_reference_v<T> &&
                         std::is_const_v<std::remove_reference_t<T>> &&
                         is_auto_constructible<std::decay_t<T>>::value> {};

template <typename T>
inline constexpr bool is_runtime_auto_constructible_dependency_v =
    is_runtime_auto_constructible_dependency<T>::value;

template <typename Container, typename Request, typename LookupKey>
constexpr bool has_lookup() {
  static_assert(is_lookup_key_v<LookupKey>,
                "internal lookup queries require dingo::detail::lookup_key");
  using key_definition = typename std::decay_t<LookupKey>::definition_type;
  if constexpr (is_static_lookup_key_definition_v<key_definition>) {
    return true;
  } else {
    return has_key_value_lookup_definition_v<Container, Request, LookupKey>;
  }
}

} // namespace dingo::detail
