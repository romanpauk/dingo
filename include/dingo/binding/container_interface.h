//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/base/none.h>
#include <dingo/binding/binding_collection.h>
#include <dingo/factory/callable.h>
#include <dingo/type/collection_traits.h>
#include <dingo/type/key.h>

#include <cstddef>
#include <type_traits>
#include <utility>

namespace dingo::detail {

// The request entry points both containers spell the same way. A container
// provides construct_collection<T>(fn, lookup_key), count_collection<T>(lookup_
// key) and with_context(fn), which runs fn with a fresh resolution context.
template <typename Container> class container_interface {
  template <typename Collection, typename Fn>
  using collection_insert_invocable =
      std::is_invocable<Fn &, Collection &,
                        typename collection_traits<Collection>::resolve_type>;

  Container &self() { return static_cast<Container &>(*this); }

public:
  template <typename T> T construct_collection() {
    return self().template construct_collection<T>(binding_collection_append{},
                                                   no_lookup_key());
  }

  template <typename T, typename Arg,
            std::enable_if_t<!is_lookup_key_v<std::decay_t<Arg>>, int> = 0>
  T construct_collection(Arg &&arg) {
    if constexpr (collection_insert_invocable<T, std::decay_t<Arg>>::value) {
      return self().template construct_collection<T>(std::forward<Arg>(arg),
                                                     no_lookup_key());
    } else {
      return self().template construct_collection<T>(
          binding_collection_append{}, make_lookup_key(std::forward<Arg>(arg)));
    }
  }

  template <typename T, typename Fn, typename IdType,
            std::enable_if_t<!is_lookup_key_v<std::decay_t<IdType>>, int> = 0>
  T construct_collection(Fn &&fn, IdType &&id) {
    return self().template construct_collection<T>(
        std::forward<Fn>(fn), make_lookup_key(std::forward<IdType>(id)));
  }

  template <typename T> std::size_t count_collection() {
    return self().template count_collection<T>(no_lookup_key());
  }

  template <typename T, typename Key> std::size_t count_collection() {
    return self().template count_collection<T>(
        make_lookup_key(type_selector<Key>{}));
  }

  template <typename Signature = void, typename Callable>
  auto invoke(Callable &&callable) {
    using callable_type = std::remove_cv_t<std::remove_reference_t<Callable>>;
    using dispatch_signature =
        callable_dispatch_signature_t<Signature, callable_type>;
    return self().with_context([&](auto &context) {
      auto type_guard = context.template track_type<callable_type>();
      return callable_invoke<dispatch_signature>::construct(
          std::forward<Callable>(callable), ephemeral_scope, context, self());
    });
  }
};

} // namespace dingo::detail
