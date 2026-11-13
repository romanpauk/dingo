//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/base/type_list.h>
#include <dingo/lookup/lookup_key.h>
#include <dingo/type/collection_traits.h>
#include <dingo/type/shape.h>

#include <cstddef>
#include <type_traits>
#include <utility>

namespace dingo::detail {

struct binding_collection_append {
  template <typename Collection, typename Value>
  void operator()(Collection &collection, Value &&value) const {
    collection_traits<std::decay_t<Collection>>::add(
        collection, std::forward<Value>(value));
  }
};

template <typename Collection, typename Value, typename = void>
struct default_collection_append : std::false_type {};

template <typename Collection, typename Value>
struct default_collection_append<
    Collection, Value,
    std::void_t<typename std::decay_t<Collection>::value_type>>
    : std::bool_constant<std::is_constructible_v<
          typename std::decay_t<Collection>::value_type, Value>> {};

template <typename Collection>
inline constexpr bool default_collection_append_v = default_collection_append<
    Collection, typename collection_traits<Collection>::resolve_type>::value;

template <typename StaticRegistry, typename T, typename LookupKey>
constexpr std::size_t static_collection_binding_count() {
  static_assert(is_lookup_key_v<LookupKey>);
  return type_list_size_v<typename StaticRegistry::template bindings<
      normalized_type_t<typename collection_traits<T>::resolve_type>,
      LookupKey>>;
}

} // namespace dingo::detail
