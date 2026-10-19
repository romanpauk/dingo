//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/base/none.h>
#include <dingo/base/type_list.h>
#include <dingo/dependency/normalized_type.h>
#include <dingo/key/key.h>
#include <dingo/lookup/lookup.h>

#include <cstddef>
#include <tuple>
#include <type_traits>

namespace dingo::detail {

template <typename Interface> struct key_value_interface {
  using type = normalized_type_t<Interface>;
};

template <> struct key_value_interface<void> {
  using type = void;
};

template <typename Interface>
using key_value_interface_t = typename key_value_interface<Interface>::type;

template <typename LookupEntry> struct runtime_lookup_entry_traits {
  static constexpr bool is_key_value = false;
  using interface_type = void;
  using key_type = ::dingo::key_type<::dingo::none_t>;
};

template <typename Interface, typename Key, typename Cardinality,
          typename Backend>
struct runtime_lookup_entry_traits<
    lookup_entry<Interface, lookup_key<key_value<Key>>, Cardinality, Backend>> {
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

template <typename TypeInterface, typename Entries>
using runtime_lookup_entries_for_interface_t =
    typename runtime_lookup_entries_for_interface<TypeInterface, Entries>::type;

template <typename Arg>
using runtime_registration_arg_traits =
    runtime_registration_key_arg_traits<std::decay_t<Arg>>;

template <typename Entry, typename Arg>
struct runtime_registration_arg_matches_entry {
private:
  using entry_traits = runtime_lookup_entry_traits<Entry>;
  using arg_traits = runtime_registration_arg_traits<Arg>;
  using arg_interface =
      key_value_interface_t<typename arg_traits::interface_type>;

public:
  static constexpr bool value =
      std::is_same_v<typename entry_traits::key_type,
                     typename arg_traits::key_type> &&
      (std::is_void_v<arg_interface> ||
       std::is_same_v<typename entry_traits::interface_type, arg_interface>);
};

template <typename Entry, typename Arg>
struct runtime_registration_arg_matches_qualified_entry {
private:
  using entry_traits = runtime_lookup_entry_traits<Entry>;
  using arg_traits = runtime_registration_arg_traits<Arg>;
  using arg_interface =
      key_value_interface_t<typename arg_traits::interface_type>;

public:
  static constexpr bool value =
      !std::is_void_v<arg_interface> &&
      std::is_same_v<typename entry_traits::key_type,
                     typename arg_traits::key_type> &&
      std::is_same_v<typename entry_traits::interface_type, arg_interface>;
};

template <typename Left, typename Right>
struct runtime_registration_args_are_same_key {
private:
  using left_traits = runtime_registration_arg_traits<Left>;
  using right_traits = runtime_registration_arg_traits<Right>;
  using left_interface =
      key_value_interface_t<typename left_traits::interface_type>;
  using right_interface =
      key_value_interface_t<typename right_traits::interface_type>;

public:
  static constexpr bool value =
      std::is_same_v<typename left_traits::key_type,
                     typename right_traits::key_type> &&
      std::is_same_v<left_interface, right_interface>;
};

template <typename Arg, typename... KeyValueArgs>
inline constexpr std::size_t runtime_registration_key_arg_count_v =
    (std::size_t{0} + ... +
     (runtime_registration_args_are_same_key<Arg, KeyValueArgs>::value ? 1U
                                                                       : 0U));

template <typename Key, typename... KeyValueArgs>
inline constexpr std::size_t unqualified_runtime_registration_key_arg_count_v =
    (std::size_t{0} + ... +
     ((std::is_same_v<
           typename runtime_registration_arg_traits<KeyValueArgs>::key_type,
           Key> &&
       std::is_void_v<typename runtime_registration_arg_traits<
           KeyValueArgs>::interface_type>)
          ? 1U
          : 0U));

template <typename Entry, typename... KeyValueArgs>
inline constexpr std::size_t qualified_runtime_registration_key_arg_count_v =
    (std::size_t{0} + ... +
     (runtime_registration_arg_matches_qualified_entry<Entry,
                                                       KeyValueArgs>::value
          ? 1U
          : 0U));

template <typename Entries, typename Key> struct runtime_lookup_entry_key_count;

template <typename Key>
struct runtime_lookup_entry_key_count<type_list<>, Key>
    : std::integral_constant<std::size_t, 0> {};

template <typename Head, typename... Tail, typename Key>
struct runtime_lookup_entry_key_count<type_list<Head, Tail...>, Key>
    : std::integral_constant<
          std::size_t,
          (runtime_registration_arg_matches_entry<Head, Key>::value ? 1U : 0U) +
              runtime_lookup_entry_key_count<type_list<Tail...>, Key>::value> {
};

template <typename InterfaceList, typename Arg, typename Entries>
struct runtime_registration_key_match_count;

template <typename Arg, typename Entries>
struct runtime_registration_key_match_count<type_list<>, Arg, Entries>
    : std::integral_constant<std::size_t, 0> {};

template <typename Interface, typename... Tail, typename Arg, typename Entries>
struct runtime_registration_key_match_count<type_list<Interface, Tail...>, Arg,
                                            Entries>
    : std::integral_constant<
          std::size_t,
          runtime_lookup_entry_key_count<
              runtime_lookup_entries_for_interface_t<Interface, Entries>,
              Arg>::value +
              runtime_registration_key_match_count<type_list<Tail...>, Arg,
                                                   Entries>::value> {};

template <typename InterfaceList, typename Entries, typename... KeyValueArgs>
constexpr void validate_supplied_runtime_registration_keys() {
  static_assert(
      ((runtime_registration_key_arg_count_v<KeyValueArgs, KeyValueArgs...> ==
        1U) &&
       ...),
      "duplicate runtime key value arguments for runtime lookup "
      "registration");
  static_assert(
      ((runtime_registration_key_match_count<InterfaceList, KeyValueArgs,
                                             Entries>::value > 0U) &&
       ...),
      "supplied runtime key value has no matching runtime-key lookup "
      "definition for the registered interface");
}

template <typename Entries, typename... KeyValueArgs>
constexpr void validate_required_runtime_registration_keys() {
  static_assert(type_list_size_v<Entries> > 0,
                "runtime dingo::key_value<K>{value} registration requires a "
                "matching runtime-key lookup definition");
  for_each(Entries{}, [](auto element) {
    using entry_type = typename decltype(element)::type;
    using key_type = typename runtime_lookup_entry_traits<entry_type>::key_type;
    static_assert(
        qualified_runtime_registration_key_arg_count_v<entry_type,
                                                       KeyValueArgs...> +
                unqualified_runtime_registration_key_arg_count_v<
                    key_type, KeyValueArgs...> >=
            1U,
        "runtime-key lookup registration requires a runtime key value for "
        "each declared runtime-key lookup");
  });
}

template <typename Entry, typename Tuple, std::size_t Index = 0>
decltype(auto) unqualified_registration_key_value(Tuple &keys) {
  using tuple_type = std::remove_reference_t<Tuple>;
  using key_type = typename runtime_lookup_entry_traits<Entry>::key_type;
  if constexpr (Index >= std::tuple_size_v<tuple_type>) {
    static_assert(Index < std::tuple_size_v<tuple_type>,
                  "missing runtime key value for runtime-key lookup "
                  "registration");
  } else {
    using element_type = std::decay_t<std::tuple_element_t<Index, tuple_type>>;
    using element_traits = runtime_registration_arg_traits<element_type>;
    if constexpr (std::is_same_v<typename element_traits::key_type, key_type> &&
                  std::is_void_v<typename element_traits::interface_type>) {
      return std::get<Index>(keys).value();
    } else {
      return unqualified_registration_key_value<Entry, Tuple, Index + 1>(keys);
    }
  }
}

template <typename Entry, typename Tuple, std::size_t Index = 0>
decltype(auto) qualified_registration_key_value(Tuple &keys) {
  using tuple_type = std::remove_reference_t<Tuple>;
  if constexpr (Index >= std::tuple_size_v<tuple_type>) {
    return unqualified_registration_key_value<Entry>(keys);
  } else if constexpr (runtime_registration_arg_matches_qualified_entry<
                           Entry, std::decay_t<std::tuple_element_t<
                                      Index, tuple_type>>>::value) {
    return std::get<Index>(keys).value();
  } else {
    return qualified_registration_key_value<Entry, Tuple, Index + 1>(keys);
  }
}

} // namespace dingo::detail
