//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/base/exceptions.h>
#include <dingo/base/type_list.h>
#include <dingo/type/collection_traits.h>
#include <dingo/type/key.h>
#include <dingo/type/normalized_type.h>

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

template <typename T, typename RuntimeAppendFn, typename StaticAppendFn,
          typename Fn>
std::size_t append_binding_collection(T &results,
                                      RuntimeAppendFn &&runtime_append,
                                      StaticAppendFn &&static_append, Fn &&fn) {
  std::size_t count = 0;
  count += std::forward<RuntimeAppendFn>(runtime_append)(results, fn);
  count += std::forward<StaticAppendFn>(static_append)(results, fn);
  return count;
}

// Constructs a collection from the bindings of one or more sources. The caller
// sums the sources into total and supplies append, which fills the collection
// from each source in turn. An empty collection is an error unless the lookup
// that produced it was declared, which allow_empty states.
template <typename T, typename AppendFn>
T construct_binding_collection(std::size_t total, bool allow_empty,
                               AppendFn &&append) {
  using collection_type = collection_traits<T>;
  using resolve_type = typename collection_type::resolve_type;

  static_assert(collection_type::is_collection,
                "missing collection_traits specialization for type T");

  if (total == 0 && !allow_empty) {
    throw detail::make_collection_type_not_found_exception<T, resolve_type>();
  }

  T results;
  collection_type::reserve(results, total);
  std::forward<AppendFn>(append)(results);
  return results;
}

} // namespace dingo::detail
