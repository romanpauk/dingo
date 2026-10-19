//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/binding/selection.h>
#include <dingo/key/key.h>
#include <dingo/lookup/operations.h>
#include <dingo/runtime/lookup/route.h>

#include <cstddef>
#include <type_traits>
#include <utility>

namespace dingo::detail {

template <typename Value>
using lookup_selection_t =
    runtime_binding_selection<typename Value::binding_interface_type>;

template <typename Value>
lookup_selection_t<Value> make_lookup_selection(Value *selected,
                                                bool ambiguous) {
  if (ambiguous) {
    return lookup_selection_t<Value>::ambiguity();
  }
  return detail::make_runtime_selection<typename Value::binding_interface_type>(
      selected ? &selected->binding() : nullptr);
}

template <typename LookupEntry, typename State, typename Key>
lookup_selection_t<typename State::value_type> select_lookup(State &state,
                                                             const Key &key) {
  using value_type = typename State::value_type;
  auto &index = state.lookup_indexes.template get<LookupEntry>();
  auto match =
      detail::lookup_find_singular<LookupEntry, value_type>(index, key);
  return make_lookup_selection(match.value, match.ambiguous);
}

template <typename LookupEntry, typename State, typename Key, typename Fn>
std::size_t for_each_lookup(State &state, const Key &key, Fn &&fn) {
  auto &index = state.lookup_indexes.template get<LookupEntry>();
  return detail::lookup_for_each<LookupEntry>(index, key, std::forward<Fn>(fn));
}

template <bool AllowMiss, typename Routing, typename LookupKey,
          typename Interface, typename State>
// NOLINTNEXTLINE(readability-function-cognitive-complexity,readability-function-size)
lookup_selection_t<typename State::value_type>
select_binding_at_interface(State &state, LookupKey &key) {
  using selection = lookup_selection_t<typename State::value_type>;
  using route = lookup_index_route<Interface, std::decay_t<LookupKey>, Routing>;
  using lookup_entry = typename route::entry;
  if constexpr (std::is_void_v<lookup_entry>) {
    if constexpr (AllowMiss) {
      return selection::miss();
    } else {
      static_assert(!std::is_void_v<lookup_entry>,
                    "keyed registration or request has no matching "
                    "dingo lookup definition for interface/key");
    }
  } else {
    return select_lookup<lookup_entry>(state, route::key(key));
  }
}

// Selection depends on the request only through its normalized value, its
// exact type and the lookup key, so request spellings sharing them share one
// chain instantiation.
template <typename Value, typename Exact, bool AllowMiss, typename Routing,
          typename State, typename LookupKey>
lookup_selection_t<typename State::value_type> select_binding(State &state,
                                                              LookupKey &key) {
  static_assert(is_lookup_key_v<LookupKey>);
  if constexpr (!is_static_lookup_key_definition_v<LookupKey>) {
    using lookup_route = lookup_index_route<Value, LookupKey, Routing>;
    if constexpr (!lookup_route::has_explicit_lookup &&
                  !std::is_same_v<Value, Exact>) {
      using exact_route = lookup_index_route<Exact, LookupKey, Routing>;
      if constexpr (exact_route::has_explicit_lookup) {
        return select_binding_at_interface<AllowMiss, Routing, LookupKey,
                                           Exact>(state, key);
      }
    }
    return select_binding_at_interface<AllowMiss, Routing, LookupKey, Value>(
        state, key);
  } else {
    auto selection =
        select_binding_at_interface<AllowMiss, Routing, LookupKey, Exact>(state,
                                                                          key);
    if constexpr (!std::is_same_v<Value, Exact>) {
      if (!selection.found()) {
        selection =
            select_binding_at_interface<AllowMiss, Routing, LookupKey, Value>(
                state, key);
      }
    }
    return selection;
  }
}

} // namespace dingo::detail
