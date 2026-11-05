//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/base/config.h>
#include <dingo/base/exceptions.h>
#include <dingo/base/none.h>
#include <dingo/base/type_list.h>
#include <dingo/binding/binding_collection.h>
#include <dingo/binding/binding_model.h>
#include <dingo/binding/binding_selection.h>
#include <dingo/binding/container_interface.h>
#include <dingo/factory/callable.h>
#include <dingo/lookup/lookup.h>
#include <dingo/lookup/operations.h>
#include <dingo/memory/allocator.h>
#include <dingo/registration/requirements.h>
#include <dingo/runtime/binding.h>
#include <dingo/runtime/container_runtime.h>
#include <dingo/runtime/container_traits.h>
#include <dingo/runtime/context.h>
#include <dingo/runtime/lookup_index.h>
#include <dingo/runtime/registration_api.h>
#include <dingo/runtime/transaction.h>
#include <dingo/storage/interface_storage_traits.h>
#include <dingo/type/annotated.h>
#include <dingo/type/auto_constructible.h>
#include <dingo/type/collection_traits.h>
#include <dingo/type/dependency_traits.h>
#include <dingo/type/key.h>
#include <dingo/type/normalized_type.h>

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <functional>
#include <limits>
#include <map>
#include <memory>
#include <tuple>
#include <type_traits>
#include <typeindex>
#include <utility>
#include <variant>
#include <vector>

namespace dingo {
namespace detail {

template <typename Derived> class runtime_registration_api;

template <typename ContainerTraits, typename Allocator,
          typename ParentContainer>
class runtime_container_impl;

// The binding a lookup entry selects.
template <typename BindingInterface> class runtime_lookup_value {
public:
  explicit runtime_lookup_value(BindingInterface *binding)
      : binding_(binding) {}

  runtime_lookup_value(runtime_lookup_value &&) noexcept = default;
  runtime_lookup_value &operator=(runtime_lookup_value &&) noexcept = default;
  runtime_lookup_value(const runtime_lookup_value &) = delete;
  runtime_lookup_value &operator=(const runtime_lookup_value &) = delete;

  BindingInterface &binding() { return *binding_; }

private:
  BindingInterface *binding_;
};

template <typename LookupEntries, typename BindingInterface, typename Allocator>
struct runtime_bindings_state {
  using value_type = runtime_lookup_value<BindingInterface>;

  explicit runtime_bindings_state(Allocator &allocator)
      : lookup_indexes(allocator) {}

  lookup_index<LookupEntries, value_type, Allocator> lookup_indexes;
};

template <typename BindingsState> struct runtime_scope_state {
  template <typename Allocator>
  runtime_scope_state(Allocator &allocator, void *parent)
      : bindings(allocator), registration_parent(parent) {}

  BindingsState bindings;
  void *registration_parent = nullptr;
};

template <typename Allocator, typename BindingsState>
struct runtime_registry_data {
  using scope_state_type = runtime_scope_state<BindingsState>;
  using scope_pointer_allocator = typename std::allocator_traits<
      Allocator>::template rebind_alloc<scope_state_type *>;

  explicit runtime_registry_data(Allocator &allocator)
      : runtime(allocator), bindings(allocator),
        scope_states(scope_pointer_allocator(allocator)) {}

  std::size_t create_scope(scope_state_type *state) {
    scope_states.push_back(state);
    return scope_states.size();
  }

  void release_scope(std::size_t scope, scope_state_type *state) noexcept {
    assert(scope > 0 && scope <= scope_states.size());
    assert(scope_states[scope - 1] == state);
    (void)state;
    scope_states[scope - 1] = nullptr;
    if (scope == scope_states.size()) {
      scope_states.pop_back();
    }
  }

  scope_state_type *find_scope(std::size_t scope) noexcept {
    assert(scope > 0 && scope <= scope_states.size());
    return scope_states[scope - 1];
  }

  container_runtime<Allocator> runtime;
  BindingsState bindings;
  std::vector<scope_state_type *, scope_pointer_allocator> scope_states;
};

template <typename Data, bool OwnsData> class runtime_data_holder;

template <typename Data> class runtime_data_holder<Data, true> {
public:
  template <typename Allocator>
  explicit runtime_data_holder(Allocator &allocator) : data_(allocator) {}

  Data &get() noexcept { return data_; }

  static constexpr std::size_t scope_id() noexcept { return 0; }

private:
  Data data_;
};

template <typename Data> class runtime_data_holder<Data, false> {
public:
  static constexpr std::size_t invalid_scope =
      std::numeric_limits<std::size_t>::max();

  // A registration container borrows the data of its root.
  template <typename Allocator> explicit runtime_data_holder(Allocator &) {}

  std::size_t scope_id() const noexcept { return scope_id_; }
  void scope_id(std::size_t value) noexcept { scope_id_ = value; }

private:
  std::size_t scope_id_ = invalid_scope;
};

template <typename Interface> struct key_value_interface {
  using type = normalized_type_t<Interface>;
};

template <> struct key_value_interface<void> {
  using type = void;
};

template <typename Interface>
using key_value_interface_t = typename key_value_interface<Interface>::type;

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

  template <typename Request, typename Result = Request>
  Result resolve_binding(runtime_selection selection, construction_scope scope,
                         runtime_context_type &context) {
    auto &binding = *selection.binding;
    if constexpr (detail::cache::supports_v<Request>) {
      return ::dingo::resolve_binding_request<Request, rtti_type>(
          scope, binding, context, detail::cache::key<Request>());
    } else {
      return ::dingo::resolve_binding_request<Request, rtti_type>(
          scope, binding, context);
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

  struct runtime_binding_value {
    runtime_binding_value(const runtime_binding_value &) = delete;
    runtime_binding_value &operator=(const runtime_binding_value &) = delete;
    runtime_binding_value(runtime_binding_value &&) = delete;
    runtime_binding_value &operator=(runtime_binding_value &&) = delete;

  protected:
    runtime_binding_value() = default;
    ~runtime_binding_value() = default;
  };

  template <std::size_t Index, typename Binding>
  struct runtime_binding_member : Binding {
    template <typename... Args>
    explicit runtime_binding_member(Args &&...args)
        : Binding(std::forward<Args>(args)...) {}
  };

  template <typename Indexes, typename... Bindings>
  struct runtime_binding_value_impl_base;

  // The bindings of one registration with several interfaces, one per
  // interface, sharing the state of the registration.
  template <std::size_t... Indexes, typename... Bindings>
  struct runtime_binding_value_impl_base<std::index_sequence<Indexes...>,
                                         Bindings...>
      : runtime_binding_value, runtime_binding_member<Indexes, Bindings>... {
    using container_type =
        typename type_list_head_t<type_list<Bindings...>>::container_type;

    // NOLINTBEGIN(bugprone-use-after-move)
    template <typename... Args>
    explicit runtime_binding_value_impl_base(Args &&...args)
        : runtime_binding_member<Indexes, Bindings>(
              std::forward<Args>(args)...)... {}
    // NOLINTEND(bugprone-use-after-move)

    // The binding of an interface.
    template <typename Interface> runtime_binding_interface_type *view() {
      using interface_type = typename annotated_traits<Interface>::type;
      static_assert(
          (std::is_same_v<typename Bindings::interface_type, interface_type> ||
           ...),
          "lookup interface is not present in runtime binding value");
      runtime_binding_interface_type *binding = nullptr;
      ((binding =
            binding == nullptr &&
                    std::is_same_v<typename Bindings::interface_type,
                                   interface_type>
                ? static_cast<runtime_binding_member<Indexes, Bindings> *>(this)
                : binding),
       ...);
      return binding;
    }

    container_type &get_container() {
      return static_cast<runtime_binding_member<
          0, type_list_head_t<type_list<Bindings...>>> &>(*this)
          .get_container();
    }
  };

  template <typename... Bindings>
  using runtime_binding_value_impl =
      runtime_binding_value_impl_base<std::index_sequence_for<Bindings...>,
                                      Bindings...>;

  template <typename LookupKeyDefinition> struct base_lookup_key_type;

  template <typename T>
  struct base_lookup_key_type<detail::lookup_key<key_type<T>>> {
    using type = T;
  };

  template <typename Interface, typename Key>
  static detail::base_lookup_key<rtti_type> base_typed_key_lookup_key() {
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

  // The key-value lookup entries of an interface.
  template <typename TypeInterface, typename Entries>
  struct runtime_lookup_entries_for_interface;

  template <typename TypeInterface, typename... Entries>
  struct runtime_lookup_entries_for_interface<TypeInterface,
                                              type_list<Entries...>> {
    using type = type_list_cat_t<std::conditional_t<
        runtime_lookup_entry_traits<Entries>::is_key_value &&
            std::is_same_v<
                normalized_type_t<TypeInterface>,
                typename runtime_lookup_entry_traits<Entries>::interface_type>,
        type_list<Entries>, type_list<>>...>;
  };

  template <typename TypeInterface>
  using runtime_lookup_entries_for_interface_t =
      typename runtime_lookup_entries_for_interface<
          TypeInterface, normalized_lookup_entries>::type;

  template <typename Arg>
  using runtime_registration_arg_traits =
      detail::runtime_registration_key_arg_traits<std::decay_t<Arg>>;

  template <typename Arg>
  using runtime_registration_arg_interface_t = detail::key_value_interface_t<
      typename runtime_registration_arg_traits<Arg>::interface_type>;

  // A key value supplies the key of a lookup entry when their keys are the
  // same: qualified, when it names the interface of the entry, or unqualified.
  template <typename Entry, typename Arg, bool Qualified>
  static constexpr bool runtime_registration_arg_keys_entry_v =
      std::is_same_v<typename runtime_lookup_entry_traits<Entry>::key_type,
                     typename runtime_registration_arg_traits<Arg>::key_type> &&
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
       (std::is_same_v<typename runtime_registration_arg_traits<Arg>::key_type,
                       typename runtime_registration_arg_traits<
                           KeyValueArgs>::key_type> &&
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
      : std::integral_constant<
            std::size_t,
            (std::size_t{0} + ... +
             runtime_lookup_entry_key_count<
                 runtime_lookup_entries_for_interface_t<Interfaces>,
                 Arg>::value)> {};

  template <typename InterfaceList, typename... KeyValueArgs>
  static constexpr void validate_supplied_runtime_registration_keys() {
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

  template <typename Entries, typename... KeyValueArgs>
  static constexpr void validate_required_runtime_registration_keys() {
    for_each(Entries{}, [](auto element) {
      using entry_type = typename decltype(element)::type;
      static_assert(
          (runtime_registration_arg_matches_entry_v<entry_type, KeyValueArgs> ||
           ...),
          "runtime-key lookup registration requires a runtime key value for "
          "each declared runtime-key lookup");
    });
  }

  // The index a lookup of an interface by a key goes to: the definition of the
  // key domain for the interface, or the base index for a static key without
  // one.
  template <typename Interface, typename Key> struct lookup_index_route {
  private:
    using key_definition = typename std::decay_t<Key>::definition_type;
    using explicit_entry =
        detail::selected_lookup_entry_t<normalized_type_t<Interface>,
                                        key_definition,
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
        return base_typed_key_lookup_key<Interface, Key>();
      } else {
        return detail::lookup_backend_key(key);
      }
    }
  };

  template <typename Value, typename Exact, bool AllowMiss, typename LookupKey>
  runtime_selection select_binding(runtime_bindings_state *state,
                                   LookupKey key) {
    static_assert(detail::is_lookup_key_v<LookupKey>);
    return state ? select_binding<Value, Exact, AllowMiss>(*state, key)
                 : runtime_selection::miss();
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
      return for_each_lookup<typename route::entry>(state, route::key(key),
                                                    std::forward<Fn>(fn));
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
      fn(results, ::dingo::resolve_binding_request<resolve_type, rtti_type>(
                      scope, entry.binding(), context));
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

  // Selection depends on the request only through its normalized value, its
  // exact type and the lookup key, so request spellings sharing them share one
  // chain instantiation.
  template <typename Value, typename Exact, bool AllowMiss, typename LookupKey>
  runtime_selection select_binding(runtime_bindings_state &state,
                                   LookupKey &key) {
    static_assert(detail::is_lookup_key_v<LookupKey>);
    if constexpr (!detail::is_static_lookup_key_definition_v<LookupKey>) {
      using lookup_route = lookup_index_route<Value, LookupKey>;
      if constexpr (!lookup_route::has_explicit_lookup &&
                    !std::is_same_v<Value, Exact>) {
        using exact_route = lookup_index_route<Exact, LookupKey>;
        if constexpr (exact_route::has_explicit_lookup) {
          return select_binding_at_interface<AllowMiss, LookupKey, Exact>(state,
                                                                          key);
        }
      }
      return select_binding_at_interface<AllowMiss, LookupKey, Value>(state,
                                                                      key);
    } else {
      auto selection =
          select_binding_at_interface<AllowMiss, LookupKey, Exact>(state, key);
      if constexpr (!std::is_same_v<Value, Exact>) {
        if (detail::selects_normalized_interface(selection.status)) {
          selection = select_binding_at_interface<AllowMiss, LookupKey, Value>(
              state, key);
        }
      }
      return selection;
    }
  }

  template <bool AllowMiss, typename LookupKey, typename Interface>
  // NOLINTNEXTLINE(readability-function-cognitive-complexity,readability-function-size)
  runtime_selection select_binding_at_interface(runtime_bindings_state &state,
                                                LookupKey &key) {
    using route = lookup_index_route<Interface, std::decay_t<LookupKey>>;
    using lookup_entry = typename route::entry;
    if constexpr (std::is_void_v<lookup_entry>) {
      if constexpr (AllowMiss) {
        return runtime_selection::miss();
      } else {
        static_assert(!std::is_void_v<lookup_entry>,
                      "keyed registration or request has no matching "
                      "dingo lookup definition for interface/key");
      }
    } else {
      return select_lookup<lookup_entry>(state, route::key(key));
    }
  }

  template <typename LookupEntry, typename Key>
  runtime_selection select_lookup(runtime_bindings_state &state,
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
  std::size_t for_each_lookup(runtime_bindings_state &state, const Key &key,
                              Fn &&fn) {
    auto &index = state.lookup_indexes.template get<LookupEntry>();
    return detail::lookup_for_each<LookupEntry>(index, key,
                                                std::forward<Fn>(fn));
  }

  // The index of the key value that supplies the key of an entry: the first
  // qualified one, else the first unqualified one.
  template <typename Entry, typename... Args>
  static constexpr std::size_t registration_key_value_index() {
    constexpr bool qualified[] = {
        runtime_registration_arg_keys_entry_v<Entry, Args, true>...};
    constexpr bool unqualified[] = {
        runtime_registration_arg_keys_entry_v<Entry, Args, false>...};
    for (std::size_t index = 0; index < sizeof...(Args); ++index) {
      if (qualified[index]) {
        return index;
      }
    }
    for (std::size_t index = 0; index < sizeof...(Args); ++index) {
      if (unqualified[index]) {
        return index;
      }
    }
    return sizeof...(Args);
  }

  template <typename Entry, typename... Args>
  static decltype(auto) registration_key_value(std::tuple<Args...> &keys) {
    constexpr std::size_t index =
        registration_key_value_index<Entry, std::decay_t<Args>...>();
    static_assert(index < sizeof...(Args),
                  "missing runtime key value for runtime-key lookup "
                  "registration");
    return std::get<index>(keys).value();
  }

  template <typename TypeInterface, typename TypeStorage, typename LookupKey,
            typename KeyArg>
  static void check_lookup_commit(lookup_commit result,
                                  const LookupKey &lookup_key,
                                  const KeyArg &key_arg) {
    if (result == lookup_commit::rejected) {
      throw detail::make_lookup_already_registered_exception<
          TypeInterface, typename TypeStorage::type>(lookup_key, key_arg);
    }
    if (result == lookup_commit::sealed) {
      throw detail::make_container_sealed_exception<TypeInterface,
                                                    typename TypeStorage::type>(
          lookup_key, key_arg);
    }
  }

  template <typename TypeInterface, typename TypeStorage, typename LookupKey,
            typename LookupEntry, typename ValueFactory, typename KeyArg>
  void insert_lookup(runtime_bindings_state &state, ValueFactory &value_factory,
                     runtime_transaction_type &transaction,
                     const LookupKey &lookup_key, const KeyArg &key_arg) {
    auto &index = state.lookup_indexes.template get<LookupEntry>();

    if constexpr (std::is_same_v<
                      typename lookup_entry_cardinality<LookupEntry>::type,
                      ::dingo::one>) {
      auto value = value_factory.template make<TypeInterface>();
      auto [handle, inserted] = index.try_emplace(key_arg, std::move(value));
      if (inserted) {
        record_lookup_rollback<LookupEntry>(transaction, state, handle,
                                            key_arg);
      } else {
        check_lookup_commit<TypeInterface, TypeStorage>(
            replace_lookup<LookupEntry>(transaction, state, handle, key_arg,
                                        std::move(value)),
            lookup_key, key_arg);
      }
    } else {
      auto handle =
          index.emplace(key_arg, value_factory.template make<TypeInterface>());
      record_lookup_rollback<LookupEntry>(transaction, state, handle, key_arg);
    }
  }

  template <typename TypeInterface, typename TypeStorage, typename LookupKey,
            typename ValueFactory, typename KeyResolver, typename... Entries>
  void commit_lookup(runtime_bindings_state &state, ValueFactory &value_factory,
                     runtime_transaction_type &transaction,
                     KeyResolver &&key_resolver, const LookupKey &lookup_key,
                     type_list<Entries...>) {
    static_assert(ValueFactory::shared_owner || sizeof...(Entries) <= 1,
                  "unique runtime binding owner can only be inserted into one "
                  "lookup");
    (insert_lookup<TypeInterface, TypeStorage, LookupKey, Entries>(
         state, value_factory, transaction, lookup_key,
         key_resolver(type_list_iterator<Entries>{})),
     ...);
  }

  template <typename LookupEntry, typename Key, typename Binding>
  static void erase_lookup_binding(runtime_bindings_state &state,
                                   const Key &key, Binding *binding) noexcept {
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
  static void record_lookup_rollback(runtime_transaction_type &transaction,
                                     runtime_bindings_state &state,
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

  // Redirects an occupied singular lookup to a new binding when the container
  // traits allow it and no resolve has sealed the container. The replaced
  // binding stays owned by the container; a rollback points the lookup back.
  template <typename LookupEntry, typename Handle, typename Key>
  lookup_commit replace_lookup(runtime_transaction_type &transaction,
                               runtime_bindings_state &state, Handle handle,
                               const Key &key, runtime_lookup_value &&value) {
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
      transaction.on_rollback([&state, key = std::decay_t<Key>(key), previous,
                               replacement]() noexcept {
        restore_lookup_binding<LookupEntry>(state, key, replacement, previous);
      });
      existing = std::move(value);
      return lookup_commit::replaced;
    }
  }

  template <typename LookupEntry, typename Key, typename Binding>
  static void restore_lookup_binding(runtime_bindings_state &state,
                                     const Key &key, Binding *replacement,
                                     Binding *previous) noexcept {
    auto &index = state.lookup_indexes.template get<LookupEntry>();
    auto it = index.find(key);
    assert(it != index.end());
    auto &value = detail::lookup_iterator_value(it);
    assert(std::addressof(value.binding()) == replacement);
    (void)replacement;
    value = runtime_lookup_value(previous);
  }

  template <typename Owner> struct shared_lookup_value_factory {
    static constexpr bool shared_owner = true;
    Owner *owner;

    template <typename TypeInterface> runtime_lookup_value make() {
      return runtime_lookup_value(owner->template view<TypeInterface>());
    }
  };

  template <bool SharedOwner, typename Binding>
  struct direct_lookup_value_factory {
    static constexpr bool shared_owner = SharedOwner;
    Binding *binding;

    template <typename TypeInterface> runtime_lookup_value make() {
      static_assert(
          std::is_same_v<typename Binding::interface_type,
                         typename annotated_traits<TypeInterface>::type>);
      return runtime_lookup_value(binding);
    }
  };

  // Commits a binding to the lookups of its interface: to each key-value
  // lookup with its supplied key value, or else to the routed lookup of its
  // static key.
  template <typename TypeInterface, typename TypeStorage, typename LookupKey,
            typename ValueFactory, typename... KeyValueArgs>
  void commit_registration(runtime_bindings_state &state,
                           ValueFactory &value_factory,
                           runtime_transaction_type &transaction,
                           LookupKey lookup_key, KeyValueArgs &&...key_values) {
    detail::interface_registration_requirements<
        TypeStorage, typename annotated_traits<TypeInterface>::type,
        typename TypeStorage::type>::assert_valid();
    using lookup_entries =
        runtime_lookup_entries_for_interface_t<TypeInterface>;
    if constexpr (sizeof...(KeyValueArgs) > 0 &&
                  type_list_size_v<lookup_entries> > 0) {
      validate_required_runtime_registration_keys<lookup_entries,
                                                  KeyValueArgs...>();
      auto key_value_tuple = std::forward_as_tuple(key_values...);
      auto key_resolver = [&](auto element) -> decltype(auto) {
        using entry_type = typename decltype(element)::type;
        return registration_key_value<entry_type>(key_value_tuple);
      };
      commit_lookup<TypeInterface, TypeStorage, LookupKey>(
          state, value_factory, transaction, key_resolver, lookup_key,
          lookup_entries{});
    } else {
      ((void)key_values, ...);
      using route = lookup_index_route<TypeInterface, LookupKey>;
      auto key_resolver = [&](auto) { return route::key(lookup_key); };
      commit_lookup<TypeInterface, TypeStorage, LookupKey>(
          state, value_factory, transaction, key_resolver, lookup_key,
          type_list<typename route::entry>{});
    }
  }
};

} // namespace detail

template <typename ContainerTraits, typename Allocator, typename ParentRegistry,
          typename ResolveRoot>
class DINGO_EMPTY_BASES runtime_registry
    : public allocator_base<Allocator>,
      public detail::runtime_registration_api<runtime_registry<
          ContainerTraits, Allocator, ParentRegistry, ResolveRoot>>,
      protected detail::registry_store<ContainerTraits, Allocator> {
  template <typename> friend class ::dingo::resolution_context;
  template <typename> friend class detail::runtime_registration_api;
  template <typename, typename, typename, typename>
  friend class runtime_binding_state;
  template <typename, typename, typename>
  friend class detail::runtime_container_impl;
  template <typename, typename, typename, typename>
  friend class runtime_registry;
  using store_type = detail::registry_store<ContainerTraits, Allocator>;
  using registry_type =
      runtime_registry<ContainerTraits, Allocator, ParentRegistry, ResolveRoot>;
  // The root registry owns the runtime data; a registration container borrows
  // that of its root.
  static constexpr bool OwnsRuntimeData = std::is_void_v<ParentRegistry>;
  using resolve_root_type = ResolveRoot;
  using container_type = resolve_root_type;
  using runtime_type = container_runtime<Allocator>;
  using typename store_type::runtime_binding_interface_type;
  using typename store_type::runtime_context_type;
  using typename store_type::runtime_selection;
  using typename store_type::runtime_transaction_type;
  template <typename Parent>
  using registration_container_type =
      runtime_registry<ContainerTraits, Allocator, Parent, resolve_root_type>;

public:
  using container_traits_type = ContainerTraits;
  using allocator_type = Allocator;
  using parent_container_type =
      std::conditional_t<std::is_same_v<void, ParentRegistry>,
                         resolve_root_type, ParentRegistry>;
  using rtti_type = typename ContainerTraits::rtti_type;
  using lookup_definition_type =
      detail::container_lookup_definition_type_t<ContainerTraits>;

protected:
  using typename store_type::runtime_bindings_state;
  using typename store_type::runtime_data_type;
  using typename store_type::runtime_scope_state;
  template <typename TypeInterface>
  using runtime_lookup_entries_for_interface_t =
      typename store_type::template runtime_lookup_entries_for_interface_t<
          TypeInterface>;
  template <typename Interface, typename Key>
  using lookup_index_route =
      typename store_type::template lookup_index_route<Interface, Key>;
  template <typename... Bindings>
  using runtime_binding_value_impl =
      typename store_type::template runtime_binding_value_impl<Bindings...>;
  template <typename Owner>
  using shared_lookup_value_factory =
      typename store_type::template shared_lookup_value_factory<Owner>;
  template <bool SharedOwner, typename Binding>
  using direct_lookup_value_factory =
      typename store_type::template direct_lookup_value_factory<SharedOwner,
                                                                Binding>;

private:
  static runtime_data_type &borrow_runtime_data(resolve_root_type *root) {
    auto &data = root->binding_store().shared_runtime_data();
    static_assert(
        std::is_same_v<std::remove_reference_t<decltype(data)>,
                       runtime_data_type>,
        "registration container traits must preserve the root runtime lookup "
        "and allocator configuration");
    return data;
  }

public:
  runtime_registry(parent_container_type *parent,
                   const allocator_type &alloc = allocator_type())
      : allocator_base<allocator_type>(alloc), parent_(parent),
        runtime_data_(get_allocator()) {}

private:
  registry_type &binding_store() { return *this; }

  registry_type &runtime_registration_parent() { return *this; }

public:
  runtime_registry(const runtime_registry &) = delete;
  runtime_registry &operator=(const runtime_registry &) = delete;

  ~runtime_registry() = default;

  allocator_type &get_allocator() {
    return allocator_base<allocator_type>::get_allocator();
  }

protected:
  container_runtime<allocator_type> &runtime() {
    return shared_runtime_data().runtime;
  }

  runtime_data_type &shared_runtime_data() {
    if constexpr (OwnsRuntimeData) {
      return runtime_data_.get();
    } else {
      return borrow_runtime_data(resolve_root());
    }
  }

  runtime_bindings_state *runtime_bindings() {
    if constexpr (OwnsRuntimeData) {
      return std::addressof(shared_runtime_data().bindings);
    } else {
      auto *scope = runtime_scope();
      return scope != nullptr ? std::addressof(scope->bindings) : nullptr;
    }
  }

  runtime_scope_state *runtime_scope() {
    static_assert(!OwnsRuntimeData);
    const auto scope = runtime_data_.scope_id();
    if (scope == decltype(runtime_data_)::invalid_scope) {
      return nullptr;
    }
    return shared_runtime_data().find_scope(scope);
  }

  void release_runtime_scope(runtime_scope_state *state) noexcept {
    shared_runtime_data().release_scope(runtime_data_.scope_id(), state);
    runtime_data_.scope_id(decltype(runtime_data_)::invalid_scope);
  }

  template <typename Parent>
  runtime_bindings_state &
  ensure_runtime_bindings(runtime_transaction_type &transaction,
                          Parent *registration_parent) {
    if constexpr (OwnsRuntimeData) {
      return *runtime_bindings();
    } else {
      if (auto *scope = runtime_scope()) {
        assert(scope->registration_parent == registration_parent);
        return scope->bindings;
      }

      auto &data = shared_runtime_data();
      auto *state =
          transaction.template construct_persistent<runtime_scope_state>(
              get_allocator(), registration_parent);
      runtime_data_.scope_id(data.create_scope(state));
      try {
        transaction.on_rollback(
            +[](void *self, void *scope) noexcept {
              static_cast<registry_type *>(self)->release_runtime_scope(
                  static_cast<runtime_scope_state *>(scope));
            },
            this, state);
      } catch (...) {
        release_runtime_scope(state);
        throw;
      }
      return state->bindings;
    }
  }

  template <typename Parent> Parent *runtime_registration_parent() {
    if constexpr (std::is_same_v<Parent, parent_container_type>) {
      return parent();
    } else {
      static_assert(!OwnsRuntimeData);
      auto *scope = runtime_scope();
      assert(scope != nullptr && scope->registration_parent != nullptr);
      return static_cast<Parent *>(scope->registration_parent);
    }
  }

  parent_container_type *parent() { return parent_; }

  // The root registry is the base of its container; a registration container
  // reaches the root through its parents.
  resolve_root_type *resolve_root() {
    if constexpr (std::is_base_of_v<registry_type, resolve_root_type>) {
      return static_cast<resolve_root_type *>(this);
    } else if constexpr (std::is_same_v<parent_container_type,
                                        resolve_root_type>) {
      return parent_;
    } else {
      assert(parent_ != nullptr);
      return parent_->binding_store().resolve_root();
    }
  }

protected:
  // Lookup misses are answered by the parent chain instead of failing at
  // compile time when the registry is not the resolve root.
  static constexpr bool lookup_may_miss() {
    return !std::is_void_v<ParentRegistry>;
  }

  template <typename Value, typename Exact, typename LookupKey,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  runtime_selection select_binding(LookupKey key) {
    return store_type::template select_binding<Value, Exact, lookup_may_miss()>(
        runtime_bindings(), std::move(key));
  }

  template <typename T, typename Fn, typename LookupKey,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  T construct_collection(construction_scope scope,
                         runtime_context_type &context, Fn &&fn,
                         LookupKey key) {
    using collection_type = collection_traits<T>;
    static_assert(collection_type::is_collection,
                  "missing collection_traits specialization for type T");
    const std::size_t count = count_collection<T>(key);
    // A declared collection lookup with no rows is a valid empty collection.
    if (count == 0 &&
        !lookup_index_route<typename collection_type::resolve_type,
                            LookupKey>::has_explicit_collection_lookup) {
      throw detail::make_collection_type_not_found_exception<
          T, typename collection_type::resolve_type>();
    }
    T results;
    collection_type::reserve(results, count);
    append_collection(scope, results, context, std::forward<Fn>(fn),
                      std::move(key));
    return results;
  }

  template <typename T, typename LookupKey,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  std::size_t count_collection(LookupKey key) {
    return store_type::template count_collection<T>(runtime_bindings(),
                                                    std::move(key));
  }

  template <typename T, typename Fn, typename LookupKey,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  std::size_t append_collection(construction_scope scope, T &results,
                                runtime_context_type &context, Fn &&fn,
                                LookupKey key) {
    return store_type::append_runtime_collection(
        runtime_bindings(), results, scope, context, std::forward<Fn>(fn),
        std::move(key));
  }

  template <typename Source> class container_proxy {
  public:
    using container_type = typename Source::container_type;

    explicit container_proxy(Source *source) : source_(source) {}

    container_type &get() const { return source_->get_container(); }
    container_type *operator->() const { return std::addressof(get()); }
    operator container_type &() const { return get(); }

  private:
    Source *source_;
  };

protected:
  template <typename Request, bool MayAutoConstruct, typename LookupKey,
            typename R = typename Request::lookup_type,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  R unresolved(construction_scope scope, runtime_context_type &context,
               const LookupKey &key) {
    return detail::request_resolution::unresolved<Request, MayAutoConstruct, R>(
        *this, *resolve_root(), scope, context, key);
  }

public:
  template <typename TypeInterface, typename TypeStorage, typename BindingState>
  using runtime_registration_binding_t =
      runtime_binding<container_type,
                      typename annotated_traits<TypeInterface>::type,
                      TypeStorage, BindingState>;

  template <typename InterfaceList, typename TypeStorage, typename BindingState>
  struct runtime_binding_value_owner;

  template <typename... TypeInterfaces, typename TypeStorage,
            typename BindingState>
  struct runtime_binding_value_owner<type_list<TypeInterfaces...>, TypeStorage,
                                     BindingState> {
    using type = runtime_binding_value_impl<runtime_registration_binding_t<
        TypeInterfaces, TypeStorage, BindingState>...>;
  };

  template <typename TypeInterface, typename TypeStorage, typename BindingState>
  struct runtime_binding_value_owner<type_list<TypeInterface>, TypeStorage,
                                     BindingState> {
    using type = runtime_registration_binding_t<TypeInterface, TypeStorage,
                                                BindingState>;
  };

  template <typename InterfaceList, typename TypeStorage, typename BindingState>
  using runtime_binding_value_owner_t =
      typename runtime_binding_value_owner<InterfaceList, TypeStorage,
                                           BindingState>::type;

private:
  template <typename... TypeArgs, typename Parent, typename Arg,
            typename... KeyValueArgs>
  // Registration is cold; keep its type-specific setup out of callers.
  // NOLINTNEXTLINE(readability-function-cognitive-complexity,readability-function-size)
  DINGO_NOINLINE auto prepare_binding(Parent *parent, Arg &&arg,
                                      KeyValueArgs &&...key_values) {
    static_assert(!detail::has_explicit_void_interface_v<TypeArgs...>,
                  "interfaces<void> is not a valid registration target");
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
    using instance_container_type = registration_container_type<Parent>;
    (void)std::addressof(arg);
    using interface_types = typename binding_model::interface_types;
    static constexpr bool storage_tag_is_complete =
        binding_model::storage_tag_is_complete;
    static_assert(storage_tag_is_complete,
                  "registered storage tag must be complete and specialize "
                  "dingo::scope_traits; include the corresponding "
                  "dingo/storage header");
    if constexpr (!storage_tag_is_complete) {
      return store_type::template invalid_registration_return<
          instance_container_type>();
    } else {
      using storage_type = typename binding_model::storage_type;
      using registration_requirements = typename binding_model::requirements;
      using registration_key = typename binding_model::key_type;

      registration_requirements::assert_valid();

      using runtime_binding_state_type =
          runtime_binding_state<registry_type, instance_container_type,
                                detail::binding_cell_t<binding_model>, Parent>;

      if constexpr (!registration_requirements::valid) {
        return store_type::template invalid_registration_return<
            instance_container_type>();
      } else {
        if constexpr (sizeof...(KeyValueArgs) > 0) {
          store_type::template validate_supplied_runtime_registration_keys<
              interface_types, KeyValueArgs...>();
        }
        // One interface is bound by the binding itself; several share the
        // state of the registration through one binding per interface.
        constexpr bool single = type_list_size_v<interface_types> == 1;
        using owner_type = runtime_binding_value_owner_t<
            interface_types, storage_type,
            std::conditional_t<single, runtime_binding_state_type,
                               runtime_binding_state_type *>>;
        static constexpr bool shared_owner =
            !single || (sizeof...(KeyValueArgs) > 0 &&
                        type_list_size_v<runtime_lookup_entries_for_interface_t<
                                type_list_head_t<interface_types>>> > 1);
        return commit_binding<shared_owner, interface_types, storage_type,
                              owner_type, runtime_binding_state_type>(
            parent, registration_key{}, std::forward<Arg>(arg),
            std::forward<KeyValueArgs>(key_values)...);
      }
    }
  }

  // Constructs a binding in the transaction, from the registered factory
  // argument when there is one.
  template <typename T, typename Arg>
  T *construct_binding(runtime_transaction_type &transaction, Arg &&arg) {
    if constexpr (is_none_v<std::decay_t<Arg>>) {
      (void)arg;
      return transaction.template construct_persistent<T>(this);
    } else {
      return transaction.template construct_persistent<T>(
          this, std::forward<Arg>(arg));
    }
  }

  // Constructs the owner of a registration and commits it to the lookups of
  // its interfaces in one transaction.
  template <bool SharedOwner, typename Interfaces, typename TypeStorage,
            typename Owner, typename State, typename Parent, typename LookupKey,
            typename Arg, typename... KeyValueArgs>
  DINGO_NOINLINE container_proxy<Owner>
  commit_binding(Parent *parent, LookupKey lookup_key, Arg &&arg,
                 KeyValueArgs &&...key_values) {
    inline_arena<DINGO_CONTEXT_ARENA_BUFFER_SIZE> scratch;
    runtime_transaction_type transaction(runtime(), scratch);
    auto &state = ensure_runtime_bindings(transaction, parent);
    auto commit = [&](auto &value_factory) {
      for_each(Interfaces{}, [&](auto element) {
        using interface_type = typename decltype(element)::type;
        this->template commit_registration<interface_type, TypeStorage>(
            state, value_factory, transaction, lookup_key, key_values...);
      });
    };
    Owner *owner = nullptr;
    if constexpr (type_list_size_v<Interfaces> == 1) {
      owner = construct_binding<Owner>(transaction, std::forward<Arg>(arg));
      direct_lookup_value_factory<SharedOwner, Owner> value_factory{owner};
      commit(value_factory);
    } else {
      auto *data =
          construct_binding<State>(transaction, std::forward<Arg>(arg));
      owner = transaction.template construct_persistent<Owner>(data);
      shared_lookup_value_factory<Owner> value_factory{owner};
      commit(value_factory);
    }
    transaction.commit();
    return container_proxy<Owner>(owner);
  }

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4702)
#endif
  friend struct detail::request_resolution;

  template <typename Request, typename LookupKey>
  runtime_selection select_request(const LookupKey &key) {
    static_assert(!std::is_const_v<typename Request::value_type>);
    return select_binding<typename Request::value_type,
                          typename Request::exact_type>(key);
  }

  template <typename Request, typename R>
  R resolve_selected(runtime_selection selection, construction_scope scope,
                     runtime_context_type &context) {
    return store_type::template resolve_binding<
        typename Request::interface_type, R>(selection, scope, context);
  }

  template <typename Request>
  [[noreturn]] void ambiguous(runtime_context_type &context) {
    throw detail::make_type_ambiguous_exception<typename Request::lookup_type>(
        context);
  }

  // A registration container answers its misses through its parent; the
  // root registry has none.
  template <typename Lookup, typename LookupKey>
  static constexpr bool parent_serves() {
    return lookup_may_miss();
  }

  template <typename R, typename LookupKey>
  R construct_request_collection(construction_scope scope,
                                 runtime_context_type &context, LookupKey key) {
    return construct_collection<R>(
        scope, context, detail::binding_collection_append{}, std::move(key));
  }

  template <typename T, bool RemoveRvalueReferences, typename LookupKey,
            typename R =
                typename request_type<T, RemoveRvalueReferences>::lookup_type,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  R resolve(construction_scope scope, runtime_context_type &context,
            LookupKey key) {
    return resolve<T, RemoveRvalueReferences>(scope, context, *this,
                                              std::move(key));
  }

  template <typename T, bool RemoveRvalueReferences, typename Origin,
            typename LookupKey,
            typename R =
                typename request_type<T, RemoveRvalueReferences>::lookup_type,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  R resolve(construction_scope scope, runtime_context_type &context,
            Origin &origin, LookupKey key) {
    using request = request_type<T, RemoveRvalueReferences>;
    return detail::request_resolution::resolve<
        request, typename request::interface_type>(*this, scope, context,
                                                   origin, std::move(key));
  }

#ifdef _MSC_VER
#pragma warning(pop)
#endif

  parent_container_type *parent_ = nullptr;

  detail::runtime_data_holder<runtime_data_type, OwnsRuntimeData> runtime_data_;
};

} // namespace dingo
