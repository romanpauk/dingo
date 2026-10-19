//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/base/type_list.h>
#include <dingo/dependency/normalized_type.h>
#include <dingo/key/key.h>
#include <dingo/lookup/base.h>
#include <dingo/lookup/lookup.h>
#include <dingo/lookup/tags.h>

#include <type_traits>

namespace dingo::detail {

// The lookup shape of one registry: the lookup definitions of its container
// traits normalized into lookup entries, and the base lookup every registration
// is published through.
template <typename Rtti, typename LookupDefinition> struct lookup_routing {
  using rtti_type = Rtti;
  using lookup_definition_type = LookupDefinition;
  using normalized_lookup_entries =
      normalize_lookup_definitions_t<lookup_definition_type>;
  using base_lookup_definition =
      detail::base_lookup_definition_t<lookup_definition_type>;
  using base_lookup_entry =
      detail::base_lookup_entry<rtti_type,
                                typename base_lookup_definition::cardinality,
                                typename base_lookup_definition::backend_type>;
  using base_lookup_cardinality = typename base_lookup_definition::cardinality;
  using lookup_index_entries = type_list_unique_t<
      type_list_cat_t<normalized_lookup_entries, type_list<base_lookup_entry>>>;
};

template <typename LookupKeyDefinition> struct base_lookup_key_type;

template <typename T>
struct base_lookup_key_type<detail::lookup_key<key_type<T>>> {
  using type = T;
};

template <typename T, typename Interface>
struct base_lookup_key_type<detail::lookup_key<key_value<T, Interface>>> {
  using type = T;
};

template <typename Interface, typename Key, typename Routing>
base_lookup_key<typename Routing::rtti_type> base_typed_key_lookup_key() {
  using rtti_type = typename Routing::rtti_type;
  using key_definition = typename std::decay_t<Key>::definition_type;
  using key_type = typename base_lookup_key_type<key_definition>::type;
  return {rtti_type::template get_type_index<Interface>(),
          rtti_type::template get_type_index<key_type>()};
}

template <typename Interface, typename KeyDomain, typename Cardinality,
          typename Routing>
using static_key_lookup_entry_t = selected_lookup_entry_with_cardinality_t<
    Interface, KeyDomain, Cardinality,
    typename Routing::normalized_lookup_entries>;

template <typename Interface, typename KeyDomain, typename Routing>
using static_key_lookup_cardinality_t = std::conditional_t<
    std::is_void_v<static_key_lookup_entry_t<Interface, KeyDomain, ::dingo::one,
                                             Routing>> &&
        !std::is_void_v<static_key_lookup_entry_t<Interface, KeyDomain,
                                                  ::dingo::many, Routing>>,
    ::dingo::many,
    std::conditional_t<!std::is_void_v<static_key_lookup_entry_t<
                           Interface, KeyDomain, ::dingo::one, Routing>>,
                       ::dingo::one,
                       typename Routing::base_lookup_cardinality>>;

template <typename Interface, typename KeyDefinition, typename Routing,
          typename = void>
struct lookup_index_route_impl;

template <typename Interface, typename KeyDefinition, typename Routing>
struct lookup_index_route_impl<
    Interface, KeyDefinition, Routing,
    std::enable_if_t<is_static_lookup_key_definition_v<KeyDefinition>>> {
  using interface_type = Interface;
  using normalized_interface = normalized_type_t<Interface>;
  using key_domain = KeyDefinition;
  using one_lookup_entry =
      static_key_lookup_entry_t<normalized_interface, key_domain, ::dingo::one,
                                Routing>;
  using many_lookup_entry =
      static_key_lookup_entry_t<normalized_interface, key_domain, ::dingo::many,
                                Routing>;
  using selected_cardinality =
      static_key_lookup_cardinality_t<normalized_interface, key_domain,
                                      Routing>;
  using lookup_entry = std::conditional_t<
      std::is_same_v<selected_cardinality, ::dingo::one> &&
          !std::is_void_v<one_lookup_entry>,
      one_lookup_entry,
      std::conditional_t<std::is_same_v<selected_cardinality, ::dingo::many> &&
                             !std::is_void_v<many_lookup_entry>,
                         many_lookup_entry,
                         typename Routing::base_lookup_entry>>;
  static constexpr bool has_explicit_lookup = false;
  static constexpr bool has_explicit_collection_lookup =
      std::is_same_v<selected_cardinality, ::dingo::many> &&
      !std::is_void_v<many_lookup_entry>;
};

template <typename Interface, typename KeyDefinition, typename Routing>
struct lookup_index_route_impl<
    Interface, KeyDefinition, Routing,
    std::enable_if_t<!is_static_lookup_key_definition_v<KeyDefinition>>> {
  using normalized_interface = normalized_type_t<Interface>;
  using key_domain = KeyDefinition;
  using lookup_entry =
      selected_lookup_entry_t<normalized_interface, key_domain,
                              typename Routing::normalized_lookup_entries>;
  using selected_cardinality =
      typename lookup_entry_cardinality<lookup_entry>::type;
  static constexpr bool has_explicit_lookup = !std::is_void_v<lookup_entry>;
  static constexpr bool has_explicit_collection_lookup =
      has_explicit_lookup &&
      std::is_same_v<selected_cardinality, ::dingo::many>;
};

template <typename Interface, typename Key, typename Routing>
struct lookup_index_route
    : lookup_index_route_impl<
          Interface, typename std::decay_t<Key>::definition_type, Routing> {
  using key_definition = typename std::decay_t<Key>::definition_type;
  using base = lookup_index_route_impl<Interface, key_definition, Routing>;
  using entry = typename base::lookup_entry;

  static decltype(auto) key(const Key &key) {
    if constexpr (std::is_same_v<entry, typename Routing::base_lookup_entry>) {
      return base_typed_key_lookup_key<typename base::interface_type, Key,
                                       Routing>();
    } else {
      return detail::lookup_backend_key(key);
    }
  }
};

} // namespace dingo::detail
