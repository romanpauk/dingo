//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

// The runtime lookup selection: the index types the traits name, the route of
// a request to the entry that serves it, and the selection of singular and
// collection candidates. Registration and execution stay in registry.h.

// The part of a runtime registry that selects lookup candidates: the lookup
// index types of its traits, the route of a request to the entry that serves
// it, the selection of singular and collection candidates, and the values the
// indexes hold. Runtime registration and the execution of a selected binding
// stay in registry.h.

#include <dingo/base/none.h>
#include <dingo/base/type_list.h>
#include <dingo/binding/binding_selection.h>
#include <dingo/binding/runtime_binding_interface.h>
#include <dingo/lookup/container_lookup_definition.h>
#include <dingo/lookup/lookup.h>
#include <dingo/lookup/operations.h>
#include <dingo/runtime/lookup_index.h>
#include <dingo/type/collection_traits.h>
#include <dingo/type/key.h>
#include <dingo/type/normalized_type.h>

#include <type_traits>
#include <utility>

namespace dingo {
namespace detail {

template <typename BindingInterface> struct runtime_lookup_binding_view {
  BindingInterface *binding = nullptr;
};

template <typename BindingInterface> class runtime_lookup_value {
public:
  using binding_view_type = runtime_lookup_binding_view<BindingInterface>;

  explicit runtime_lookup_value(binding_view_type view) : view_(view) {}

  runtime_lookup_value(runtime_lookup_value &&) noexcept = default;
  runtime_lookup_value &operator=(runtime_lookup_value &&) noexcept = default;
  runtime_lookup_value(const runtime_lookup_value &) = delete;
  runtime_lookup_value &operator=(const runtime_lookup_value &) = delete;

  BindingInterface &binding() { return *view_.binding; }

private:
  binding_view_type view_;
};

template <typename LookupEntries, typename BindingInterface, typename Allocator>
struct runtime_bindings_state {
  using value_type = runtime_lookup_value<BindingInterface>;

  explicit runtime_bindings_state(Allocator &allocator)
      : lookup_indexes(allocator) {}

  lookup_index<LookupEntries, value_type, Allocator> lookup_indexes;
};

// The selection half of a runtime registry: the lookup index types its
// container traits name, the route of a request to the index entry that serves
// it, and the selection of singular and collection candidates. It owns no
// data; it reads the lookup state its registry hands it, so registration and
// execution stay with the registry.
template <typename ContainerTraits, typename Allocator> class lookup_selection {
protected:
  using allocator_type = Allocator;
  using rtti_type = typename ContainerTraits::rtti_type;
  using lookup_definition_type =
      detail::container_lookup_definition_type_t<ContainerTraits>;
  using runtime_context_type = runtime_context<Allocator>;

  using normalized_lookup_entries =
      detail::normalize_lookup_definitions_t<lookup_definition_type>;
  using base_lookup_definition =
      detail::base_lookup_definition_t<lookup_definition_type>;
  using base_lookup_entry =
      detail::base_lookup_entry<rtti_type,
                                typename base_lookup_definition::cardinality,
                                typename base_lookup_definition::backend_type>;
  using base_lookup_cardinality = typename base_lookup_definition::cardinality;
  using lookup_index_entries = type_list_unique_t<
      type_list_cat_t<normalized_lookup_entries, type_list<base_lookup_entry>>>;
  template <typename LookupEntry>
  using lookup_entry_cardinality =
      detail::lookup_entry_cardinality<LookupEntry>;

  using runtime_binding_interface_type =
      runtime_binding_interface<typename ContainerTraits::rtti_type,
                                runtime_context_type>;
  using runtime_selection =
      detail::runtime_binding_selection<runtime_binding_interface_type>;

  using runtime_lookup_binding_view =
      detail::runtime_lookup_binding_view<runtime_binding_interface_type>;
  using runtime_lookup_value =
      detail::runtime_lookup_value<runtime_binding_interface_type>;
  using runtime_bindings_state = detail::runtime_bindings_state<
      lookup_index_entries, runtime_binding_interface_type, allocator_type>;

  template <typename LookupKeyDefinition> struct base_lookup_key_type;

  template <typename T>
  struct base_lookup_key_type<detail::lookup_key<key_type<T>>> {
    using type = T;
  };

  template <typename T, typename Interface>
  struct base_lookup_key_type<detail::lookup_key<key_value<T, Interface>>> {
    using type = T;
  };

  template <typename Interface, typename Key>
  static detail::base_lookup_key<rtti_type> base_typed_key_lookup_key() {
    using key_definition = typename std::decay_t<Key>::definition_type;
    using key_type = typename base_lookup_key_type<key_definition>::type;
    return {rtti_type::template get_type_index<Interface>(),
            rtti_type::template get_type_index<key_type>()};
  }

  template <typename Interface, typename KeyDomain, typename Cardinality>
  using static_key_lookup_entry_t =
      detail::selected_lookup_entry_with_cardinality_t<
          Interface, KeyDomain, Cardinality, normalized_lookup_entries>;

  template <typename Interface, typename KeyDomain>
  using static_key_lookup_cardinality_t = std::conditional_t<
      std::is_void_v<
          static_key_lookup_entry_t<Interface, KeyDomain, ::dingo::one>> &&
          !std::is_void_v<
              static_key_lookup_entry_t<Interface, KeyDomain, ::dingo::many>>,
      ::dingo::many,
      std::conditional_t<!std::is_void_v<static_key_lookup_entry_t<
                             Interface, KeyDomain, ::dingo::one>>,
                         ::dingo::one, base_lookup_cardinality>>;

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

  template <typename TypeInterface, typename Entries>
  struct runtime_lookup_entries_for_interface;

  template <typename TypeInterface>
  struct runtime_lookup_entries_for_interface<TypeInterface, type_list<>> {
    using type = type_list<>;
  };

  template <typename TypeInterface, typename Head, typename... Tail>
  struct runtime_lookup_entries_for_interface<TypeInterface,
                                              type_list<Head, Tail...>> {
  private:
    using head_traits = runtime_lookup_entry_traits<Head>;
    using tail_type =
        typename runtime_lookup_entries_for_interface<TypeInterface,
                                                      type_list<Tail...>>::type;
    static constexpr bool matches =
        head_traits::is_key_value &&
        std::is_same_v<normalized_type_t<TypeInterface>,
                       typename head_traits::interface_type>;

  public:
    using type =
        std::conditional_t<matches, type_list_cat_t<type_list<Head>, tail_type>,
                           tail_type>;
  };

  template <typename TypeInterface>
  using runtime_lookup_entries_for_interface_t =
      typename runtime_lookup_entries_for_interface<
          TypeInterface, normalized_lookup_entries>::type;

  template <typename Interface, typename KeyDefinition, typename = void>
  struct lookup_index_route_impl;

  template <typename Interface, typename KeyDefinition>
  struct lookup_index_route_impl<
      Interface, KeyDefinition,
      std::enable_if_t<
          detail::is_static_lookup_key_definition_v<KeyDefinition>>> {
    using interface_type = Interface;
    using normalized_interface = normalized_type_t<Interface>;
    using key_domain = KeyDefinition;
    using one_lookup_entry =
        static_key_lookup_entry_t<normalized_interface, key_domain,
                                  ::dingo::one>;
    using many_lookup_entry =
        static_key_lookup_entry_t<normalized_interface, key_domain,
                                  ::dingo::many>;
    using selected_cardinality =
        static_key_lookup_cardinality_t<normalized_interface, key_domain>;
    using lookup_entry = std::conditional_t<
        std::is_same_v<selected_cardinality, ::dingo::one> &&
            !std::is_void_v<one_lookup_entry>,
        one_lookup_entry,
        std::conditional_t<
            std::is_same_v<selected_cardinality, ::dingo::many> &&
                !std::is_void_v<many_lookup_entry>,
            many_lookup_entry, base_lookup_entry>>;
    static constexpr bool has_explicit_lookup = false;
    static constexpr bool has_explicit_collection_lookup =
        std::is_same_v<selected_cardinality, ::dingo::many> &&
        !std::is_void_v<many_lookup_entry>;
  };

  template <typename Interface, typename KeyDefinition>
  struct lookup_index_route_impl<
      Interface, KeyDefinition,
      std::enable_if_t<
          !detail::is_static_lookup_key_definition_v<KeyDefinition>>> {
    using normalized_interface = normalized_type_t<Interface>;
    using key_domain = KeyDefinition;
    using lookup_entry =
        detail::selected_lookup_entry_t<normalized_interface, key_domain,
                                        normalized_lookup_entries>;
    using selected_cardinality =
        typename lookup_entry_cardinality<lookup_entry>::type;
    static constexpr bool has_explicit_lookup = !std::is_void_v<lookup_entry>;
    static constexpr bool has_explicit_collection_lookup =
        has_explicit_lookup &&
        std::is_same_v<selected_cardinality, ::dingo::many>;
  };

  template <typename Interface, typename Key>
  struct lookup_index_route
      : lookup_index_route_impl<Interface,
                                typename std::decay_t<Key>::definition_type> {
    using key_definition = typename std::decay_t<Key>::definition_type;
    using base = lookup_index_route_impl<Interface, key_definition>;
    using entry = typename base::lookup_entry;

    static decltype(auto) key(const Key &key) {
      if constexpr (std::is_same_v<entry, base_lookup_entry>) {
        return base_typed_key_lookup_key<typename base::interface_type, Key>();
      } else {
        return detail::lookup_backend_key(key);
      }
    }
  };

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

  template <typename T, typename LookupKey,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  std::size_t count(runtime_bindings_state *state, LookupKey key) {
    std::size_t result = 0;
    if (state) {
      for_each_collection_entry<T>(*state, key, [&](auto &) { ++result; });
    }
    return result;
  }

  template <typename Value, typename Exact, bool AllowMiss, typename LookupKey,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  detail::binding_status binding_status(runtime_bindings_state *state,
                                        LookupKey key) {
    if (!state) {
      return detail::binding_status::not_found;
    }
    auto selection = select_binding<Value, Exact, AllowMiss>(*state, key);
    return selection.status;
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
        if (!selection.found()) {
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

  runtime_selection make_selection(runtime_lookup_value *selected,
                                   bool ambiguous) {
    if (ambiguous) {
      return runtime_selection::ambiguity();
    }
    return detail::make_runtime_selection<runtime_binding_interface_type>(
        selected ? &selected->binding() : nullptr);
  }

  template <typename LookupEntry, typename Key>
  runtime_selection select_lookup(runtime_bindings_state &state,
                                  const Key &key) {
    auto &index = state.lookup_indexes.template get<LookupEntry>();
    auto match =
        detail::lookup_find_singular<LookupEntry, runtime_lookup_value>(index,
                                                                        key);
    return make_selection(match.value, match.ambiguous);
  }

  template <typename LookupEntry, typename Key, typename Fn>
  std::size_t for_each_lookup(runtime_bindings_state &state, const Key &key,
                              Fn &&fn) {
    auto &index = state.lookup_indexes.template get<LookupEntry>();
    return detail::lookup_for_each<LookupEntry>(index, key,
                                                std::forward<Fn>(fn));
  }
};

} // namespace detail
} // namespace dingo
