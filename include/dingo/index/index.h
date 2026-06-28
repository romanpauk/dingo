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
#include <dingo/type/normalized_type.h>
#include <dingo/type/type_list.h>

#include <optional>
#include <tuple>
#include <type_traits>
#include <utility>

namespace dingo {
template <typename Arg, typename Definitions> struct index_tag;
template <typename... Entries> struct indexes {};
template <typename Interface, typename Key, typename Backend> struct index {};
template <typename... Args> struct interfaces;
template <typename T, auto... Values> struct key;

namespace detail {
template <typename...> inline constexpr bool index_dependent_false_v = false;

template <typename Backend, typename Key, typename Value, typename Allocator>
struct index_storage;

template <typename Storage, typename Key, typename Value, typename = void>
struct has_index_storage_contract : std::false_type {};

template <typename Storage, typename Key, typename Value>
struct has_index_storage_contract<
    Storage, Key, Value,
    std::void_t<decltype(std::declval<Storage &>().emplace(
                    std::declval<Key>(), std::declval<Value>())),
                decltype(std::declval<Storage &>().find(
                    std::declval<const Key &>()))>>
    : std::bool_constant<
          std::is_same_v<decltype(std::declval<Storage &>().emplace(
                             std::declval<Key>(), std::declval<Value>())),
                         bool> &&
          std::is_same_v<decltype(std::declval<Storage &>().find(
                             std::declval<const Key &>())),
                         Value *>> {};

template <typename Storage, typename Key, typename Value>
inline constexpr bool has_index_storage_contract_v =
    has_index_storage_contract<Storage, Key, Value>::value;

template <typename Interface, typename Key, typename Tag> struct index_entry {
  using interface_type = Interface;
  using key = Key;
  using tag = Tag;
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

template <typename Interfaces, typename Key, typename Tag>
struct normalize_index_interfaces;

template <typename... Interfaces, typename Key, typename Tag>
struct normalize_index_interfaces<type_list<Interfaces...>, Key, Tag> {
  using type = type_list<index_entry<normalized_index_interface_t<Interfaces>,
                                     normalized_index_key_t<Key>, Tag>...>;
};

template <typename Definition> struct normalize_index_definition;

template <typename Interface, typename Key, typename Tag>
struct normalize_index_definition<::dingo::index<Interface, Key, Tag>>
    : normalize_index_interfaces<typename index_interface_arg<Interface>::type,
                                 Key, Tag> {};

template <typename DefinitionList> struct normalize_index_definitions;

template <typename... Definitions>
struct normalize_index_definitions<type_list<Definitions...>> {
  using type = type_list_cat_t<
      typename normalize_index_definition<Definitions>::type...>;
};

template <> struct normalize_index_definitions<std::tuple<>> {
  using type = type_list<>;
};

template <typename Definition, typename... Definitions>
struct normalize_index_definitions<std::tuple<Definition, Definitions...>> {
  static_assert(index_dependent_false_v<Definition, Definitions...>,
                "legacy std::tuple index definitions are no longer "
                "supported; use "
                "dingo::indexes<dingo::index<Interface, Key, Backend>>");
};

template <typename... Definitions>
struct normalize_index_definitions<::dingo::indexes<Definitions...>>
    : normalize_index_definitions<type_list<Definitions...>> {};

template <typename Definitions>
using normalize_index_definitions_t =
    typename normalize_index_definitions<Definitions>::type;

template <typename Candidate, typename Entry>
struct same_index_slot
    : std::bool_constant<
          std::is_same_v<typename Candidate::interface_type,
                         typename Entry::interface_type> &&
          std::is_same_v<typename Candidate::key, typename Entry::key>> {};

template <typename Candidate, typename List> struct contains_index_slot;

template <typename Candidate>
struct contains_index_slot<Candidate, type_list<>> : std::false_type {};

template <typename Candidate, typename Head, typename... Tail>
struct contains_index_slot<Candidate, type_list<Head, Tail...>>
    : std::bool_constant<
          same_index_slot<Candidate, Head>::value ||
          contains_index_slot<Candidate, type_list<Tail...>>::value> {};

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
      std::is_same_v<typename Head::key, Key>;

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

template <typename Entry> struct index_entry_tag {
  using type = typename Entry::tag;
};

template <> struct index_entry_tag<void> {
  using type = void;
};

template <typename Arg> struct index_lookup_arg {
  using interface_type = void;
  using key = Arg;
};

template <typename Interface, typename Key>
struct index_lookup_arg<type_list<Interface, Key>> {
  using interface_type = Interface;
  using key = Key;
};

template <typename Arg, typename Definitions> struct index_tag_impl {
private:
  using lookup = index_lookup_arg<Arg>;
  using entries = normalize_index_definitions_t<Definitions>;
  using entry = selected_index_entry_t<typename lookup::interface_type,
                                       typename lookup::key, entries>;

public:
  using type = typename index_entry_tag<entry>::type;
};

template <typename Entry, typename Value, typename Allocator>
struct index_holder {
  using index_type =
      index_storage<typename Entry::tag, typename Entry::key, Value, Allocator>;
  static_assert(std::is_constructible_v<index_type, Allocator &>,
                "index backend must be constructible from Allocator&");
  static_assert(
      has_index_storage_contract_v<index_type, typename Entry::key, Value>,
      "index backend must provide bool emplace(Key, Value) and Value* "
      "find(Key)");

  explicit index_holder(Allocator &allocator) : index_(allocator) {}

  index_type &get(Allocator &allocator) {
    if (!index_) {
      index_.emplace(allocator);
    }
    return *index_;
  }

private:
  allocator_ptr<index_type, Allocator> index_;
};

template <typename Entries, typename Value, typename Allocator>
struct index_bindings_impl;

template <typename Value, typename Allocator>
struct index_bindings_impl<type_list<>, Value, Allocator> {
  index_bindings_impl(Allocator &) {}

  template <typename Interface, typename Key> void get_index(Allocator &) {
    static_assert(index_dependent_false_v<Interface, Key>,
                  "indexed registration or lookup has no matching "
                  "dingo index definition for interface/key");
  }
};

template <typename Value, typename Allocator, typename... Entries>
struct index_bindings_impl<type_list<Entries...>, Value, Allocator> {
  index_bindings_impl(Allocator &allocator)
      : indexes_(index_holder<Entries, Value, Allocator>(allocator)...) {
    static_assert(!has_duplicate_index_slot_v<type_list<Entries...>>,
                  "duplicate dingo index definition for interface/key");
  }

  template <typename Interface, typename Key>
  decltype(auto) get_index(Allocator &allocator) {
    using entry = selected_index_entry_t<Interface, Key, type_list<Entries...>>;
    if constexpr (std::is_void_v<entry>) {
      static_assert(!std::is_void_v<entry>,
                    "indexed registration or lookup has no matching "
                    "dingo index definition for interface/key");
    } else {
      return std::get<index_holder<entry, Value, Allocator>>(indexes_).get(
          allocator);
    }
  }

private:
  std::tuple<index_holder<Entries, Value, Allocator>...> indexes_;
};

template <typename Definitions, typename Value, typename Allocator>
struct index_bindings
    : index_bindings_impl<normalize_index_definitions_t<Definitions>, Value,
                          Allocator> {
  using base = index_bindings_impl<normalize_index_definitions_t<Definitions>,
                                   Value, Allocator>;
  using base::base;
};

template <typename Value, typename Allocator> struct empty_index_bindings {
  empty_index_bindings(Allocator &) {}

  template <typename Interface, typename Key> struct empty_index {
    Value *find(const Key &) { return nullptr; }
  };

  template <typename Interface, typename Key>
  empty_index<Interface, Key> get_index(Allocator &) {
    return {};
  }
};

template <typename Value, typename Allocator>
struct index_bindings<std::tuple<>, Value, Allocator>
    : empty_index_bindings<Value, Allocator> {
  using empty_index_bindings<Value, Allocator>::empty_index_bindings;
};

template <typename Value, typename Allocator>
struct index_bindings<::dingo::indexes<>, Value, Allocator>
    : empty_index_bindings<Value, Allocator> {
  using empty_index_bindings<Value, Allocator>::empty_index_bindings;
};
} // namespace detail

template <typename Arg, typename Definitions>
struct index_tag : detail::index_tag_impl<Arg, Definitions> {};

} // namespace dingo
