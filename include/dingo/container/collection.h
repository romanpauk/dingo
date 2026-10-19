//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/base/construction_scope.h>
#include <dingo/base/exceptions.h>
#include <dingo/base/none.h>
#include <dingo/binding/collection.h>
#include <dingo/dependency/collection.h>
#include <dingo/key/key.h>
#include <dingo/key/selected.h>
#include <dingo/lookup/lookup.h>
#include <dingo/runtime/context.h>

#include <cstddef>
#include <type_traits>
#include <utility>

namespace dingo {
namespace detail {

// Collection construction, appending and counting of the container front-end.
// The mixin owns the collection overload set and drives it through the host
// container, which provides the runtime registry, the static registry and the
// parent container.
template <typename Derived, typename RuntimeContext>
class container_collection_api {
  using runtime_context_type = RuntimeContext;

  Derived &self() { return static_cast<Derived &>(*this); }

protected:
  template <typename T, typename Fn, typename LookupKey,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  T construct_collection_scoped(construction_scope scope,
                                runtime_context_type &context, Fn &&fn,
                                LookupKey key) {
    return construct_collection_impl<T>(scope, context, std::move(key),
                                        std::forward<Fn>(fn));
  }

public:
  template <typename T> T construct_collection() {
    return construct_collection<T>(detail::no_lookup_key());
  }

  template <
      typename T, typename Fn,
      std::enable_if_t<!detail::is_lookup_key_v<std::decay_t<Fn>>, int> = 0>
  T construct_collection(Fn &&fn) {
    return construct_collection<T>(std::forward<Fn>(fn),
                                   detail::no_lookup_key());
  }

  template <typename T, typename LookupKey,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  T construct_collection(LookupKey key) {
    if constexpr (Derived::has_static_v &&
                  detail::is_static_lookup_key_v<LookupKey>) {
      if (self().template select_static_collection_construct<T, LookupKey>()) {
        return execute_transaction(
            self().runtime(), [&](runtime_context_type &context) -> T {
              return self()
                  .statics()
                  .template construct_collection<T, LookupKey>(ephemeral_scope,
                                                               self(), context);
            });
      }
    }

    return execute_transaction(
        self().runtime(), [&](runtime_context_type &context) -> T {
          return construct_collection_scoped<T>(
              ephemeral_scope, context, detail::binding_collection_append{},
              std::move(key));
        });
  }

  template <typename T, typename Fn, typename LookupKey,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  T construct_collection(Fn &&fn, LookupKey key) {
    if constexpr (Derived::has_static_v &&
                  detail::is_static_lookup_key_v<LookupKey>) {
      if (self().template select_static_collection_construct<T, LookupKey>()) {
        return execute_transaction(
            self().runtime(), [&](runtime_context_type &context) -> T {
              return self()
                  .statics()
                  .template construct_collection<T, LookupKey>(
                      ephemeral_scope, self(), context, std::forward<Fn>(fn));
            });
      }
    }

    return execute_transaction(
        self().runtime(), [&](runtime_context_type &context) -> T {
          return construct_collection_scoped<T>(
              ephemeral_scope, context, std::forward<Fn>(fn), std::move(key));
        });
  }

  template <typename T, typename Key> T construct_collection(key_type<Key>) {
    return construct_collection<T>(detail::make_lookup_key(key_type<Key>{}));
  }

  template <typename T, typename Fn, typename Key>
  T construct_collection(Fn &&fn, key_type<Key>) {
    return construct_collection<T>(std::forward<Fn>(fn),
                                   detail::make_lookup_key(key_type<Key>{}));
  }

  template <typename T, typename Fn> T construct_collection(Fn &&fn, none_t) {
    return construct_collection<T>(std::forward<Fn>(fn),
                                   detail::no_lookup_key());
  }

  template <typename T, typename Fn>
  std::size_t append_collection(construction_scope scope, T &results,
                                runtime_context_type &context, Fn &&fn) {
    return append_collection<T>(scope, results, context, std::forward<Fn>(fn),
                                detail::no_lookup_key());
  }

  template <typename T, typename Key, typename Fn>
  std::size_t append_collection(construction_scope scope, T &results,
                                runtime_context_type &context, Fn &&fn) {
    return append_collection<T>(scope, results, context, std::forward<Fn>(fn),
                                detail::make_lookup_key(type_selector<Key>{}));
  }

  template <typename T, typename Fn, typename LookupKey,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  std::size_t append_collection(construction_scope scope, T &results,
                                runtime_context_type &context, Fn &&fn,
                                LookupKey key) {
    return append_collection_impl<T>(scope, results, context, std::move(key),
                                     std::forward<Fn>(fn));
  }

  template <typename T, typename LookupKey, typename Fn,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  std::size_t append_collection_impl(construction_scope scope, T &results,
                                     runtime_context_type &context,
                                     LookupKey key, Fn &&fn) {
    std::size_t count = 0;
    if constexpr (Derived::runtime_configuration::merge_parent_collections &&
                  Derived::has_parent_v) {
      if (self().parent_) {
        count += self().parent_->template append_collection<T>(
            scope, results, context, fn, key);
      }
    }
    if constexpr (Derived::has_static_v) {
      count += detail::append_binding_collection(
          results,
          [&](auto &collection, auto &&append) {
            return self().template runtime_append_collection<T>(
                scope, collection, context,
                std::forward<decltype(append)>(append), key);
          },
          [&](auto &collection, auto &&append) {
            return self().statics().template append_collection<T, LookupKey>(
                scope, collection, self(), context,
                std::forward<decltype(append)>(append));
          },
          std::forward<Fn>(fn));
    } else {
      count += self().template runtime_append_collection<T>(
          scope, results, context, std::forward<Fn>(fn), key);
    }
    return count;
  }

  template <typename T> std::size_t count_collection() {
    return count_collection<T>(detail::no_lookup_key());
  }

  template <typename T, typename Key> std::size_t count_collection() {
    return count_collection<T>(detail::make_lookup_key(type_selector<Key>{}));
  }

  template <typename T, typename LookupKey,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  std::size_t count_collection(LookupKey key) {
    std::size_t count = self().template runtime_count_collection<T>(key);
    if constexpr (Derived::has_static_v) {
      count +=
          Derived::static_registry_type::template count_collection<T,
                                                                   LookupKey>();
    }
    if constexpr (Derived::runtime_configuration::merge_parent_collections &&
                  Derived::has_parent_v) {
      if (self().parent_) {
        count += self().parent_->template count_collection<T>(key);
      }
    }
    return count;
  }

private:
  template <typename T, typename LookupKey, typename Fn,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  T construct_collection_impl(construction_scope scope,
                              runtime_context_type &context, LookupKey key,
                              Fn &&fn) {
    using collection_type = collection_traits<T>;
    using resolve_type = typename collection_type::resolve_type;

    static_assert(collection_type::is_collection,
                  "missing collection_traits specialization for type T");

    const std::size_t total = count_collection<T>(key);
    // A declared collection lookup with no rows is a valid empty collection.
    if (total == 0 &&
        !self().template runtime_has_explicit_collection_lookup<T>(key)) {
      throw detail::make_collection_type_not_found_exception<T, resolve_type>();
    }

    T results;
    collection_type::reserve(results, total);

    auto &&append = fn;
    append_collection_impl<T>(scope, results, context, key, append);
    return results;
  }
};

} // namespace detail
} // namespace dingo
