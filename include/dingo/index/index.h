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

#include <cstddef>
#include <memory>
#include <tuple>
#include <type_traits>

namespace dingo {
namespace detail {
template <typename Key, typename Rows, typename Allocator>
class linear_lookup_storage;
template <typename Key, typename Rows, typename Allocator>
class ordered_lookup_storage;
template <typename Key, typename Rows, typename Allocator>
class unordered_lookup_storage;
template <typename Key, typename Rows, typename Allocator, std::size_t Size>
class array_lookup_storage;
} // namespace detail

struct linear {
  template <typename Key, typename Rows, typename Allocator>
  using storage = detail::linear_lookup_storage<Key, Rows, Allocator>;
};
struct ordered {
  template <typename Key, typename Rows, typename Allocator>
  using storage = detail::ordered_lookup_storage<Key, Rows, Allocator>;
};
struct unordered {
  template <typename Key, typename Rows, typename Allocator>
  using storage = detail::unordered_lookup_storage<Key, Rows, Allocator>;
};
template <std::size_t Size> struct array {
  template <typename Key, typename Rows, typename Allocator>
  using storage = detail::array_lookup_storage<Key, Rows, Allocator, Size>;
};

template <typename... Definitions> struct queries {};
struct no_key {};
template <typename Key> struct typed_key {};
template <typename Key> struct runtime_key {};
struct one {};
struct many {};
namespace detail {
struct no_lookup_backend {};
template <typename Interface, typename KeyDomain, typename Cardinality,
          typename Backend = no_lookup_backend>
struct query_definition {};
} // namespace detail
template <typename Interface>
using single = detail::query_definition<Interface, no_key, one>;
template <typename Interface>
using collection = detail::query_definition<Interface, no_key, many>;
template <typename Key, typename Interface, typename Cardinality = one,
          typename Backend = ordered>
using associative =
    detail::query_definition<Interface, runtime_key<Key>, Cardinality, Backend>;
template <typename Interface, typename Key, typename Cardinality = one>
using typed = detail::query_definition<Interface, typed_key<Key>, Cardinality>;
template <typename... Args> struct interfaces;
template <typename T, auto... Values> struct key;

namespace detail {
template <typename Interface, typename KeyDomain, typename Cardinality,
          typename Backend>
struct lookup_entry {
  using interface_type = Interface;
  using key_domain = KeyDomain;
  using cardinality = Cardinality;
  using backend_type = Backend;
};

template <typename T> struct lookup_interface_arg {
  using type = type_list<T>;
};

template <typename... Types> struct lookup_interface_arg<interfaces<Types...>> {
  using type = type_list<Types...>;
};

template <typename T> struct lookup_key_arg {
  using type = T;
};

template <typename T, auto... Values> struct lookup_key_arg<key<T, Values...>> {
  static_assert(sizeof...(Values) == 0,
                "query definitions require dingo::key<Key>, not "
                "dingo::key<Key, Value>");
  using type = T;
};

template <typename T>
using normalized_lookup_interface_t = normalized_type_t<T>;

template <typename T>
using normalized_lookup_key_t =
    std::remove_cv_t<std::remove_reference_t<typename lookup_key_arg<T>::type>>;

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
    query_definition<Interface, KeyDomain, Cardinality, Backend>>
    : normalize_lookup_interfaces<
          typename lookup_interface_arg<Interface>::type, KeyDomain,
          Cardinality, Backend> {};

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
struct normalize_lookup_definitions<::dingo::queries<Definitions...>>
    : normalize_lookup_definitions<type_list<Definitions...>> {};

template <typename Definitions>
using normalize_lookup_definitions_t =
    typename normalize_lookup_definitions<Definitions>::type;

template <typename Candidate, typename Entry>
struct same_lookup_slot
    : std::bool_constant<std::is_same_v<typename Candidate::interface_type,
                                        typename Entry::interface_type> &&
                         std::is_same_v<typename Candidate::key_domain,
                                        typename Entry::key_domain>> {};

template <typename Candidate, typename Entry>
struct same_lookup_definition
    : std::bool_constant<same_lookup_slot<Candidate, Entry>::value &&
                         std::is_same_v<typename Candidate::cardinality,
                                        typename Entry::cardinality>> {};

template <typename Candidate, typename List> struct contains_lookup_slot;

template <typename Candidate>
struct contains_lookup_slot<Candidate, type_list<>> : std::false_type {};

template <typename Candidate, typename Head, typename... Tail>
struct contains_lookup_slot<Candidate, type_list<Head, Tail...>>
    : std::bool_constant<
          same_lookup_slot<Candidate, Head>::value ||
          contains_lookup_slot<Candidate, type_list<Tail...>>::value> {};

template <typename Candidate, typename List> struct contains_lookup_definition;

template <typename Candidate>
struct contains_lookup_definition<Candidate, type_list<>> : std::false_type {};

template <typename Candidate, typename Head, typename... Tail>
struct contains_lookup_definition<Candidate, type_list<Head, Tail...>>
    : std::bool_constant<
          same_lookup_definition<Candidate, Head>::value ||
          contains_lookup_definition<Candidate, type_list<Tail...>>::value> {};

template <typename Seen, typename Remaining> struct has_duplicate_lookup_slot;

template <typename... Seen>
struct has_duplicate_lookup_slot<type_list<Seen...>, type_list<>>
    : std::false_type {};

template <typename... Seen, typename Head, typename... Tail>
struct has_duplicate_lookup_slot<type_list<Seen...>, type_list<Head, Tail...>>
    : std::bool_constant<
          contains_lookup_slot<Head, type_list<Seen...>>::value ||
          has_duplicate_lookup_slot<type_list<Seen..., Head>,
                                    type_list<Tail...>>::value> {};

template <typename Entries>
inline constexpr bool has_duplicate_lookup_slot_v =
    has_duplicate_lookup_slot<type_list<>, Entries>::value;

template <typename Seen, typename Remaining>
struct has_duplicate_lookup_definition;

template <typename... Seen>
struct has_duplicate_lookup_definition<type_list<Seen...>, type_list<>>
    : std::false_type {};

template <typename... Seen, typename Head, typename... Tail>
struct has_duplicate_lookup_definition<type_list<Seen...>,
                                       type_list<Head, Tail...>>
    : std::bool_constant<
          contains_lookup_definition<Head, type_list<Seen...>>::value ||
          has_duplicate_lookup_definition<type_list<Seen..., Head>,
                                          type_list<Tail...>>::value> {};

template <typename Entries>
inline constexpr bool has_duplicate_lookup_definition_v =
    has_duplicate_lookup_definition<type_list<>, Entries>::value;

template <typename Interface, typename Key, typename Entries>
struct matching_lookup_entry;

template <typename Interface, typename Key>
struct matching_lookup_entry<Interface, Key, type_list<>> {
  using type = void;
};

template <typename Interface, typename Key, typename Head, typename... Tail>
struct matching_lookup_entry<Interface, Key, type_list<Head, Tail...>> {
private:
  static constexpr bool exact =
      std::is_same_v<typename Head::interface_type, Interface> &&
      std::is_same_v<typename Head::key_domain, ::dingo::runtime_key<Key>>;

public:
  using type = std::conditional_t<
      exact, Head,
      typename matching_lookup_entry<Interface, Key, type_list<Tail...>>::type>;
};

template <typename Interface, typename Key, typename Entries>
struct selected_lookup_entry {
private:
  using normalized_interface = normalized_lookup_interface_t<Interface>;
  using normalized_key = normalized_lookup_key_t<Key>;

public:
  using type = typename matching_lookup_entry<normalized_interface,
                                              normalized_key, Entries>::type;
};

template <typename Interface, typename Key, typename Entries>
using selected_lookup_entry_t =
    typename selected_lookup_entry<Interface, Key, Entries>::type;

template <typename Interface, typename Cardinality, typename Entries>
struct matching_no_key_lookup_entry;

template <typename Interface, typename Cardinality>
struct matching_no_key_lookup_entry<Interface, Cardinality, type_list<>> {
  using type = void;
};

template <typename Interface, typename Cardinality, typename Head,
          typename... Tail>
struct matching_no_key_lookup_entry<Interface, Cardinality,
                                    type_list<Head, Tail...>> {
private:
  static constexpr bool exact =
      std::is_same_v<typename Head::interface_type, Interface> &&
      std::is_same_v<typename Head::key_domain, ::dingo::no_key> &&
      std::is_same_v<typename Head::cardinality, Cardinality>;

public:
  using type =
      std::conditional_t<exact, Head,
                         typename matching_no_key_lookup_entry<
                             Interface, Cardinality, type_list<Tail...>>::type>;
};

template <typename Interface, typename Cardinality, typename Entries>
struct selected_no_key_lookup_entry {
private:
  using normalized_interface = normalized_lookup_interface_t<Interface>;

public:
  using type =
      typename matching_no_key_lookup_entry<normalized_interface, Cardinality,
                                            Entries>::type;
};

template <typename Interface, typename Cardinality, typename Entries>
using selected_no_key_lookup_entry_t =
    typename selected_no_key_lookup_entry<Interface, Cardinality,
                                          Entries>::type;

template <typename Interface, typename Key, typename Cardinality,
          typename Entries>
struct matching_typed_key_lookup_entry;

template <typename Interface, typename Key, typename Cardinality>
struct matching_typed_key_lookup_entry<Interface, Key, Cardinality,
                                       type_list<>> {
  using type = void;
};

template <typename Interface, typename Key, typename Cardinality, typename Head,
          typename... Tail>
struct matching_typed_key_lookup_entry<Interface, Key, Cardinality,
                                       type_list<Head, Tail...>> {
private:
  static constexpr bool exact =
      std::is_same_v<typename Head::interface_type, Interface> &&
      std::is_same_v<typename Head::key_domain, ::dingo::typed_key<Key>> &&
      std::is_same_v<typename Head::cardinality, Cardinality>;

public:
  using type = std::conditional_t<
      exact, Head,
      typename matching_typed_key_lookup_entry<Interface, Key, Cardinality,
                                               type_list<Tail...>>::type>;
};

template <typename Interface, typename Key, typename Cardinality,
          typename Entries>
struct selected_typed_key_lookup_entry {
private:
  using normalized_interface = normalized_lookup_interface_t<Interface>;
  using normalized_key = normalized_lookup_key_t<Key>;

public:
  using type = typename matching_typed_key_lookup_entry<
      normalized_interface, normalized_key, Cardinality, Entries>::type;
};

template <typename Interface, typename Key, typename Cardinality,
          typename Entries>
using selected_typed_key_lookup_entry_t =
    typename selected_typed_key_lookup_entry<Interface, Key, Cardinality,
                                             Entries>::type;

template <typename Value, typename Allocator>
using lookup_storage_allocator_t = std::conditional_t<
    ::dingo::is_static_allocator_v<Allocator>, std::allocator<Value>,
    typename std::allocator_traits<Allocator>::template rebind_alloc<Value>>;

template <typename StorageAllocator, typename Allocator>
StorageAllocator make_lookup_storage_allocator(Allocator &allocator) {
  if constexpr (::dingo::is_static_allocator_v<Allocator>) {
    (void)allocator;
    return StorageAllocator{};
  } else {
    return StorageAllocator(allocator);
  }
}

} // namespace detail

} // namespace dingo
