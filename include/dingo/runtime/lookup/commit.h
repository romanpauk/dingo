//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/base/exceptions.h>
#include <dingo/base/type_list.h>
#include <dingo/key/annotated.h>
#include <dingo/lookup/lookup.h>
#include <dingo/lookup/operations.h>
#include <dingo/lookup/tags.h>
#include <dingo/registration/requirements.h>
#include <dingo/runtime/lookup/keys.h>
#include <dingo/runtime/lookup/route.h>

#include <cassert>
#include <memory>
#include <tuple>
#include <type_traits>
#include <utility>

namespace dingo::detail {

template <typename LookupEntry, typename State, typename Key, typename Binding>
void erase_lookup_binding(State &state, const Key &key,
                          Binding *binding) noexcept {
  auto &index = state.lookup_indexes.template get<LookupEntry>();
  if constexpr (std::is_same_v<
                    typename lookup_entry_cardinality<LookupEntry>::type,
                    ::dingo::one>) {
    auto it = index.find(key);
    if (it != index.end()) {
      auto &value = detail::lookup_iterator_value(it);
      if (std::addressof(value.binding()) == binding) {
        index.erase(it);
        return;
      }
    }
  } else {
    auto [first, last] = index.equal_range(key);
    for (auto it = first; it != last; ++it) {
      auto &value = detail::lookup_iterator_value(it);
      if (std::addressof(value.binding()) == binding) {
        index.erase(it);
        return;
      }
    }
  }
  assert(false && "runtime lookup binding was not found");
}

template <typename LookupEntry, typename Transaction, typename State,
          typename Handle, typename Key>
void record_lookup_rollback(Transaction &transaction, State &state,
                            Handle handle, const Key &key) {
  auto &value = detail::lookup_iterator_value(handle);
  auto *binding = std::addressof(value.binding());
  try {
    transaction.on_rollback(
        [&state, key = std::decay_t<Key>(key), binding]() noexcept {
          erase_lookup_binding<LookupEntry>(state, key, binding);
        });
  } catch (...) {
    erase_lookup_binding<LookupEntry>(state, key, binding);
    throw;
  }
}

template <typename TypeInterface, typename TypeStorage, typename LookupKey,
          typename LookupEntry, typename State, typename ValueFactory,
          typename KeyArg>
auto insert_lookup(State &state, ValueFactory &value_factory,
                   const LookupKey &lookup_key, const KeyArg &key_arg) {
  auto &index = state.lookup_indexes.template get<LookupEntry>();

  if constexpr (std::is_same_v<
                    typename lookup_entry_cardinality<LookupEntry>::type,
                    ::dingo::one>) {
    auto [handle, inserted] = index.try_emplace(
        key_arg, value_factory.template make<TypeInterface>());
    if (!inserted) {
      throw detail::make_lookup_already_registered_exception<
          TypeInterface, typename TypeStorage::type>(lookup_key, key_arg);
    }
    return handle;
  } else {
    return index.emplace(key_arg, value_factory.template make<TypeInterface>());
  }
}

template <typename TypeInterface, typename TypeStorage, typename LookupKey,
          typename State, typename ValueFactory, typename Transaction,
          typename KeyResolver>
void commit_lookup(State &, ValueFactory &, Transaction &, KeyResolver &&,
                   const LookupKey &, type_list<>) {}

template <typename TypeInterface, typename TypeStorage, typename LookupKey,
          typename Head, typename... Tail, typename State,
          typename ValueFactory, typename Transaction, typename KeyResolver>
void commit_lookup(State &state, ValueFactory &value_factory,
                   Transaction &transaction, KeyResolver &&key_resolver,
                   const LookupKey &lookup_key, type_list<Head, Tail...>) {
  static_assert(ValueFactory::shared_owner || sizeof...(Tail) == 0,
                "unique runtime binding owner can only be inserted into one "
                "lookup");
  decltype(auto) key_arg = key_resolver(type_list_iterator<Head>{});
  auto handle = insert_lookup<TypeInterface, TypeStorage, LookupKey, Head>(
      state, value_factory, lookup_key, key_arg);
  record_lookup_rollback<Head>(transaction, state, handle, key_arg);
  commit_lookup<TypeInterface, TypeStorage, LookupKey>(
      state, value_factory, transaction,
      std::forward<KeyResolver>(key_resolver), lookup_key,
      type_list<Tail...>{});
}

template <typename BaseLookupEntry, typename State, typename Transaction,
          typename Key>
bool commit_singular_base_lookup(State &state, Transaction &transaction,
                                 Key key, typename State::value_type value) {
  auto &index = state.lookup_indexes.template get<BaseLookupEntry>();
  auto [handle, inserted] = index.try_emplace(key, std::move(value));
  if (inserted) {
    record_lookup_rollback<BaseLookupEntry>(transaction, state, handle, key);
  }
  return inserted;
}

template <typename Value, typename Owner> struct shared_lookup_value_factory {
  static constexpr bool shared_owner = true;
  Owner *owner;

  template <typename TypeInterface> Value make() {
    return Value(owner->template view<TypeInterface>());
  }
};

template <typename Value, bool SharedOwner, typename Binding>
struct direct_lookup_value_factory {
  static constexpr bool shared_owner = SharedOwner;
  Binding *binding;

  template <typename TypeInterface> Value make() {
    static_assert(
        std::is_same_v<typename Binding::interface_type,
                       typename annotated_traits<TypeInterface>::type>);
    return Value(typename Value::binding_view_type{binding});
  }
};

template <typename TypeInterface, typename TypeStorage, typename LookupKey,
          typename Cardinality, typename Routing, typename State,
          typename ValueFactory, typename Transaction>
void commit_static_lookup(State &state, ValueFactory &value_factory,
                          Transaction &transaction, LookupKey lookup_key) {
  using key_domain = typename LookupKey::definition_type;
  using explicit_lookup_entry =
      static_key_lookup_entry_t<TypeInterface, key_domain, Cardinality,
                                Routing>;
  using route = lookup_index_route<TypeInterface, LookupKey, Routing>;
  using routed_lookup_entry = typename route::entry;
  using routed_cardinality =
      typename lookup_entry_cardinality<routed_lookup_entry>::type;
  if constexpr (!std::is_void_v<explicit_lookup_entry> ||
                std::is_same_v<Cardinality, ::dingo::one> ||
                std::is_same_v<routed_cardinality, Cardinality>) {
    static_assert(!std::is_void_v<routed_lookup_entry>);
    auto key_resolver = [&](auto) { return route::key(lookup_key); };
    commit_lookup<TypeInterface, TypeStorage, LookupKey>(
        state, value_factory, transaction, key_resolver, lookup_key,
        type_list<routed_lookup_entry>{});
  } else {
    static_assert(std::is_same_v<Cardinality, ::dingo::one>,
                  "static-key many lookup registrations require an explicit "
                  "dingo lookup definition");
  }
}

template <typename TypeInterface, typename TypeStorage, typename Routing,
          typename State, typename ValueFactory, typename Transaction,
          typename LookupKey, typename... KeyValueArgs>
// NOLINTNEXTLINE(readability-function-cognitive-complexity)
void commit_registration(State &state, ValueFactory &value_factory,
                         Transaction &transaction, LookupKey lookup_key,
                         KeyValueArgs &&...key_values) {
  interface_registration_requirements<
      TypeStorage, typename annotated_traits<TypeInterface>::type,
      typename TypeStorage::type>::assert_valid();
  ((void)key_values, ...);
  using key_domain = typename LookupKey::definition_type;
  using lookup_cardinality_type =
      static_key_lookup_cardinality_t<TypeInterface, key_domain, Routing>;

  if constexpr (sizeof...(KeyValueArgs) > 0) {
    using lookup_entries = runtime_lookup_entries_for_interface_t<
        TypeInterface, typename Routing::normalized_lookup_entries>;
    if constexpr (type_list_size_v<lookup_entries> > 0) {
      validate_required_runtime_registration_keys<lookup_entries,
                                                  KeyValueArgs...>();
      auto key_value_tuple = std::forward_as_tuple(key_values...);
      auto key_resolver = [&](auto element) -> decltype(auto) {
        using entry_type = typename decltype(element)::type;
        return qualified_registration_key_value<entry_type>(key_value_tuple);
      };
      commit_lookup<TypeInterface, TypeStorage, LookupKey>(
          state, value_factory, transaction, key_resolver, lookup_key,
          lookup_entries{});
    } else {
      commit_static_lookup<TypeInterface, TypeStorage, LookupKey,
                           lookup_cardinality_type, Routing>(
          state, value_factory, transaction, lookup_key);
    }
  } else {
    commit_static_lookup<TypeInterface, TypeStorage, LookupKey,
                         lookup_cardinality_type, Routing>(
        state, value_factory, transaction, lookup_key);
  }
}

} // namespace dingo::detail
