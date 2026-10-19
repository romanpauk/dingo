//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/base/construction_scope.h>
#include <dingo/type/collection_traits.h>
#include <dingo/type/key.h>
#include <dingo/type/normalized_type.h>

#include <cstddef>
#include <type_traits>
#include <utility>

namespace dingo {
namespace detail {

template <typename Collection, typename Fn>
using collection_insert_invocable =
    std::is_invocable<Fn &, Collection &,
                      typename collection_traits<Collection>::resolve_type>;

// The collection operations a container offers, in the spellings its users
// write them: without a key, with a type key, with a runtime key, with or
// without a function that inserts the resolved values. Each spelling is
// reduced to a lookup key. The runtime and the static container provide the
// rest:
//
//   construct_collection_by_key<T>(key)
//   construct_collection_by_key<T>(fn, key)
//   append_collection<T>(scope, results, context, fn, key)
//   count_collection<T>(key)
template <typename Derived> class container_api {
public:
  template <typename T> T construct_collection() {
    return derived().template construct_collection_by_key<T>(no_lookup_key());
  }

  template <typename T, typename Fn,
            std::enable_if_t<
                !is_lookup_key_v<std::decay_t<Fn>> && !is_key_value_v<Fn> &&
                    collection_insert_invocable<T, std::decay_t<Fn>>::value,
                int> = 0>
  T construct_collection(Fn &&fn) {
    return derived().template construct_collection_by_key<T>(
        std::forward<Fn>(fn), no_lookup_key());
  }

  template <typename T, typename IdType,
            std::enable_if_t<
                !collection_insert_invocable<T, std::decay_t<IdType>>::value,
                int> = 0>
  T construct_collection(IdType &&id) {
    return derived().template construct_collection_by_key<T>(
        make_lookup_key(std::forward<IdType>(id)));
  }

  template <typename T, typename Fn, typename IdType>
  T construct_collection(Fn &&fn, IdType &&id) {
    return derived().template construct_collection_by_key<T>(
        std::forward<Fn>(fn), make_lookup_key(std::forward<IdType>(id)));
  }

  template <typename T, typename Fn, typename Context>
  std::size_t append_collection(construction_scope scope, T &results,
                                Context &context, Fn &&fn) {
    return derived().template append_collection<T>(
        scope, results, context, std::forward<Fn>(fn), no_lookup_key());
  }

  template <typename T, typename Key, typename Fn, typename Context>
  std::size_t append_collection(construction_scope scope, T &results,
                                Context &context, Fn &&fn) {
    return derived().template append_collection<T>(
        scope, results, context, std::forward<Fn>(fn),
        make_lookup_key(type_selector<Key>{}));
  }

  template <typename T> std::size_t count_collection() {
    return derived().template count_collection<T>(no_lookup_key());
  }

  template <typename T, typename Key> std::size_t count_collection() {
    return derived().template count_collection<T>(
        make_lookup_key(type_selector<Key>{}));
  }

protected:
  container_api() = default;
  ~container_api() = default;

private:
  Derived &derived() { return static_cast<Derived &>(*this); }
};

} // namespace detail
} // namespace dingo
