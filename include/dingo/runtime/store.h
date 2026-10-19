//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/base/construction_scope.h>
#include <dingo/base/exceptions.h>
#include <dingo/binding/interface.h>
#include <dingo/binding/request.h>
#include <dingo/binding/selection.h>
#include <dingo/context/cache.h>
#include <dingo/dependency/collection.h>
#include <dingo/dependency/normalized_type.h>
#include <dingo/key/key.h>
#include <dingo/lookup/definition.h>
#include <dingo/registration/requirements.h>
#include <dingo/runtime/binding_value.h>
#include <dingo/runtime/context.h>
#include <dingo/runtime/data.h>
#include <dingo/runtime/lookup/commit.h>
#include <dingo/runtime/lookup/keys.h>
#include <dingo/runtime/lookup/query.h>
#include <dingo/runtime/lookup/route.h>
#include <dingo/runtime/transaction.h>

#include <cassert>
#include <cstddef>
#include <type_traits>
#include <utility>

namespace dingo::detail {

// The part of a runtime registry that depends only on the container traits and
// the allocator: lookup index types, lookup selection, registration commit and
// the rollback of both. runtime_registry derives from it, so the machinery is
// instantiated once per traits/allocator pair instead of once per registry
// specialization. Data ownership stays in runtime_registry, which hands the
// active lookup state to the store.
template <typename ContainerTraits, typename Allocator> class registry_store {
protected:
  using store_type = registry_store;
  using allocator_type = Allocator;
  using rtti_type = typename ContainerTraits::rtti_type;
  using lookup_definition_type =
      detail::container_lookup_definition_type_t<ContainerTraits>;
  using runtime_context_type = runtime_context<Allocator>;
  using runtime_transaction_type = runtime_transaction<Allocator>;
  using runtime_binding_interface_type =
      runtime_binding_interface<typename ContainerTraits::rtti_type,
                                runtime_context_type>;
  using runtime_selection =
      detail::runtime_binding_selection<runtime_binding_interface_type>;

  using lookup_routing_type =
      detail::lookup_routing<rtti_type, lookup_definition_type>;
  using normalized_lookup_entries =
      typename lookup_routing_type::normalized_lookup_entries;
  using base_lookup_entry = typename lookup_routing_type::base_lookup_entry;
  using lookup_index_entries =
      typename lookup_routing_type::lookup_index_entries;
  template <typename LookupEntry>
  using lookup_entry_cardinality =
      detail::lookup_entry_cardinality<LookupEntry>;

  using runtime_lookup_binding_view =
      detail::runtime_lookup_binding_view<runtime_binding_interface_type>;
  using runtime_lookup_value =
      detail::runtime_lookup_value<runtime_binding_interface_type>;
  using runtime_bindings_state = detail::runtime_bindings_state<
      lookup_index_entries, runtime_binding_interface_type, allocator_type>;

public:
  using runtime_data_type =
      detail::runtime_registry_data<allocator_type, runtime_bindings_state>;

protected:
  using runtime_scope_state = typename runtime_data_type::scope_state_type;

  registry_store() { validate_lookup_definitions(); }

  registry_store(const registry_store &) = delete;
  registry_store &operator=(const registry_store &) = delete;

  ~registry_store() = default;

  static constexpr void validate_lookup_definitions() {
    using entries_type = normalized_lookup_entries;
    static_assert(
        !detail::has_duplicate_base_lookup_definition_v<lookup_definition_type>,
        "duplicate dingo base lookup definition");
    static_assert(!detail::has_duplicate_lookup_definition_v<entries_type>,
                  "duplicate dingo lookup definition for interface/key "
                  "domain/cardinality");
    static_assert(!detail::has_duplicate_lookup_key_type_v<entries_type>,
                  "conflicting dingo lookup definitions for interface/key "
                  "domain");
  }

  template <typename T, typename Fn, typename LookupKey,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  T construct_collection(runtime_bindings_state *state,
                         construction_scope scope,
                         runtime_context_type &context, Fn &&fn,
                         LookupKey key) {
    using collection_type = collection_traits<T>;
    using resolve_type = typename collection_type::resolve_type;

    static_assert(collection_type::is_collection,
                  "missing collection_traits specialization for type T");

    T results;
    const std::size_t count = count_collection<T>(state, key);
    if (count == 0 && !has_explicit_collection_lookup<T>(key)) {
      throw detail::make_collection_type_not_found_exception<T, resolve_type>();
    }

    collection_type::reserve(results, count);
    append_runtime_collection(state, results, scope, context,
                              std::forward<Fn>(fn), std::move(key));
    return results;
  }

  template <typename Value, typename Exact, bool AllowMiss, typename LookupKey,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  detail::binding_status binding_status(runtime_bindings_state *state,
                                        LookupKey key) {
    if (!state) {
      return detail::binding_status::not_found;
    }
    auto selection =
        detail::select_binding<Value, Exact, AllowMiss, lookup_routing_type>(
            *state, key);
    return selection.status;
  }

  template <typename Request, typename Result = Request>
  Result resolve_binding(runtime_selection selection, construction_scope scope,
                         runtime_context_type &context) {
    auto &binding = *selection.binding;
    if constexpr (detail::cache::supports_v<Request>) {
      return resolve<Request, Result>(scope, binding, context,
                                      detail::cache::key<Request>());
    } else {
      return resolve<Request, Result>(scope, binding, context);
    }
  }

  struct cache_result {
    bool hit = false;
    void *address = nullptr;
  };

  template <typename Request>
  cache_result lookup_cache(runtime_selection selection) const {
    if constexpr (detail::cache::supports_v<Request>) {
      if (selection.status == detail::binding_status::found) {
        auto *entry = selection.binding->cache_slot();
        if (entry != nullptr && entry->key == detail::cache::key<Request>()) {
          return {true, entry->address};
        }
      }
    }
    return {};
  }

  template <typename Request, typename Result>
  static Result resolve_cached(cache_result result) {
    assert(result.hit);
    return detail::convert_resolved_binding<Request>(result.address);
  }

  template <typename T> static T invalid_registration_return();

  using runtime_binding_value = detail::runtime_binding_value;

  template <typename... Bindings>
  using runtime_binding_value_impl =
      detail::runtime_binding_value_impl<runtime_binding_interface_type,
                                         Bindings...>;

  template <typename Interface, typename Key>
  using lookup_index_route =
      detail::lookup_index_route<Interface, Key, lookup_routing_type>;

  template <typename TypeInterface>
  using runtime_lookup_entries_for_interface_t =
      detail::runtime_lookup_entries_for_interface_t<TypeInterface,
                                                     normalized_lookup_entries>;

  template <typename InterfaceList, typename... KeyValueArgs>
  static constexpr void validate_supplied_runtime_registration_keys() {
    detail::validate_supplied_runtime_registration_keys<
        InterfaceList, normalized_lookup_entries, KeyValueArgs...>();
  }

  template <typename Owner>
  using shared_lookup_value_factory =
      detail::shared_lookup_value_factory<runtime_lookup_value, Owner>;

  template <bool SharedOwner, typename Binding>
  using direct_lookup_value_factory =
      detail::direct_lookup_value_factory<runtime_lookup_value, SharedOwner,
                                          Binding>;

  template <typename T, typename LookupKey>
  static constexpr bool has_explicit_collection_lookup(const LookupKey &) {
    static_assert(detail::is_lookup_key_v<std::decay_t<LookupKey>>);
    using collection_type = collection_traits<T>;
    using resolve_type = typename collection_type::resolve_type;
    using lookup_key_type = std::decay_t<LookupKey>;
    using route = lookup_index_route<resolve_type, lookup_key_type>;
    return route::has_explicit_collection_lookup;
  }

  template <typename Value, typename Exact, bool AllowMiss, typename LookupKey>
  runtime_selection source_select(runtime_bindings_state *state,
                                  LookupKey key) {
    static_assert(detail::is_lookup_key_v<LookupKey>);
    return state ? detail::select_binding<Value, Exact, AllowMiss,
                                          lookup_routing_type>(*state, key)
                 : runtime_selection::miss();
  }

  template <typename Request, typename LookupKey>
  auto source_resolve(construction_scope scope, runtime_selection selection,
                      runtime_context_type &context, const LookupKey &) ->
      typename Request::interface_type {
    return resolve_binding<typename Request::lookup_type,
                           typename Request::interface_type>(selection, scope,
                                                             context);
  }

  template <typename T, typename LookupKey, typename Fn,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  std::size_t for_each_collection_entry(runtime_bindings_state &state,
                                        LookupKey key, Fn &&fn) {
    using collection_type = collection_traits<T>;
    using resolve_type = typename collection_type::resolve_type;
    using lookup_type = normalized_type_t<resolve_type>;
    using lookup_key_type = std::decay_t<LookupKey>;
    using route = lookup_index_route<lookup_type, lookup_key_type>;

    if constexpr (route::has_explicit_collection_lookup ||
                  std::is_same_v<typename route::entry, base_lookup_entry>) {
      return detail::for_each_lookup<typename route::entry>(
          state, route::key(key), std::forward<Fn>(fn));
    } else {
      (void)fn;
      return 0;
    }
  }

  template <typename T, typename Fn, typename LookupKey,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  std::size_t append_runtime_collection(runtime_bindings_state *state,
                                        T &results, construction_scope scope,
                                        runtime_context_type &context, Fn &&fn,
                                        LookupKey key) {
    using collection_type = collection_traits<T>;
    using resolve_type = typename collection_type::resolve_type;
    if (!state) {
      return 0;
    }
    return for_each_collection_entry<T>(*state, key, [&](auto &entry) {
      fn(results, resolve_collection_type<resolve_type>(scope, entry.binding(),
                                                        context));
    });
  }

  template <typename T, typename LookupKey,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  std::size_t count_collection(runtime_bindings_state *state, LookupKey key) {
    std::size_t result = 0;
    if (state) {
      for_each_collection_entry<T>(*state, key, [&](auto &) { ++result; });
    }
    return result;
  }

  bool commit_singular_base_lookup(runtime_bindings_state &state,
                                   runtime_transaction_type &transaction,
                                   detail::base_lookup_key<rtti_type> key,
                                   runtime_lookup_value value) {
    return detail::commit_singular_base_lookup<base_lookup_entry>(
        state, transaction, key, std::move(value));
  }

  template <typename TypeInterface, typename TypeStorage, typename LookupKey,
            typename ValueFactory, typename... KeyValueArgs>
  void commit_registration(runtime_bindings_state &state,
                           ValueFactory &value_factory,
                           runtime_transaction_type &transaction,
                           LookupKey lookup_key, KeyValueArgs &&...key_values) {
    detail::commit_registration<TypeInterface, TypeStorage,
                                lookup_routing_type>(
        state, value_factory, transaction, lookup_key,
        std::forward<KeyValueArgs>(key_values)...);
  }

  template <typename Request, typename Result, typename Binding,
            typename Context>
  Result resolve(construction_scope scope, Binding &binding, Context &context,
                 const void *cache_key = nullptr) {
    return ::dingo::resolve_binding_request<Request, rtti_type>(
        scope, binding, context, cache_key);
  }

  template <typename T, typename Binding, typename Context>
  T resolve_collection_type(construction_scope scope, Binding &binding,
                            Context &context) {
    return ::dingo::resolve_binding_request<T, rtti_type>(scope, binding,
                                                          context);
  }

  template <class Storage, class TypeInterface, class Type>
  void check_interface_requirements() {
    detail::interface_registration_requirements<Storage, TypeInterface,
                                                Type>::assert_valid();
  }
};

} // namespace dingo::detail
