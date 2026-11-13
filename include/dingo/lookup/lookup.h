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
#include <dingo/lookup/lookup_key.h>
#include <dingo/lookup/ordered.h>
#include <dingo/lookup/unordered.h>
#include <dingo/type/shape.h>

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

// A lookup is defined for the interface it names, as a binding is bound to
// it: an array and its element are two interfaces.
template <typename T> using normalized_lookup_interface_t = std::remove_cv_t<T>;

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

template <typename Definitions> struct normalize_lookup_definitions;

template <typename... Definitions>
struct normalize_lookup_definitions<::dingo::lookups<Definitions...>> {
  using type = type_list_cat_t<
      typename normalize_lookup_definition<Definitions>::type...>;
};

template <typename Definitions>
using normalize_lookup_definitions_t =
    typename normalize_lookup_definitions<Definitions>::type;

template <typename Candidate, typename Entry>
struct is_same_lookup_key_type
    : std::bool_constant<
          std::is_same_v<typename Candidate::interface_type,
                         typename Entry::interface_type> &&
          std::is_same_v<typename Candidate::key_domain::definition_type,
                         typename Entry::key_domain::definition_type>> {};

template <typename Candidate, typename Entry>
struct is_same_lookup_definition
    : std::bool_constant<is_same_lookup_key_type<Candidate, Entry>::value &&
                         std::is_same_v<typename Candidate::cardinality,
                                        typename Entry::cardinality>> {};

// The number of entries of Entries that are the same as Entry under Same.
template <template <typename, typename> typename Same, typename Entry,
          typename... Entries>
inline constexpr std::size_t same_lookup_entry_count_v =
    (std::size_t{0} + ... + (Same<Entry, Entries>::value ? 1 : 0));

// Whether two entries of Entries are the same under Same.
template <template <typename, typename> typename Same, typename Entries>
struct has_duplicate_lookup_entry;

template <template <typename, typename> typename Same, typename... Entries>
struct has_duplicate_lookup_entry<Same, type_list<Entries...>>
    : std::bool_constant<(
          (same_lookup_entry_count_v<Same, Entries, Entries...> > 1) || ...)> {
};

template <typename Entries>
inline constexpr bool has_duplicate_lookup_key_type_v =
    has_duplicate_lookup_entry<is_same_lookup_key_type, Entries>::value;

template <typename Entries>
inline constexpr bool has_duplicate_lookup_definition_v =
    has_duplicate_lookup_entry<is_same_lookup_definition, Entries>::value;

template <typename Entry, typename Interface, typename Key>
inline constexpr bool is_lookup_entry_for_v =
    std::is_same_v<typename Entry::interface_type, Interface> &&
    std::is_same_v<typename Entry::key_domain::definition_type,
                   typename Key::definition_type>;

template <typename Interface, typename Key, typename Entries>
struct matching_lookup_entry;

template <typename Interface, typename Key, typename... Entries>
struct matching_lookup_entry<Interface, Key, type_list<Entries...>> {
  using type =
      type_list_find_t<void, type_list<Entries...>,
                       is_lookup_entry_for_v<Entries, Interface, Key>...>;
};

// The lookup of an interface: the one defined for the interface itself, or
// else the one of its normalized interface, as a request selects the bindings
// of its exact interface or else those of its normalized one.
template <typename Interface, typename Key, typename Entries>
struct selected_lookup_entry {
private:
  using exact_interface = std::remove_cv_t<std::remove_reference_t<Interface>>;
  using normalized_key = std::decay_t<Key>;
  static_assert(is_lookup_key_v<normalized_key>,
                "internal lookup selection requires dingo::detail::lookup_key");
  using exact_entry =
      typename matching_lookup_entry<exact_interface, normalized_key,
                                     Entries>::type;

public:
  using type = std::conditional_t<
      std::is_void_v<exact_entry>,
      typename matching_lookup_entry<normalized_type_t<Interface>,
                                     normalized_key, Entries>::type,
      exact_entry>;
};

template <typename Interface, typename Key, typename Entries>
using selected_lookup_entry_t =
    typename selected_lookup_entry<Interface, Key, Entries>::type;

} // namespace detail

} // namespace dingo
