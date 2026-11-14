//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/base/exceptions.h>
#include <dingo/base/meta.h>
#include <dingo/base/none.h>
#include <dingo/binding/resolution.h>
#include <dingo/binding/selection.h>
#include <dingo/factory/auto_constructible.h>
#include <dingo/factory/callable.h>
#include <dingo/factory/constructor.h>
#include <dingo/lookup/container_lookup_definition.h>
#include <dingo/lookup/lookup.h>
#include <dingo/lookup/lookup_key.h>
#include <dingo/memory/allocator.h>
#include <dingo/registration/requirements.h>
#include <dingo/registration/type_registration.h>
#include <dingo/request/collection.h>
#include <dingo/request/decomposition.h>
#include <dingo/resolution/entry_points.h>
#include <dingo/rtti/typeid_provider.h>
#include <dingo/runtime/binding.h>
#include <dingo/runtime/container_traits.h>
#include <dingo/runtime/context.h>
#include <dingo/runtime/registry_store.h>
#include <dingo/storage/interface_storage_traits.h>
#include <dingo/type/annotated.h>
#include <dingo/type/collection_traits.h>
#include <dingo/type/shape.h>

#include <cassert>
#include <cstddef>
#include <memory>
#include <tuple>
#include <type_traits>
#include <utility>

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4702)
#endif

namespace dingo {

// The resolve front-end of a runtime container. It is the root runtime
// registry and adds what a root answers beyond its bindings: the parent
// fallback, auto-construction, and the transaction and cache of a request.
template <typename ContainerTraits, typename Allocator,
          typename ParentContainer>
class DINGO_EMPTY_BASES detail::runtime_container_impl
    : public allocator_base<Allocator>,
      public detail::container_requests<detail::runtime_container_impl<
          ContainerTraits, Allocator, ParentContainer>> {
  template <typename> friend class ::dingo::resolution_context;

  using self_type = detail::runtime_container_impl<ContainerTraits, Allocator,
                                                   ParentContainer>;

public:
  using container_type = self_type;

private:
public:
  using container_traits_type = ContainerTraits;
  using allocator_type = Allocator;
  using rtti_type = typename ContainerTraits::rtti_type;
  using lookup_definition_type =
      detail::container_lookup_definition_type_t<ContainerTraits>;

protected:
  using runtime_context_type = runtime_context<Allocator>;
  using runtime_transaction_type = runtime_transaction<Allocator>;
  using runtime_binding_interface_type =
      runtime_binding_interface<typename ContainerTraits::rtti_type,
                                runtime_context_type>;
  using runtime_selection =
      detail::runtime_binding_selection<runtime_binding_interface_type>;
  // The selection of a request in the container and in the levels it falls
  // back to before any parent, and the last level it was selected in.
  using level_selection =
      detail::runtime_level_selection<runtime_binding_interface_type,
                                      self_type>;

  using normalized_lookup_entries =
      detail::normalize_lookup_definitions_t<lookup_definition_type>;
  using base_lookup_definition =
      detail::base_lookup_definition_t<lookup_definition_type>;
  using base_lookup_entry =
      detail::base_lookup_entry<rtti_type,
                                typename base_lookup_definition::cardinality,
                                typename base_lookup_definition::backend_type>;
  using lookup_index_entries = type_list_unique_t<
      type_list_cat_t<normalized_lookup_entries, type_list<base_lookup_entry>>>;
  template <typename LookupEntry>
  using lookup_entry_cardinality =
      detail::lookup_entry_cardinality<LookupEntry>;

  using runtime_lookup_value =
      detail::runtime_lookup_value<runtime_binding_interface_type>;
  using runtime_bindings_state = detail::runtime_bindings_state<
      lookup_index_entries, runtime_binding_interface_type, allocator_type>;

  static constexpr bool replaces_until_resolved = std::is_same_v<
      detail::container_duplicate_registration_policy_t<ContainerTraits>,
      replace_until_resolved>;

  enum class lookup_commit { inserted, replaced, rejected, sealed };

public:
  using runtime_data_type =
      detail::runtime_registry_data<allocator_type, runtime_bindings_state>;

protected:
  using runtime_scope_state = typename runtime_data_type::scope_state_type;

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

  struct cache_result {
    bool hit = false;
    void *address = nullptr;
  };

  // The address a selected binding caches for the request of a cache key.
  static cache_result lookup_cache(runtime_selection selection,
                                   const void *key) {
    if (selection.status == detail::binding_status::found) {
      auto *entry = selection.binding->cache_slot();
      if (entry != nullptr && entry->key == key) {
        return {true, entry->address};
      }
    }
    return {};
  }

  template <typename T> static T unreachable();

  template <typename LookupKeyDefinition> struct base_lookup_key_type;

  template <typename T>
  struct base_lookup_key_type<detail::lookup_key<key_type<T>>> {
    using type = T;
  };

  template <typename Interface, typename Key>
  static detail::base_lookup_key<rtti_type> typed_key() {
    using key_definition = typename std::decay_t<Key>::definition_type;
    using key_type = typename base_lookup_key_type<key_definition>::type;
    return {rtti_type::template get_type_index<Interface>(),
            rtti_type::template get_type_index<key_type>()};
  }

  template <typename LookupEntry> struct runtime_lookup_entry_traits {
    static constexpr bool is_key_value = false;
    using interface_type = void;
    using key_type = ::dingo::key_type<::dingo::none_t>;
  };

  template <typename Interface, typename Key, typename Cardinality,
            typename Backend>
  struct runtime_lookup_entry_traits<detail::lookup_entry<
      Interface, detail::lookup_key<key_value<Key>>, Cardinality, Backend>> {
    static constexpr bool is_key_value = true;
    using interface_type = Interface;
    using key_type = Key;
  };

  // The key-value lookup entries of an interface of a registration: those
  // defined for the interface itself, or else those of its normalized
  // interface, unless the registration binds that one too. An array and its
  // element are two interfaces, each with its own lookup.
  template <typename TypeInterface, typename Interfaces, typename Entries>
  struct interface_lookup_entries;

  template <typename TypeInterface, typename... Interfaces, typename... Entries>
  struct interface_lookup_entries<TypeInterface, type_list<Interfaces...>,
                                  type_list<Entries...>> {
  private:
    template <typename Interface>
    using lookup_entries = type_list_cat_t<std::conditional_t<
        runtime_lookup_entry_traits<Entries>::is_key_value &&
            std::is_same_v<Interface, typename runtime_lookup_entry_traits<
                                          Entries>::interface_type>,
        type_list<Entries>, type_list<>>...>;
    using exact_interface = std::remove_cv_t<TypeInterface>;
    using normalized_interface = normalized_type_t<TypeInterface>;
    using exact = lookup_entries<exact_interface>;
    static constexpr bool binds_normalized =
        !std::is_same_v<exact_interface, normalized_interface> &&
        (std::is_same_v<std::remove_cv_t<Interfaces>, normalized_interface> ||
         ...);

  public:
    using type =
        std::conditional_t<type_list_size_v<exact> != 0 || binds_normalized,
                           exact, lookup_entries<normalized_interface>>;
    // Whether the lookups of the interface are those of its normalized
    // interface, which the registration commits under that one.
    static constexpr bool defers_normalized =
        type_list_size_v<exact> == 0 && binds_normalized &&
        type_list_size_v<lookup_entries<normalized_interface>> != 0;
  };

  template <typename TypeInterface, typename Interfaces>
  using interface_lookup_entries_t =
      typename interface_lookup_entries<TypeInterface, Interfaces,
                                        normalized_lookup_entries>::type;

  template <typename Arg>
  using runtime_registration_arg_traits = std::decay_t<Arg>;

  template <typename Arg>
  using runtime_registration_arg_interface_t = detail::key_value_interface_t<
      typename runtime_registration_arg_traits<Arg>::interface_type>;

  // A key value supplies the key of a lookup entry when their keys are the
  // same: qualified, when it names the interface of the entry, or unqualified.
  template <typename Entry, typename Arg, bool Qualified>
  static constexpr bool runtime_registration_arg_keys_entry_v =
      std::is_same_v<typename runtime_lookup_entry_traits<Entry>::key_type,
                     typename runtime_registration_arg_traits<Arg>::type> &&
      (Qualified
           ? std::is_same_v<
                 typename runtime_lookup_entry_traits<Entry>::interface_type,
                 runtime_registration_arg_interface_t<Arg>>
           : std::is_void_v<runtime_registration_arg_interface_t<Arg>>);

  template <typename Entry, typename Arg>
  static constexpr bool runtime_registration_arg_matches_entry_v =
      runtime_registration_arg_keys_entry_v<Entry, Arg, true> ||
      runtime_registration_arg_keys_entry_v<Entry, Arg, false>;

  template <typename Arg, typename... KeyValueArgs>
  static constexpr std::size_t runtime_registration_key_arg_count_v =
      (std::size_t{0} + ... +
       (std::is_same_v<
                typename runtime_registration_arg_traits<Arg>::type,
                typename runtime_registration_arg_traits<KeyValueArgs>::type> &&
                std::is_same_v<
                    runtime_registration_arg_interface_t<Arg>,
                    runtime_registration_arg_interface_t<KeyValueArgs>>
            ? 1U
            : 0U));

  template <typename Entries, typename Arg>
  struct runtime_lookup_entry_key_count;

  template <typename... Entries, typename Arg>
  struct runtime_lookup_entry_key_count<type_list<Entries...>, Arg>
      : std::integral_constant<
            std::size_t,
            (std::size_t{0} + ... +
             (runtime_registration_arg_matches_entry_v<Entries, Arg> ? 1U
                                                                     : 0U))> {};

  template <typename InterfaceList, typename Arg>
  struct runtime_registration_key_match_count;

  template <typename... Interfaces, typename Arg>
  struct runtime_registration_key_match_count<type_list<Interfaces...>, Arg>
      : std::integral_constant<std::size_t,
                               (std::size_t{0} + ... +
                                runtime_lookup_entry_key_count<
                                    interface_lookup_entries_t<
                                        Interfaces, type_list<Interfaces...>>,
                                    Arg>::value)> {};

  template <typename InterfaceList, typename... KeyValueArgs>
  static constexpr void check_supplied_keys() {
    static_assert(
        ((runtime_registration_key_arg_count_v<KeyValueArgs, KeyValueArgs...> ==
          1U) &&
         ...),
        "duplicate runtime key value arguments for runtime lookup "
        "registration");
    static_assert(
        ((runtime_registration_key_match_count<InterfaceList,
                                               KeyValueArgs>::value > 0U) &&
         ...),
        "supplied runtime key value has no matching runtime-key lookup "
        "definition for the registered interface");
  }

  // Whether a key value of a registration supplies the key of a lookup entry.
  template <typename Entry, typename... KeyValueArgs>
  static constexpr bool has_registration_key() {
    return (runtime_registration_arg_matches_entry_v<Entry, KeyValueArgs> ||
            ...);
  }

  template <typename... KeyValueArgs, typename... Entries>
  static constexpr bool has_registration_keys(type_list<Entries...>) {
    return (has_registration_key<Entries, KeyValueArgs...>() && ...);
  }

  // A registration supplies the key of every runtime-key lookup it is
  // registered in. It is asserted here, not in a lambda, whose body MSVC
  // does not instantiate before the error it would explain.
  template <typename Entries, typename... KeyValueArgs>
  static constexpr void check_required_keys() {
    static_assert(has_registration_keys<KeyValueArgs...>(Entries{}),
                  "runtime-key lookup registration requires a runtime key "
                  "value for each declared runtime-key lookup");
  }

  // The index a lookup of an interface by a key goes to: the definition of the
  // key domain for the interface, or the base index for a static key without
  // one.
  template <typename Interface, typename Key> struct lookup_index_route {
  private:
    using key_definition = typename std::decay_t<Key>::definition_type;
    using explicit_entry =
        detail::selected_lookup_entry_t<Interface, key_definition,
                                        normalized_lookup_entries>;
    static constexpr bool is_static_key =
        detail::is_static_lookup_key_definition_v<key_definition>;

  public:
    using entry =
        std::conditional_t<is_static_key && std::is_void_v<explicit_entry>,
                           base_lookup_entry, explicit_entry>;
    static constexpr bool has_explicit_lookup =
        !is_static_key && !std::is_void_v<explicit_entry>;
    static constexpr bool has_explicit_collection_lookup =
        !std::is_void_v<explicit_entry> &&
        std::is_same_v<typename lookup_entry_cardinality<explicit_entry>::type,
                       ::dingo::many>;

    static decltype(auto) key(const Key &key) {
      if constexpr (std::is_same_v<entry, base_lookup_entry>) {
        return typed_key<Interface, Key>();
      } else {
        return key.backend_key();
      }
    }
  };

  // Passes fn the binding of each element of a collection, through the
  // operations of the lookup of its element: the lookup of the collection the
  // container declares, or the base lookup of the element.
  template <typename T, typename LookupKey, typename Fn,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  std::size_t visit_collection(runtime_bindings_state &state, LookupKey key,
                               Fn &&fn) {
    using collection_type = collection_traits<T>;
    using resolve_type = typename collection_type::resolve_type;
    using lookup_type = normalized_type_t<resolve_type>;
    using lookup_key_type = std::decay_t<LookupKey>;
    using route = lookup_index_route<lookup_type, lookup_key_type>;

    if constexpr (route::has_explicit_collection_lookup ||
                  std::is_same_v<typename route::entry, base_lookup_entry>) {
      decltype(auto) element_key = route::key(key);
      return route_operations<lookup_type, lookup_key_type>().for_each(
          state, std::addressof(element_key),
          [](void *visitor, runtime_binding_interface_type &binding) {
            (*static_cast<std::remove_reference_t<Fn> *>(visitor))(binding);
          },
          std::addressof(fn));
    } else {
      (void)fn;
      return 0;
    }
  }

  template <typename T, typename Fn, typename LookupKey,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  std::size_t append_bindings(runtime_bindings_state *state, T &results,
                              construction_scope scope,
                              runtime_context_type &context, Fn &&fn,
                              LookupKey key) {
    using collection_type = collection_traits<T>;
    using resolve_type = typename collection_type::resolve_type;
    if (!state) {
      return 0;
    }
    return visit_collection<T>(
        *state, key, [&](runtime_binding_interface_type &binding) {
          fn(results, ::dingo::resolve_binding<resolve_type, rtti_type>(
                          scope, binding, context));
        });
  }

  template <typename T, typename LookupKey,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  std::size_t count_collection(runtime_bindings_state *state, LookupKey key) {
    std::size_t result = 0;
    if (state) {
      visit_collection<T>(*state, key, [&](auto &) { ++result; });
    }
    return result;
  }

  template <typename LookupEntry, typename Key>
  static runtime_selection select_lookup(runtime_bindings_state &state,
                                         const Key &key) {
    auto &index = state.lookup_indexes.template get<LookupEntry>();
    auto match =
        detail::lookup_find_singular<LookupEntry, runtime_lookup_value>(index,
                                                                        key);
    if (match.ambiguous) {
      return runtime_selection::ambiguity();
    }
    return match.value ? runtime_selection::found(match.value->binding())
                       : runtime_selection::miss();
  }

  template <typename LookupEntry, typename Key, typename Fn>
  static std::size_t visit_lookups(runtime_bindings_state &state,
                                   const Key &key, Fn &&fn) {
    auto &index = state.lookup_indexes.template get<LookupEntry>();
    return detail::visit_entries<LookupEntry>(index, key, std::forward<Fn>(fn));
  }

  // The index of the key value that supplies the key of an entry: the first
  // qualified one, else the first unqualified one.
  template <typename Entry, typename... Args>
  static constexpr std::size_t key_value_index() {
    constexpr std::size_t qualified = pack_find<
        runtime_registration_arg_keys_entry_v<Entry, Args, true>...>();
    if constexpr (qualified < sizeof...(Args)) {
      return qualified;
    } else {
      return pack_find<
          runtime_registration_arg_keys_entry_v<Entry, Args, false>...>();
    }
  }

  template <typename Entry, typename... Args>
  static decltype(auto) registration_key_value(std::tuple<Args...> &keys) {
    constexpr std::size_t index =
        key_value_index<Entry, std::decay_t<Args>...>();
    static_assert(index < sizeof...(Args),
                  "missing runtime key value for runtime-key lookup "
                  "registration");
    return std::get<index>(keys).value();
  }

  // A registration the lookup rejects, named by its descriptors.
  static void check_lookup_commit(lookup_commit result,
                                  type_descriptor interface_type,
                                  type_descriptor storage,
                                  type_descriptor key_type) {
    if (result == lookup_commit::rejected) {
      throw detail::make_lookup_already_registered_exception(interface_type,
                                                             storage, key_type);
    }
    if (result == lookup_commit::sealed) {
      throw detail::make_container_sealed_exception(interface_type, storage,
                                                    key_type);
    }
  }

  // What a registration commits to a lookup: its value, and the
  // descriptors that name the registration if the lookup rejects it.
  struct lookup_registration {
    type_descriptor interface_type;
    type_descriptor storage;
    type_descriptor key_type;
  };

  // Inserts a binding into a lookup. It is of the lookup and its key, not of
  // the binding: the value of the binding is made by the caller.
  template <typename LookupEntry, typename KeyArg>
  static void insert_lookup(runtime_bindings_state &state,
                            runtime_transaction_type &transaction,
                            runtime_lookup_value value, const KeyArg &key_arg,
                            const lookup_registration &registration) {
    auto &index = state.lookup_indexes.template get<LookupEntry>();

    if constexpr (std::is_same_v<
                      typename lookup_entry_cardinality<LookupEntry>::type,
                      ::dingo::one>) {
      auto [handle, inserted] = index.try_emplace(key_arg, std::move(value));
      if (inserted) {
        undo_lookup<LookupEntry>(transaction, state, handle, key_arg);
      } else {
        check_lookup_commit(replace_lookup<LookupEntry>(transaction, state,
                                                        handle, key_arg,
                                                        std::move(value)),
                            registration.interface_type, registration.storage,
                            registration.key_type);
      }
    } else {
      auto handle = index.emplace(key_arg, std::move(value));
      undo_lookup<LookupEntry>(transaction, state, handle, key_arg);
    }
  }

  template <typename LookupEntry, typename Key, typename Binding>
  static void erase_lookup(runtime_bindings_state &state, const Key &key,
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

  template <typename LookupEntry, typename Handle, typename Key>
  static void undo_lookup(runtime_transaction_type &transaction,
                          runtime_bindings_state &state, Handle handle,
                          const Key &key) {
    auto &value = detail::lookup_iterator_value(handle);
    auto *binding = std::addressof(value.binding());
    try {
      transaction.defer_rollback(
          [&state, key = std::decay_t<Key>(key), binding]() noexcept {
            erase_lookup<LookupEntry>(state, key, binding);
          });
    } catch (...) {
      erase_lookup<LookupEntry>(state, key, binding);
      throw;
    }
  }

  // Redirects an occupied singular lookup to a new binding when the container
  // traits allow it and no resolve has sealed the container. The replaced
  // binding stays owned by the container; a rollback points the lookup back.
  template <typename LookupEntry, typename Handle, typename Key>
  static lookup_commit replace_lookup(runtime_transaction_type &transaction,
                                      runtime_bindings_state &state,
                                      Handle handle, const Key &key,
                                      runtime_lookup_value &&value) {
    if constexpr (!replaces_until_resolved) {
      (void)transaction;
      (void)state;
      (void)handle;
      (void)key;
      (void)value;
      return lookup_commit::rejected;
    } else {
      if (transaction.sealed()) {
        return lookup_commit::sealed;
      }
      auto &existing = detail::lookup_iterator_value(handle);
      auto *previous = std::addressof(existing.binding());
      auto *replacement = std::addressof(value.binding());
      transaction.defer_rollback([&state, key = std::decay_t<Key>(key),
                                  previous, replacement]() noexcept {
        restore_lookup<LookupEntry>(state, key, replacement, previous);
      });
      existing = std::move(value);
      return lookup_commit::replaced;
    }
  }

  template <typename LookupEntry, typename Key, typename Binding>
  static void restore_lookup(runtime_bindings_state &state, const Key &key,
                             Binding *replacement, Binding *previous) noexcept {
    auto &index = state.lookup_indexes.template get<LookupEntry>();
    auto it = index.find(key);
    assert(it != index.end());
    auto &value = detail::lookup_iterator_value(it);
    assert(std::addressof(value.binding()) == replacement);
    (void)replacement;
    value = runtime_lookup_value(previous);
  }

  // The operations of a lookup the container declares, of a key of it: one
  // table for each, of no binding or request type, by which the runtime
  // engine selects a request, commits a registration and enumerates a
  // collection.
  struct lookup_operations {
    runtime_selection (*find)(runtime_bindings_state &state, const void *key);
    void (*insert)(runtime_bindings_state &state,
                   runtime_transaction_type &transaction,
                   runtime_lookup_value value, const void *key,
                   const lookup_registration &registration);
    std::size_t (*for_each)(runtime_bindings_state &state, const void *key,
                            void (*visit)(void *,
                                          runtime_binding_interface_type &),
                            void *visitor);
  };

  template <typename LookupEntry, typename Key> struct lookup_operations_v {
    static runtime_selection find(runtime_bindings_state &state,
                                  const void *key) {
      return select_lookup<LookupEntry>(state, *static_cast<const Key *>(key));
    }

    static void insert(runtime_bindings_state &state,
                       runtime_transaction_type &transaction,
                       runtime_lookup_value value, const void *key,
                       const lookup_registration &registration) {
      insert_lookup<LookupEntry>(state, transaction, std::move(value),
                                 *static_cast<const Key *>(key), registration);
    }

    static std::size_t for_each(runtime_bindings_state &state, const void *key,
                                void (*visit)(void *,
                                              runtime_binding_interface_type &),
                                void *visitor) {
      return visit_lookups<LookupEntry>(state, *static_cast<const Key *>(key),
                                        [&](runtime_lookup_value &entry) {
                                          visit(visitor, entry.binding());
                                        });
    }

    static constexpr lookup_operations value{&find, &insert, &for_each};
  };

  // The operations of the lookup of a key of an interface, routed as the
  // container declares it.
  template <typename Interface, typename LookupKey>
  static const lookup_operations &route_operations() {
    using route = lookup_index_route<Interface, std::decay_t<LookupKey>>;
    using entry = typename route::entry;
    static_assert(!std::is_void_v<entry>,
                  "keyed registration or request has no matching "
                  "dingo lookup definition for interface/key");
    using key_type = std::decay_t<decltype(route::key(
        std::declval<const std::decay_t<LookupKey> &>()))>;
    return lookup_operations_v<entry, key_type>::value;
  }

  using runtime_type = runtime_memory<Allocator>;

private:
  static runtime_data_type &borrow_data(self_type *root) {
    auto &data = root->shared_runtime_data();
    static_assert(
        std::is_same_v<std::remove_reference_t<decltype(data)>,
                       runtime_data_type>,
        "registration container traits must preserve the root runtime lookup "
        "and allocator configuration");
    return data;
  }

private:
  // A container is its own root; a level of local bindings is not.
  bool owns_data() const noexcept {
    return root_ == static_cast<const self_type *>(this);
  }

public:
  allocator_type &get_allocator() {
    return allocator_base<allocator_type>::get_allocator();
  }

  // Registers TypeArgs, built by an optional factory argument and found by
  // the runtime key values that follow it.
  template <typename... TypeArgs, typename... Args>
  auto register_type(Args &&...args) {
    if constexpr (detail::are_runtime_registration_key_args_v<Args...>) {
      return this->template prepare_binding<TypeArgs...>(
          none_t{}, std::forward<Args>(args)...);
    } else {
      return register_argument<TypeArgs...>(std::forward<Args>(args)...);
    }
  }

  template <typename... TypeArgs, typename Fn>
  auto register_type_collection(Fn &&fn) {
    using registration = type_registration<TypeArgs...>;
    return register_type<TypeArgs...>(
        callable([this, collection_fn = std::forward<Fn>(fn)]() mutable {
          return (*this)
              .template construct_collection<
                  typename registration::storage_type::type>(collection_fn);
        }));
  }

  template <typename... TypeArgs> auto register_type_collection() {
    return register_type_collection<TypeArgs...>(
        detail::binding_collection_append{});
  }

private:
  template <typename... TypeArgs, typename Arg, typename... KeyArgs>
  auto register_argument(Arg &&arg, KeyArgs &&...keys) {
    static_assert(detail::are_runtime_registration_key_args_v<KeyArgs...>,
                  "register_type takes a factory argument followed by "
                  "runtime key values");
    return this->template prepare_binding<TypeArgs...>(
        std::forward<Arg>(arg), std::forward<KeyArgs>(keys)...);
  }

public:
protected:
  runtime_memory<allocator_type> &runtime() {
    return shared_runtime_data().runtime;
  }

  runtime_data_type &shared_runtime_data() {
    if (owns_data()) {
      return runtime_data_.get();
    }
    return borrow_data(resolve_root());
  }

  runtime_bindings_state *runtime_bindings() {
    if (owns_data()) {
      return std::addressof(shared_runtime_data().bindings);
    }
    auto *scope = runtime_scope();
    return scope != nullptr ? std::addressof(scope->bindings) : nullptr;
  }

  runtime_scope_state *runtime_scope() {
    assert(!owns_data());
    const auto scope = runtime_data_.scope_id();
    if (scope == decltype(runtime_data_)::invalid_scope) {
      return nullptr;
    }
    return shared_runtime_data().find_scope(scope);
  }

  void release_scope(runtime_scope_state *state) noexcept {
    shared_runtime_data().release_scope(runtime_data_.scope_id(), state);
    runtime_data_.scope_id(decltype(runtime_data_)::invalid_scope);
  }

  runtime_bindings_state &make_bindings(runtime_transaction_type &transaction) {
    if (owns_data()) {
      return *runtime_bindings();
    }
    if (auto *scope = runtime_scope()) {
      return scope->bindings;
    }

    auto &data = shared_runtime_data();
    auto *state =
        transaction.template construct_persistent<runtime_scope_state>(
            get_allocator(), static_cast<void *>(this));
    runtime_data_.scope_id(data.create_scope(state));
    try {
      transaction.defer_rollback(
          +[](void *self, void *scope) noexcept {
            static_cast<self_type *>(self)->release_scope(
                static_cast<runtime_scope_state *>(scope));
          },
          this, state);
    } catch (...) {
      release_scope(state);
      throw;
    }
    return state->bindings;
  }

  // The container a level of local bindings falls back to: the container,
  // or the level, the registration it belongs to is made in. None for a
  // container.
  self_type *owner_level() const noexcept {
    return owns_data() ? nullptr : runtime_data_.owner();
  }

  // The container that owns the runtime data.
  self_type *resolve_root() { return root_; }

protected:
  // A lookup the traits do not declare fails as it compiles, in a level of
  // local bindings as in its container: they have the same traits.

  template <typename T, typename Fn, typename LookupKey,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  T construct_collection(construction_scope scope,
                         runtime_context_type &context, Fn &&fn,
                         LookupKey key) {
    using collection_type = collection_traits<T>;
    static_assert(collection_type::is_collection,
                  "missing collection_traits specialization for type T");
    const collection_source source = collection_bindings<T>(key);
    // A declared collection lookup with no rows is a valid empty collection.
    if (source.count == 0 &&
        !lookup_index_route<typename collection_type::resolve_type,
                            LookupKey>::has_explicit_collection_lookup) {
      throw detail::make_collection_type_not_found_exception<
          T, typename collection_type::resolve_type>();
    }
    T results;
    collection_type::reserve(results, source.count);
    append_bindings(source.state, results, scope, context, std::forward<Fn>(fn),
                    std::move(key));
    return results;
  }

  template <typename T, typename LookupKey,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  std::size_t count_collection(LookupKey key) {
    return collection_bindings<T>(key).count;
  }

  // The bindings a collection is made of, and how many of them: those of the
  // container, or of the first level it falls back to that binds an element
  // of the collection, as a request selects a binding.
  struct collection_source {
    runtime_bindings_state *state;
    std::size_t count;
  };

  template <typename T, typename LookupKey>
  collection_source collection_bindings(const LookupKey &key) {
    self_type *level = this;
    for (;;) {
      auto *state = level->runtime_bindings();
      const std::size_t count = count_collection<T>(state, key);
      self_type *owner = level->owner_level();
      if (count != 0 || owner == nullptr) {
        return {state, count};
      }
      level = owner;
    }
  }

  // The level of the local bindings of a registration, made as it is first
  // asked for.
  class container_proxy {
  public:
    using container_type = self_type;

    container_proxy(runtime_binding_header *header, self_type *registry)
        : header_(header), registry_(registry) {}

    container_type &get() const {
      if (header_->level == nullptr) {
        header_->level = std::addressof(
            registry_->runtime().template construct<container_type>(
                detail::local_level,
                static_cast<container_type *>(header_->container),
                registry_->get_allocator()));
      }
      return *static_cast<container_type *>(header_->level);
    }
    container_type *operator->() const { return std::addressof(get()); }
    operator container_type &() const { return get(); }

  private:
    runtime_binding_header *header_;
    self_type *registry_;
  };

  // The edge of the binding of an interface of a registration.
  template <typename TypeInterface, typename TypeStorage>
  using runtime_registration_edge_t =
      runtime_binding_edge<container_type, unannotated_t<TypeInterface>,
                           TypeStorage>;

private:
  template <typename... TypeArgs, typename Arg, typename... KeyValueArgs>
  // Registration is cold; keep its type-specific setup out of callers.
  // NOLINTNEXTLINE(readability-function-cognitive-complexity,readability-function-size)
  DINGO_NOINLINE auto prepare_binding(Arg &&arg, KeyValueArgs &&...key_values) {
    using registration =
        std::conditional_t<!is_none_v<std::decay_t<Arg>>,
                           type_registration<TypeArgs..., factory<Arg>>,
                           type_registration<TypeArgs...>>;
    static_assert(
        !detail::is_key_value_v<typename registration::key_type>,
        "dingo::key_type<T, V> registration keys require a static fixed "
        "runtime-key request");
    using binding_model = detail::binding_model<registration>;
    using bindings_type = typename binding_model::bindings_type;
    static_assert(std::is_void_v<bindings_type>,
                  "runtime registrations cannot declare compile-time "
                  "bindings<...>; use static_container");
    (void)std::addressof(arg);
    using interface_types = typename binding_model::interface_types;
    constexpr bool valid = detail::registration_checked_v<registration>;
    if constexpr (!binding_model::stored) {
      return unreachable<container_proxy>();
    } else {
      using storage_type = typename binding_model::storage_type;
      using registration_key = typename binding_model::key_type;

      if constexpr (!valid) {
        return unreachable<container_proxy>();
      } else {
        if constexpr (sizeof...(KeyValueArgs) > 0) {
          check_supplied_keys<interface_types, KeyValueArgs...>();
        }
        return commit_registration<storage_type>(
            registration_key{}, std::forward<Arg>(arg), interface_types{},
            key_values...);
      }
    }
  }

  // The transaction of a registration, with its scratch memory, made and
  // destroyed out of line, once for a container, not for every registration.
  class registration_frame {
  public:
    DINGO_NOINLINE explicit registration_frame(runtime_type &runtime)
        : transaction_(runtime, scratch_) {}
    DINGO_NOINLINE ~registration_frame() = default;
    registration_frame(const registration_frame &) = delete;
    registration_frame &operator=(const registration_frame &) = delete;

    runtime_transaction_type &transaction() noexcept { return transaction_; }

  private:
    inline_arena<DINGO_CONTEXT_ARENA_BUFFER_SIZE> scratch_;
    runtime_transaction_type transaction_;
  };

  using runtime_binding_node_type =
      typename runtime_binding_interface_type::node_type;

  // A registration as constant data: the layout of its record, which
  // destroys its cell, and the node and the descriptor of the binding of each
  // of its interfaces.
  struct registration_node {
    std::size_t size;
    std::size_t alignment;
    std::size_t cell_offset;
    std::size_t bindings_offset;
    void (*destroy)(void *) noexcept;
    const runtime_binding_node_type *const *nodes;
    const type_descriptor *interfaces;
    std::size_t count;
    type_descriptor storage;
  };

  // Constructs the cell of a registration in its record, from the registered
  // factory argument when there is one.
  template <typename Cell, typename Arg>
  static void construct_cell(void *cell, void *arg) {
    if constexpr (is_none_v<std::decay_t<Arg>>) {
      (void)arg;
      ::new (cell) Cell();
    } else {
      ::new (cell) Cell(
          std::forward<Arg>(*static_cast<std::remove_reference_t<Arg> *>(arg)));
    }
  }

  template <typename TypeStorage, typename Record, typename... TypeInterfaces>
  struct registration_node_v {
    static constexpr const runtime_binding_node_type *nodes[] = {
        &runtime_registration_edge_t<TypeInterfaces, TypeStorage>::node...};
    static constexpr type_descriptor interfaces[] = {
        detail::described_type_v<TypeInterfaces>...};
    static constexpr registration_node value{
        Record::size,
        Record::alignment,
        detail::runtime_cell_offset_v<detail::binding_cell<TypeStorage>>,
        Record::bindings_offset,
        &Record::destroy,
        nodes,
        interfaces,
        sizeof...(TypeInterfaces),
        detail::described_type_v<typename TypeStorage::type>};
  };

  using lookup_operations = lookup_operations;

  // A lookup a registration commits the binding of one of its interfaces to:
  // the index of the binding of the interface, the lookup, the key, and the
  // descriptor of the type of the key, which names the registration if the
  // lookup rejects it.
  struct lookup_insert {
    std::size_t binding;
    const lookup_operations *lookup;
    const void *key;
    type_descriptor key_type;
  };

  // The lookups of an interface of a registration: of the static key of the
  // registration, as the container routes it, or of each key value it is
  // registered with, to each lookup of the interface that is keyed by it.
  template <std::size_t Index, typename TypeInterface, typename Interfaces,
            typename LookupKey, typename... KeyValueArgs>
  static void lookup_inserts(lookup_insert *&out, const LookupKey &lookup_key,
                             const void *static_key,
                             std::tuple<KeyValueArgs &...> &key_values) {
    using key_type = std::decay_t<LookupKey>;
    using entries = interface_lookup_entries_t<TypeInterface, Interfaces>;
    if constexpr (sizeof...(KeyValueArgs) > 0 &&
                  interface_lookup_entries<
                      TypeInterface, Interfaces,
                      normalized_lookup_entries>::defers_normalized) {
      // A keyed request of an interface that defers to its normalized
      // interface selects the lookup of that one, which commits it.
      (void)out, (void)lookup_key, (void)static_key, (void)key_values;
    } else if constexpr (sizeof...(KeyValueArgs) == 0 ||
                         type_list_size_v<entries> == 0) {
      // The lookup of the static key of the registration, as the container
      // routes it, also of an interface that no key value keys.
      (void)key_values;
      using route = lookup_index_route<TypeInterface, key_type>;
      using route_key = std::decay_t<decltype(route::key(lookup_key))>;
      *out++ = {Index, &route_operations<TypeInterface, key_type>(), static_key,
                detail::registration_key_type<route_key>(lookup_key)};
    } else {
      (void)static_key;
      check_required_keys<entries, std::decay_t<KeyValueArgs>...>();
      for_each(entries{}, [&](auto element) {
        using entry = typename decltype(element)::type;
        const auto &key = registration_key_value<entry>(key_values);
        using key_arg = std::decay_t<decltype(key)>;
        *out++ = {Index, &lookup_operations_v<entry, key_arg>::value,
                  std::addressof(key),
                  detail::registration_key_type<key_arg>(lookup_key)};
      });
    }
  }

  template <typename TypeInterface, typename Interfaces, typename... Args>
  static constexpr std::size_t insert_count() {
    using entries = interface_lookup_entries_t<TypeInterface, Interfaces>;
    if constexpr (sizeof...(Args) == 0 || type_list_size_v<entries> == 0) {
      return 1;
    } else {
      return type_list_size_v<entries>;
    }
  }

  // Registers a type: the typed part of a registration is its node, the
  // construction of its cell from the registered factory argument, and the
  // keys of the lookups of its interfaces; the commit is of the node, by one
  // function of the container. Arg is the type of the factory argument as it
  // was passed.
  template <typename TypeStorage, typename LookupKey, typename Arg,
            typename... TypeInterfaces, typename... KeyValueArgs>
  container_proxy commit_registration(LookupKey lookup_key, Arg &&arg,
                                      type_list<TypeInterfaces...>,
                                      KeyValueArgs &...key_values) {
    using interfaces = type_list<TypeInterfaces...>;
    using cell_type = detail::binding_cell<TypeStorage>;
    using record = detail::runtime_binding_record<
        cell_type, runtime_binding_interface_type, sizeof...(TypeInterfaces)>;
    (detail::interface_registration_requirements<
         TypeStorage, unannotated_t<TypeInterfaces>,
         typename TypeStorage::type>::assert_valid(),
     ...);
    // The keys of a static key, which a lookup is routed to by the type of
    // each interface: they live as long as the commit.
    const auto static_keys = std::make_tuple(
        lookup_index_route<TypeInterfaces, std::decay_t<LookupKey>>::key(
            lookup_key)...);
    std::tuple<KeyValueArgs &...> keys(key_values...);
    constexpr std::size_t count =
        (insert_count<TypeInterfaces, interfaces, KeyValueArgs...>() + ... + 0);
    lookup_insert inserts[count + 1] = {};
    lookup_insert *out = inserts;
    insert_all<TypeInterfaces...>(out, lookup_key, static_keys, keys,
                                  std::index_sequence_for<TypeInterfaces...>{});
    return commit_record(
        registration_node_v<TypeStorage, record, TypeInterfaces...>::value,
        inserts, static_cast<std::size_t>(out - inserts),
        &construct_cell<cell_type, Arg>,
        const_cast<void *>(
            static_cast<const volatile void *>(std::addressof(arg))));
  }

  template <typename... TypeInterfaces, typename LookupKey, typename StaticKeys,
            typename KeyValues, std::size_t... Indexes>
  static void insert_all(lookup_insert *&out, const LookupKey &lookup_key,
                         const StaticKeys &static_keys, KeyValues &key_values,
                         std::index_sequence<Indexes...>) {
    (lookup_inserts<Indexes, TypeInterfaces, type_list<TypeInterfaces...>>(
         out, lookup_key, std::addressof(std::get<Indexes>(static_keys)),
         key_values),
     ...);
  }

  // Commits a registration of its node, in one transaction: constructs its
  // record, its header, its cell, and the binding of each of its interfaces,
  // and inserts each binding into the lookups of its interface. It is of no
  // registration type.
  DINGO_NOINLINE container_proxy commit_record(
      const registration_node &node, const lookup_insert *inserts,
      std::size_t count, void (*make_cell)(void *, void *), void *arg) {
    registration_frame frame(runtime());
    auto &transaction = frame.transaction();
    auto &state = make_bindings(transaction);
    auto *header = static_cast<runtime_binding_header *>(
        transaction.construct_persistent_erased(
            node.size, node.alignment, node.destroy, [&](void *place) {
              auto *made = ::new (place)
                  runtime_binding_header{{nullptr, nullptr}, this, nullptr};
              make_cell(static_cast<unsigned char *>(place) + node.cell_offset,
                        arg);
              for (std::size_t index = 0; index < node.count; ++index) {
                ::new (binding_address(node, *made, index))
                    runtime_binding_interface_type(node.nodes[index], made);
              }
            }));
    for (std::size_t index = 0; index < count; ++index) {
      const lookup_insert &insert = inserts[index];
      insert.lookup->insert(
          state, transaction,
          runtime_lookup_value(binding_address(node, *header, insert.binding)),
          insert.key,
          {node.interfaces[insert.binding], node.storage, insert.key_type});
    }
    transaction.commit();
    return container_proxy(header, this);
  }

  static runtime_binding_interface_type *
  binding_address(const registration_node &node, runtime_binding_header &header,
                  std::size_t index) noexcept {
    return reinterpret_cast<runtime_binding_interface_type *>(
               reinterpret_cast<unsigned char *>(&header) +
               node.bindings_offset) +
           index;
  }

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4702)
#endif
  friend struct detail::request_resolution;

  // A request as data: the lookup and the key of its exact interface and of
  // its normalized interface, whether it is selected by its exact interface
  // first, and whether it falls back to its normalized interface when its
  // exact interface binds nothing.
  struct lookup_request {
    const lookup_operations *exact;
    const void *exact_key;
    const lookup_operations *value;
    const void *value_key;
    bool exact_first;
    bool falls_back;
  };

  // Selects the binding of a request of its data. It is of no request type.
  runtime_selection select_record(const lookup_request &request) {
    auto *state = runtime_bindings();
    if (state == nullptr) {
      return runtime_selection::miss();
    }
    if (!request.exact_first) {
      return request.value->find(*state, request.value_key);
    }
    using lookup_target = std::pair<const lookup_operations *, const void *>;
    return detail::select_exact_first(
        [&](const lookup_target &at) {
          return at.first->find(*state, at.second);
        },
        lookup_target(request.exact, request.exact_key),
        lookup_target(request.value, request.value_key), request.falls_back);
  }

  // Selects a request by select, of its data, while its keys exist: the lookups
  // its interfaces are routed to, as the container declares them. A static key
  // selects the exact interface first, and the normalized one when the exact
  // one binds nothing. A key value selects by the exact interface only when it
  // has a lookup of its own, falling back to the normalized one when that has
  // one too.
  template <typename Request, typename LookupKey, typename Selection>
  Selection select_key(const LookupKey &key,
                       Selection (self_type::*select)(const lookup_request &)) {
    using key_type = std::decay_t<LookupKey>;
    using exact = typename Request::exact_type;
    using value = typename Request::value_type;
    using exact_route = lookup_index_route<exact, key_type>;
    using value_route = lookup_index_route<value, key_type>;
    if constexpr (detail::is_static_lookup_key_definition_v<key_type>) {
      decltype(auto) exact_key = exact_route::key(key);
      decltype(auto) value_key = value_route::key(key);
      return (this->*select)(lookup_request{
          &route_operations<exact, key_type>(), std::addressof(exact_key),
          &route_operations<value, key_type>(), std::addressof(value_key), true,
          !std::is_same_v<value, exact>});
    } else if constexpr (!std::is_same_v<value, exact> &&
                         exact_route::has_explicit_lookup &&
                         !std::is_same_v<typename exact_route::entry,
                                         typename value_route::entry>) {
      decltype(auto) exact_key = exact_route::key(key);
      if constexpr (value_route::has_explicit_lookup) {
        decltype(auto) value_key = value_route::key(key);
        return (this->*select)(lookup_request{
            &route_operations<exact, key_type>(), std::addressof(exact_key),
            &route_operations<value, key_type>(), std::addressof(value_key),
            true, true});
      } else {
        return (this->*select)(lookup_request{
            &route_operations<exact, key_type>(), std::addressof(exact_key),
            nullptr, nullptr, true, false});
      }
    } else {
      decltype(auto) value_key = value_route::key(key);
      return (this->*select)(
          lookup_request{nullptr, nullptr, &route_operations<value, key_type>(),
                         std::addressof(value_key), false, false});
    }
  }

  template <typename Request, typename LookupKey>
  level_selection select_request(const LookupKey &key) {
    static_assert(!std::is_const_v<typename Request::value_type>);
    return select_key<Request>(key, &self_type::select_levels);
  }

  // The binding of a selection resolves the request, through the cache of
  // the binding where the request may be cached.
  template <typename Request, typename R>
  R resolve_selected(runtime_selection selection, construction_scope scope,
                     runtime_context_type &context) {
    using interface_type = typename Request::interface_type;
    auto &binding = *selection.binding;
    if constexpr (detail::cache::supports_v<interface_type>) {
      return ::dingo::resolve_binding<interface_type, rtti_type>(
          scope, binding, context, detail::cache::key<interface_type>());
    } else {
      return ::dingo::resolve_binding<interface_type, rtti_type>(scope, binding,
                                                                 context);
    }
  }

  template <typename R, typename LookupKey>
  R construct_request_collection(construction_scope scope,
                                 runtime_context_type &context, LookupKey key) {
    return construct_collection<R>(
        scope, context, detail::binding_collection_append{}, std::move(key));
  }

#ifdef _MSC_VER
#pragma warning(pop)
#endif

  self_type *root_ = nullptr;

  detail::runtime_data_holder<runtime_data_type, self_type> runtime_data_;

public:
  // A container without an explicit parent type accepts a parent of its own
  // type.
  using parent_container_type =
      std::conditional_t<std::is_void_v<ParentContainer>, self_type,
                         detail::container_implementation_t<ParentContainer>>;

private:
  // The parent answers only the lookups it declares.
  template <typename Lookup, typename LookupKey>
  static constexpr bool parent_serves() {
    static_assert(detail::is_lookup_key_v<LookupKey>);
    return detail::has_lookup<parent_container_type, Lookup, LookupKey>();
  }

public:
  runtime_container_impl() : runtime_container_impl(allocator_type()) {}

  explicit runtime_container_impl(allocator_type alloc)
      : runtime_container_impl(nullptr, alloc) {}

  // A container owns the runtime data.
  runtime_container_impl(parent_container_type *parent,
                         const allocator_type &alloc = allocator_type())
      : allocator_base<Allocator>(alloc), root_(this),
        runtime_data_(get_allocator()), parent_(parent) {
    validate_lookup_definitions();
  }

  // A level of the local bindings of a registration made in owner. It borrows
  // the runtime data of the root of owner, and a request it binds no binding
  // for falls back to owner.
  runtime_container_impl(detail::local_level_t, self_type *owner,
                         const allocator_type &alloc = allocator_type())
      : allocator_base<Allocator>(alloc), root_(owner->root_),
        runtime_data_(owner) {}

  // Resolved instances reference each other by address and runtime bindings
  // point back at the container, so it cannot move. runtime_container moves by
  // owning it through a handle.
  runtime_container_impl(const self_type &) = delete;
  runtime_container_impl(self_type &&) = delete;
  // The container destroys the runtime data it owns as it is destroyed,
  // while it can still tell whether it is its own root.
  ~runtime_container_impl() { runtime_data_.destroy(owns_data()); }
  self_type &operator=(const self_type &) = delete;
  self_type &operator=(self_type &&) = delete;

private:
  template <typename Request, typename R> static constexpr bool is_cacheable() {
    return detail::cache::supports_v<typename Request::interface_type> &&
           !collection_traits<R>::is_collection;
  }

  // Selects a request of its data in the container, and in the levels of
  // local bindings it falls back to: the first level that binds it,
  // ambiguously or not, or else the last one. It is of no request type.
  DINGO_NOINLINE level_selection select_levels(const lookup_request &request) {
    self_type *level = this;
    for (;;) {
      auto selection = level->select_record(request);
      self_type *owner = level->owner_level();
      if (!detail::falls_back(selection.status) || owner == nullptr) {
        return {selection, level};
      }
      level = owner;
    }
  }

  // Resolves a request in the context of a request of this runtime. The
  // algorithm resolves the request of its selection in the levels, in the
  // last level it was selected in: the binding it found, or the parent of
  // that level, or else the origin. A collection is constructed by the
  // algorithm.
  template <typename Request, typename R, typename Origin, typename LookupKey>
  R resolve_local(construction_scope scope, runtime_context_type &context,
                  Origin &origin, LookupKey key) {
    if constexpr (!collection_traits<R>::is_collection) {
      return detail::request_resolution::resolve_level<Request, R>(
          select_request<Request>(key), scope, context, origin, key);
    } else {
      return detail::request_resolution::resolve<Request, R>(
          *this, scope, context, origin, std::move(key));
    }
  }

  // Resolves a request of its selection in a transaction of the container,
  // which its frame opens and ends, out of line of the entry.
  template <typename Request, typename R, typename LookupKey>
  DINGO_NOINLINE R resolve_transacted(const level_selection &found,
                                      LookupKey &key) {
    detail::transaction_frame<runtime_type> frame(this->runtime());
    return detail::request_resolution::resolve_level<Request, R>(
        found, entry_scope(caller_scope), frame.context(), *this, key);
  }

  // A request that may be cached reads the cache of its selected binding
  // before it enters a transaction.
  template <typename Request, typename R, typename LookupKey>
  R resolve_entry(LookupKey key) {
    using interface_type = typename Request::interface_type;
    if constexpr (!collection_traits<R>::is_collection) {
      const level_selection found = select_request<Request>(key);
      if constexpr (is_cacheable<Request, R>()) {
        auto result =
            this->lookup_cache(found, detail::cache::key<interface_type>());
        if (result.hit) {
          return detail::resolved_value<interface_type>(
              {result.address, resolved_address::access_kind::borrow});
        }
      }
      return resolve_transacted<Request, R>(found, key);
    } else {
      return transact([&](runtime_context_type &context) -> R {
        return detail::request_resolution::resolve<Request, R>(
            *this, entry_scope(caller_scope), context, *this, std::move(key));
      });
    }
  }

public:
  // The registry base and the registration and resolution interfaces declare
  // some of the same names; these are the ones a container exposes.
  using detail::container_requests<self_type>::construct;
  using detail::container_requests<self_type>::resolve;
  using detail::container_requests<self_type>::construct_collection;
  using detail::container_requests<self_type>::count_collection;

  template <typename T, typename Fn, typename LookupKey,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  T construct_collection(Fn &&fn, LookupKey key) {
    return transact([&](runtime_context_type &context) -> T {
      return construct_collection<T>(entry_scope(caller_scope), context,
                                     std::forward<Fn>(fn), std::move(key));
    });
  }

  template <typename Fn> decltype(auto) transact(Fn &&fn) {
    return execute_transaction(this->runtime(), std::forward<Fn>(fn));
  }

  // The scope a request enters with, known only at run time: the resolution
  // of the bindings a runtime container selects at run time is instantiated
  // once for every scope, not once for every scope known as the code
  // compiles.
  template <construction_lifetime Lifetime>
  static constexpr construction_scope
  entry_scope(fixed_construction_scope<Lifetime>) {
    return construction_scope(Lifetime);
  }

  template <typename Request, typename IdType>
  detail::binding_status binding_status(IdType &&id) {
    using request = request_type<Request>;
    auto key = detail::make_lookup_key(std::forward<IdType>(id));
    const level_selection found = this->template select_request<request>(key);
    if constexpr (parent_serves<typename request::lookup_type,
                                decltype(key)>()) {
      auto *parent = found.level->parent_;
      if (parent && found.status == detail::binding_status::not_found) {
        return parent->template binding_status<Request>(key);
      }
    }
    return found.status;
  }

private:
  friend struct detail::request_resolution;
  friend class detail::container_requests<self_type>;

  parent_container_type *parent() { return parent_; }

  // The request itself if its binding publishes it, otherwise its normalized
  // value.
  template <typename Request, typename R, typename Selection>
  R resolve_normalizing(Selection, construction_scope scope,
                        runtime_context_type &context) {
    try {
      return detail::request_resolution::resolve<Request, R>(
          *this, scope, context, *this, detail::no_lookup_key());
    } catch (const type_not_convertible_exception &) {
      return detail::request_resolution::resolve_normalized<Request, R>(
          *this, scope, context);
    }
  }

  // A request in a context of another runtime opens a transaction of this one.
  template <typename Request, typename R, typename Origin, typename LookupKey>
  DINGO_ALWAYS_INLINE R resolve_request(construction_scope scope,
                                        runtime_context_type &context,
                                        Origin &origin, LookupKey key) {
    if (context.owns(this->runtime())) {
      return resolve_local<Request, R>(scope, context, origin, std::move(key));
    }
    return resolve_nested<Request, R>(scope, context, origin, std::move(key));
  }
  // A request in the context of a static container, which a static child
  // passes to this parent, opens a transaction of this container: the parent
  // resolves it as an operation of its own, and rolls it back if it fails.
  template <
      typename Request, typename R, typename Context, typename Origin,
      typename LookupKey, typename Scope,
      std::enable_if_t<!std::is_same_v<Context, runtime_context_type>, int> = 0>
  DINGO_NOINLINE R resolve_request(Scope scope, Context &, Origin &origin,
                                   LookupKey key) {
    return transact([&](runtime_context_type &context) -> R {
      return detail::request_resolution::resolve<Request, R>(
          *this, construction_scope(scope), context, origin, std::move(key));
    });
  }

  template <typename Request, typename R, typename Origin, typename LookupKey>
  DINGO_NOINLINE R resolve_nested(construction_scope scope,
                                  runtime_context_type &context, Origin &origin,
                                  LookupKey key) {
    assert(!context.owns(this->runtime()));
    detail::nested_transaction_frame<runtime_type, Allocator> frame(
        this->runtime(), context);
    return resolve_local<Request, R>(scope, context, origin, std::move(key));
  }

public:
  template <typename Request, bool MayAutoConstruct, typename LookupKey,
            typename R = typename Request::lookup_type,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  R unresolved(construction_scope scope, runtime_context_type &context,
               LookupKey key) {
    return detail::request_resolution::unresolved<Request, MayAutoConstruct, R>(
        *this, *this->resolve_root(), scope, context, key);
  }

private:
  parent_container_type *parent_ = nullptr;
};

} // namespace dingo

#ifdef _MSC_VER
#pragma warning(pop)
#endif
