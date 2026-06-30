//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/core/config.h>

#include <dingo/core/exceptions.h>
#include <dingo/memory/allocator.h>
#include <dingo/memory/static_allocator.h>
#include <dingo/type/normalized_type.h>
#include <dingo/type/type_list.h>

#include <memory>
#include <tuple>
#include <type_traits>

namespace dingo {
template <typename... Projections> struct selectors {};
struct no_key {};
template <typename Key> struct typed_key {};
template <typename Key> struct runtime_key {};
struct one {};
struct many {};
template <typename Interface, typename KeyDomain, typename Cardinality>
struct selector {};
template <typename Interface>
using collection = selector<Interface, no_key, many>;
template <typename Interface, typename Key, typename Cardinality = one>
using associative = selector<Interface, runtime_key<Key>, Cardinality>;
template <typename Interface, typename Key>
using unique_index = selector<Interface, runtime_key<Key>, one>;
template <typename Interface, typename Key>
using multi_index = selector<Interface, runtime_key<Key>, many>;
template <typename... Args> struct interfaces;
template <typename T, auto... Values> struct key;

namespace detail {
template <typename Interface, typename KeyDomain, typename Cardinality>
struct index_entry {
  using interface_type = Interface;
  using key_domain = KeyDomain;
  using cardinality = Cardinality;
};

template <typename T> struct index_interface_arg {
  using type = type_list<T>;
};

template <typename... Types> struct index_interface_arg<interfaces<Types...>> {
  using type = type_list<Types...>;
};

template <typename T> struct index_key_arg {
  using type = T;
};

template <typename T, auto... Values> struct index_key_arg<key<T, Values...>> {
  static_assert(sizeof...(Values) == 0,
                "index definitions require dingo::key<Key>, not "
                "dingo::key<Key, Value>");
  using type = T;
};

template <typename T> using normalized_index_interface_t = normalized_type_t<T>;

template <typename T>
using normalized_index_key_t =
    std::remove_cv_t<std::remove_reference_t<typename index_key_arg<T>::type>>;

template <typename Interfaces, typename KeyDomain, typename Cardinality>
struct normalize_index_interfaces;

template <typename... Interfaces, typename KeyDomain, typename Cardinality>
struct normalize_index_interfaces<type_list<Interfaces...>, KeyDomain,
                                  Cardinality> {
  using type = type_list<index_entry<normalized_index_interface_t<Interfaces>,
                                     KeyDomain, Cardinality>...>;
};

template <typename Definition> struct normalize_index_definition;

template <typename Interface, typename KeyDomain, typename Cardinality>
struct normalize_index_definition<
    ::dingo::selector<Interface, KeyDomain, Cardinality>>
    : normalize_index_interfaces<typename index_interface_arg<Interface>::type,
                                 KeyDomain, Cardinality> {};

template <typename DefinitionList> struct normalize_index_definitions;

template <typename... Definitions>
struct normalize_index_definitions<type_list<Definitions...>> {
  using type = type_list_cat_t<
      typename normalize_index_definition<Definitions>::type...>;
};

template <> struct normalize_index_definitions<std::tuple<>> {
  using type = type_list<>;
};

template <typename... Definitions>
struct normalize_index_definitions<::dingo::selectors<Definitions...>>
    : normalize_index_definitions<type_list<Definitions...>> {};

template <typename Definitions>
using normalize_index_definitions_t =
    typename normalize_index_definitions<Definitions>::type;

template <typename Candidate, typename Entry>
struct same_index_slot
    : std::bool_constant<std::is_same_v<typename Candidate::interface_type,
                                        typename Entry::interface_type> &&
                         std::is_same_v<typename Candidate::key_domain,
                                        typename Entry::key_domain>> {};

template <typename Candidate, typename Entry>
struct same_index_definition
    : std::bool_constant<same_index_slot<Candidate, Entry>::value &&
                         std::is_same_v<typename Candidate::cardinality,
                                        typename Entry::cardinality>> {};

template <typename Candidate, typename List> struct contains_index_slot;

template <typename Candidate>
struct contains_index_slot<Candidate, type_list<>> : std::false_type {};

template <typename Candidate, typename Head, typename... Tail>
struct contains_index_slot<Candidate, type_list<Head, Tail...>>
    : std::bool_constant<
          same_index_slot<Candidate, Head>::value ||
          contains_index_slot<Candidate, type_list<Tail...>>::value> {};

template <typename Candidate, typename List> struct contains_index_definition;

template <typename Candidate>
struct contains_index_definition<Candidate, type_list<>> : std::false_type {};

template <typename Candidate, typename Head, typename... Tail>
struct contains_index_definition<Candidate, type_list<Head, Tail...>>
    : std::bool_constant<
          same_index_definition<Candidate, Head>::value ||
          contains_index_definition<Candidate, type_list<Tail...>>::value> {};

template <typename Seen, typename Remaining> struct has_duplicate_index_slot;

template <typename... Seen>
struct has_duplicate_index_slot<type_list<Seen...>, type_list<>>
    : std::false_type {};

template <typename... Seen, typename Head, typename... Tail>
struct has_duplicate_index_slot<type_list<Seen...>, type_list<Head, Tail...>>
    : std::bool_constant<contains_index_slot<Head, type_list<Seen...>>::value ||
                         has_duplicate_index_slot<type_list<Seen..., Head>,
                                                  type_list<Tail...>>::value> {
};

template <typename Entries>
inline constexpr bool has_duplicate_index_slot_v =
    has_duplicate_index_slot<type_list<>, Entries>::value;

template <typename Seen, typename Remaining>
struct has_duplicate_index_definition;

template <typename... Seen>
struct has_duplicate_index_definition<type_list<Seen...>, type_list<>>
    : std::false_type {};

template <typename... Seen, typename Head, typename... Tail>
struct has_duplicate_index_definition<type_list<Seen...>,
                                      type_list<Head, Tail...>>
    : std::bool_constant<
          contains_index_definition<Head, type_list<Seen...>>::value ||
          has_duplicate_index_definition<type_list<Seen..., Head>,
                                         type_list<Tail...>>::value> {};

template <typename Entries>
inline constexpr bool has_duplicate_index_definition_v =
    has_duplicate_index_definition<type_list<>, Entries>::value;

template <typename Interface, typename Key, typename Entries>
struct index_entry_for;

template <typename Interface, typename Key>
struct index_entry_for<Interface, Key, type_list<>> {
  using type = void;
};

template <typename Interface, typename Key, typename Head, typename... Tail>
struct index_entry_for<Interface, Key, type_list<Head, Tail...>> {
private:
  static constexpr bool exact =
      std::is_same_v<typename Head::interface_type, Interface> &&
      std::is_same_v<typename Head::key_domain, ::dingo::runtime_key<Key>>;

public:
  using type = std::conditional_t<
      exact, Head,
      typename index_entry_for<Interface, Key, type_list<Tail...>>::type>;
};

template <typename Interface, typename Key, typename Entries>
struct selected_index_entry {
private:
  using normalized_interface = normalized_index_interface_t<Interface>;
  using normalized_key = normalized_index_key_t<Key>;

public:
  using type = typename index_entry_for<normalized_interface, normalized_key,
                                        Entries>::type;
};

template <typename Interface, typename Key, typename Entries>
using selected_index_entry_t =
    typename selected_index_entry<Interface, Key, Entries>::type;

template <typename Interface, typename Cardinality, typename Entries>
struct no_key_index_entry_for;

template <typename Interface, typename Cardinality>
struct no_key_index_entry_for<Interface, Cardinality, type_list<>> {
  using type = void;
};

template <typename Interface, typename Cardinality, typename Head,
          typename... Tail>
struct no_key_index_entry_for<Interface, Cardinality,
                              type_list<Head, Tail...>> {
private:
  static constexpr bool exact =
      std::is_same_v<typename Head::interface_type, Interface> &&
      std::is_same_v<typename Head::key_domain, ::dingo::no_key> &&
      std::is_same_v<typename Head::cardinality, Cardinality>;

public:
  using type =
      std::conditional_t<exact, Head,
                         typename no_key_index_entry_for<
                             Interface, Cardinality, type_list<Tail...>>::type>;
};

template <typename Interface, typename Cardinality, typename Entries>
struct selected_no_key_index_entry {
private:
  using normalized_interface = normalized_index_interface_t<Interface>;

public:
  using type = typename no_key_index_entry_for<normalized_interface,
                                               Cardinality, Entries>::type;
};

template <typename Interface, typename Cardinality, typename Entries>
using selected_no_key_index_entry_t =
    typename selected_no_key_index_entry<Interface, Cardinality, Entries>::type;

template <typename Interface, typename Key, typename Cardinality,
          typename Entries>
struct typed_key_index_entry_for;

template <typename Interface, typename Key, typename Cardinality>
struct typed_key_index_entry_for<Interface, Key, Cardinality, type_list<>> {
  using type = void;
};

template <typename Interface, typename Key, typename Cardinality, typename Head,
          typename... Tail>
struct typed_key_index_entry_for<Interface, Key, Cardinality,
                                 type_list<Head, Tail...>> {
private:
  static constexpr bool exact =
      std::is_same_v<typename Head::interface_type, Interface> &&
      std::is_same_v<typename Head::key_domain, ::dingo::typed_key<Key>> &&
      std::is_same_v<typename Head::cardinality, Cardinality>;

public:
  using type = std::conditional_t<
      exact, Head,
      typename typed_key_index_entry_for<Interface, Key, Cardinality,
                                         type_list<Tail...>>::type>;
};

template <typename Interface, typename Key, typename Cardinality,
          typename Entries>
struct selected_typed_key_index_entry {
private:
  using normalized_interface = normalized_index_interface_t<Interface>;
  using normalized_key = normalized_index_key_t<Key>;

public:
  using type =
      typename typed_key_index_entry_for<normalized_interface, normalized_key,
                                         Cardinality, Entries>::type;
};

template <typename Interface, typename Key, typename Cardinality,
          typename Entries>
using selected_typed_key_index_entry_t =
    typename selected_typed_key_index_entry<Interface, Key, Cardinality,
                                            Entries>::type;

template <typename Value, typename Allocator>
using selector_storage_allocator_t = std::conditional_t<
    ::dingo::is_static_allocator_v<Allocator>, std::allocator<Value>,
    typename std::allocator_traits<Allocator>::template rebind_alloc<Value>>;

template <typename StorageAllocator, typename Allocator>
StorageAllocator make_selector_storage_allocator(Allocator &allocator) {
  if constexpr (::dingo::is_static_allocator_v<Allocator>) {
    (void)allocator;
    return StorageAllocator{};
  } else {
    return StorageAllocator(allocator);
  }
}

} // namespace detail

} // namespace dingo
