//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/base/exceptions.h>
#include <dingo/base/type_list.h>
#include <dingo/lookup/array.h>
#include <dingo/lookup/base.h>
#include <dingo/lookup/collection.h>
#include <dingo/lookup/ordered.h>
#include <dingo/lookup/unordered.h>
#include <dingo/type/key.h>
#include <dingo/type/normalized_type.h>

#include <tuple>
#include <type_traits>

namespace dingo {
template <typename... Args> struct interfaces;
template <typename T, auto... Values> struct key_type;

template <typename Interface>
using single =
    detail::lookup_definition<Interface, detail::no_lookup_key_t, one>;
template <typename Key, typename Interface, typename Cardinality = one,
          typename Backend = ordered>
using associative =
    detail::lookup_definition<Interface, detail::lookup_key<key_value<Key>>,
                              Cardinality, Backend>;
template <typename Key, typename Interface, typename Cardinality = one>
using typed =
    detail::lookup_definition<Interface, detail::lookup_key<key_type<Key>>,
                              Cardinality>;

namespace detail {
template <typename Interface, typename KeyDomain, typename Cardinality,
          typename Backend>
struct lookup_entry {
  using interface_type = Interface;
  using key_domain = KeyDomain;
  using cardinality = Cardinality;
  using backend_type = Backend;
};

template <typename LookupEntry, bool = std::is_void_v<LookupEntry>>
struct lookup_entry_cardinality {
  using type = typename LookupEntry::cardinality;
};

template <typename LookupEntry>
struct lookup_entry_cardinality<LookupEntry, true> {
  using type = ::dingo::one;
};

template <typename T> struct lookup_interface_arg {
  using type = type_list<T>;
};

template <typename... Types> struct lookup_interface_arg<interfaces<Types...>> {
  using type = type_list<Types...>;
};

template <typename T>
using normalized_lookup_interface_t = normalized_type_t<T>;

template <typename Interfaces, typename KeyDomain, typename Cardinality,
          typename Backend>
struct normalize_lookup_interfaces;

template <typename... Interfaces, typename KeyDomain, typename Cardinality,
          typename Backend>
struct normalize_lookup_interfaces<type_list<Interfaces...>, KeyDomain,
                                   Cardinality, Backend> {
  using type = type_list<lookup_entry<normalized_lookup_interface_t<Interfaces>,
                                      KeyDomain, Cardinality, Backend>...>;
};

template <typename Definition> struct normalize_lookup_definition;

template <typename Interface, typename KeyDomain, typename Cardinality,
          typename Backend>
struct normalize_lookup_definition<
    lookup_definition<Interface, KeyDomain, Cardinality, Backend>>
    : normalize_lookup_interfaces<
          typename lookup_interface_arg<Interface>::type,
          typename KeyDomain::definition_type, Cardinality, Backend> {};

template <typename Cardinality, typename Backend>
struct normalize_lookup_definition<::dingo::base<Cardinality, Backend>> {
  using type = type_list<>;
};

template <typename DefinitionList> struct normalize_lookup_definitions;

template <typename... Definitions>
struct normalize_lookup_definitions<type_list<Definitions...>> {
  using type = type_list_cat_t<
      typename normalize_lookup_definition<Definitions>::type...>;
};

template <> struct normalize_lookup_definitions<std::tuple<>> {
  using type = type_list<>;
};

template <typename... Definitions>
struct normalize_lookup_definitions<::dingo::lookups<Definitions...>>
    : normalize_lookup_definitions<type_list<Definitions...>> {};

template <typename Definitions>
using normalize_lookup_definitions_t =
    typename normalize_lookup_definitions<Definitions>::type;

template <typename Candidate, typename Entry, bool CheckCardinality>
struct is_same_lookup_entry
    : std::bool_constant<
          std::is_same_v<typename Candidate::interface_type,
                         typename Entry::interface_type> &&
          std::is_same_v<typename Candidate::key_domain::definition_type,
                         typename Entry::key_domain::definition_type> &&
          (!CheckCardinality || std::is_same_v<typename Candidate::cardinality,
                                               typename Entry::cardinality>)> {
};

template <typename Candidate, typename Entry>
using is_same_lookup_key_type = is_same_lookup_entry<Candidate, Entry, false>;

template <typename Candidate, typename Entry>
using is_same_lookup_definition = is_same_lookup_entry<Candidate, Entry, true>;

template <typename Candidate, typename List, bool CheckCardinality>
struct contains_lookup_entry;

template <typename Candidate, bool CheckCardinality>
struct contains_lookup_entry<Candidate, type_list<>, CheckCardinality>
    : std::false_type {};

template <typename Candidate, bool CheckCardinality, typename Head,
          typename... Tail>
struct contains_lookup_entry<Candidate, type_list<Head, Tail...>,
                             CheckCardinality>
    : std::bool_constant<
          is_same_lookup_entry<Candidate, Head, CheckCardinality>::value ||
          contains_lookup_entry<Candidate, type_list<Tail...>,
                                CheckCardinality>::value> {};

template <typename Candidate, typename List>
using contains_lookup_key_type = contains_lookup_entry<Candidate, List, false>;

template <typename Candidate, typename List>
using contains_lookup_definition = contains_lookup_entry<Candidate, List, true>;

template <typename Seen, typename Remaining, bool CheckCardinality>
struct has_duplicate_lookup_entry;

template <typename... Seen, bool CheckCardinality>
struct has_duplicate_lookup_entry<type_list<Seen...>, type_list<>,
                                  CheckCardinality> : std::false_type {};

template <typename... Seen, bool CheckCardinality, typename Head,
          typename... Tail>
struct has_duplicate_lookup_entry<type_list<Seen...>, type_list<Head, Tail...>,
                                  CheckCardinality>
    : std::bool_constant<contains_lookup_entry<Head, type_list<Seen...>,
                                               CheckCardinality>::value ||
                         has_duplicate_lookup_entry<type_list<Seen..., Head>,
                                                    type_list<Tail...>,
                                                    CheckCardinality>::value> {
};

template <typename Entries>
inline constexpr bool has_duplicate_lookup_key_type_v =
    has_duplicate_lookup_entry<type_list<>, Entries, false>::value;

template <typename Entries>
inline constexpr bool has_duplicate_lookup_definition_v =
    has_duplicate_lookup_entry<type_list<>, Entries, true>::value;

template <typename Interface, typename Key, typename Cardinality,
          typename Entries>
struct matching_lookup_entry_impl;

template <typename Interface, typename Key, typename Cardinality>
struct matching_lookup_entry_impl<Interface, Key, Cardinality, type_list<>> {
  using type = void;
};

template <typename Interface, typename Key, typename Cardinality, typename Head,
          typename... Tail>
struct matching_lookup_entry_impl<Interface, Key, Cardinality,
                                  type_list<Head, Tail...>> {
private:
  static constexpr bool exact =
      std::is_same_v<typename Head::interface_type, Interface> &&
      std::is_same_v<typename Head::key_domain::definition_type,
                     typename Key::definition_type> &&
      (std::is_void_v<Cardinality> ||
       std::is_same_v<typename Head::cardinality, Cardinality>);

public:
  using type = std::conditional_t<
      exact, Head,
      typename matching_lookup_entry_impl<Interface, Key, Cardinality,
                                          type_list<Tail...>>::type>;
};

template <typename Interface, typename Key, typename Entries>
using matching_lookup_entry =
    matching_lookup_entry_impl<Interface, Key, void, Entries>;

template <typename Interface, typename Key, typename Cardinality,
          typename Entries>
using matching_lookup_entry_with_cardinality =
    matching_lookup_entry_impl<Interface, Key, Cardinality, Entries>;

template <typename Interface, typename Key, typename Cardinality,
          typename Entries>
struct selected_lookup_entry_for {
private:
  using normalized_interface = normalized_lookup_interface_t<Interface>;
  using normalized_key = std::decay_t<Key>;
  static_assert(is_lookup_key_v<normalized_key>,
                "internal lookup selection requires dingo::detail::lookup_key");

public:
  using type =
      typename matching_lookup_entry_impl<normalized_interface, normalized_key,
                                          Cardinality, Entries>::type;
};

template <typename Interface, typename Key, typename Entries>
using selected_lookup_entry =
    selected_lookup_entry_for<Interface, Key, void, Entries>;

template <typename Interface, typename Key, typename Entries>
using selected_lookup_entry_t =
    typename selected_lookup_entry<Interface, Key, Entries>::type;

template <typename Interface, typename Key, typename Cardinality,
          typename Entries>
using selected_lookup_entry_with_cardinality =
    selected_lookup_entry_for<Interface, Key, Cardinality, Entries>;

template <typename Interface, typename Key, typename Cardinality,
          typename Entries>
using selected_lookup_entry_with_cardinality_t =
    typename selected_lookup_entry_with_cardinality<Interface, Key, Cardinality,
                                                    Entries>::type;

} // namespace detail

} // namespace dingo
