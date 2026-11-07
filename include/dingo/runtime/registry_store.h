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
#include <dingo/binding/request_resolution.h>
#include <dingo/factory/callable.h>
#include <dingo/lookup/lookup.h>
#include <dingo/lookup/operations.h>
#include <dingo/memory/allocator.h>
#include <dingo/registration/requirements.h>
#include <dingo/runtime/binding.h>
#include <dingo/runtime/container_traits.h>
#include <dingo/runtime/context.h>
#include <dingo/runtime/lookup_index.h>
#include <dingo/runtime/memory.h>
#include <dingo/runtime/transaction.h>
#include <dingo/storage/interface_storage_traits.h>
#include <dingo/type/annotated.h>
#include <dingo/type/auto_constructible.h>
#include <dingo/type/collection_traits.h>
#include <dingo/type/key.h>
#include <dingo/type/request.h>
#include <dingo/type/shape.h>

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

  runtime_memory<Allocator> runtime;
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
          Request, typename std::decay_t<LookupKey>::definition_type,
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
      using interface_type = unannotated_t<Interface>;
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

  // The key-value lookup entries of an interface of a registration: those
  // defined for the interface itself, or else those of its normalized
  // interface, unless the registration binds that one too. An array and its
  // element are two interfaces, each with its own lookup.
  template <typename TypeInterface, typename Interfaces, typename Entries>
  struct runtime_lookup_entries_for_interface;

  template <typename TypeInterface, typename... Interfaces, typename... Entries>
  struct runtime_lookup_entries_for_interface<
      TypeInterface, type_list<Interfaces...>, type_list<Entries...>> {
  private:
    template <typename Interface>
    using entries_of = type_list_cat_t<std::conditional_t<
        runtime_lookup_entry_traits<Entries>::is_key_value &&
            std::is_same_v<Interface, typename runtime_lookup_entry_traits<
                                          Entries>::interface_type>,
        type_list<Entries>, type_list<>>...>;
    using exact_interface = std::remove_cv_t<TypeInterface>;
    using normalized_interface = normalized_type_t<TypeInterface>;
    using exact = entries_of<exact_interface>;
    static constexpr bool binds_normalized =
        !std::is_same_v<exact_interface, normalized_interface> &&
        (std::is_same_v<std::remove_cv_t<Interfaces>, normalized_interface> ||
         ...);

  public:
    using type =
        std::conditional_t<type_list_size_v<exact> != 0 || binds_normalized,
                           exact, entries_of<normalized_interface>>;
    // Whether the lookups of the interface are those of its normalized
    // interface, which the registration commits under that one.
    static constexpr bool defers_to_normalized =
        type_list_size_v<exact> == 0 && binds_normalized &&
        type_list_size_v<entries_of<normalized_interface>> != 0;
  };

  template <typename TypeInterface, typename Interfaces>
  using runtime_lookup_entries_for_interface_t =
      typename runtime_lookup_entries_for_interface<
          TypeInterface, Interfaces, normalized_lookup_entries>::type;

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
                                    runtime_lookup_entries_for_interface_t<
                                        Interfaces, type_list<Interfaces...>>,
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
        return base_typed_key_lookup_key<Interface, Key>();
      } else {
        return key.backend_key();
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
      // The lookup of the exact interface, when it has one of its own, and
      // else that of the normalized one, as a static container selects.
      using lookup_route = lookup_index_route<Value, LookupKey>;
      using exact_route = lookup_index_route<Exact, LookupKey>;
      if constexpr (!std::is_same_v<Value, Exact> &&
                    exact_route::has_explicit_lookup &&
                    !std::is_same_v<typename exact_route::entry,
                                    typename lookup_route::entry>) {
        auto selection =
            select_binding_at_interface<AllowMiss, LookupKey, Exact>(state,
                                                                     key);
        if constexpr (lookup_route::has_explicit_lookup) {
          if (!detail::selects_normalized_interface(selection.status)) {
            return selection;
          }
        } else {
          return selection;
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
      static_assert(std::is_same_v<typename Binding::interface_type,
                                   unannotated_t<TypeInterface>>);
      return runtime_lookup_value(binding);
    }
  };

  // Commits a binding to the lookups of its interface: to each key-value
  // lookup with its supplied key value, or else to the routed lookup of its
  // static key.
  template <typename TypeInterface, typename TypeStorage, typename Interfaces,
            typename LookupKey, typename ValueFactory, typename... KeyValueArgs>
  void commit_registration(runtime_bindings_state &state,
                           ValueFactory &value_factory,
                           runtime_transaction_type &transaction,
                           LookupKey lookup_key, KeyValueArgs &&...key_values) {
    detail::interface_registration_requirements<
        TypeStorage, unannotated_t<TypeInterface>,
        typename TypeStorage::type>::assert_valid();
    using lookup_entries =
        runtime_lookup_entries_for_interface_t<TypeInterface, Interfaces>;
    if constexpr (sizeof...(KeyValueArgs) > 0 &&
                  runtime_lookup_entries_for_interface<
                      TypeInterface, Interfaces,
                      normalized_lookup_entries>::defers_to_normalized) {
      // A keyed request of the interface selects the lookup of its normalized
      // interface, which the registration commits under that one.
      (void)state;
      (void)value_factory;
      (void)transaction;
      (void)lookup_key;
      ((void)key_values, ...);
    } else if constexpr (sizeof...(KeyValueArgs) > 0 &&
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
} // namespace dingo
