//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

// The registration half of a runtime registry that validates runtime key
// values against the lookup declarations. The public registration API and the
// commit/rollback live in registry.h until the next slice; this store owns the
// key-value validation rules.

#include <dingo/base/type_list.h>
#include <dingo/runtime/selection.h>
#include <dingo/type/key.h>
#include <dingo/type/normalized_type.h>

#include <type_traits>
#include <utility>

namespace dingo {
namespace detail {

template <typename Interface> struct key_value_interface {
  using type = normalized_type_t<Interface>;
};

template <> struct key_value_interface<void> {
  using type = void;
};

template <typename Interface>
using key_value_interface_t = typename key_value_interface<Interface>::type;

template <typename ContainerTraits, typename Allocator>
class registration_store : public lookup_selection<ContainerTraits, Allocator> {
  using lookup_base = lookup_selection<ContainerTraits, Allocator>;

protected:
  template <typename LookupEntry>
  using runtime_lookup_entry_traits =
      typename lookup_base::template runtime_lookup_entry_traits<LookupEntry>;

  template <typename TypeInterface>
  using runtime_lookup_entries_for_interface_t =
      typename lookup_base::template runtime_lookup_entries_for_interface_t<
          TypeInterface>;

  template <typename Arg>
  using runtime_registration_arg_traits =
      detail::runtime_registration_key_arg_traits<std::decay_t<Arg>>;

  template <typename Entry, typename Arg>
  struct runtime_registration_arg_matches_entry {
  private:
    using entry_traits = runtime_lookup_entry_traits<Entry>;
    using arg_traits = runtime_registration_arg_traits<Arg>;
    using arg_interface =
        detail::key_value_interface_t<typename arg_traits::interface_type>;

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
        detail::key_value_interface_t<typename arg_traits::interface_type>;

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
        detail::key_value_interface_t<typename left_traits::interface_type>;
    using right_interface =
        detail::key_value_interface_t<typename right_traits::interface_type>;

  public:
    static constexpr bool value =
        std::is_same_v<typename left_traits::key_type,
                       typename right_traits::key_type> &&
        std::is_same_v<left_interface, right_interface>;
  };

  template <typename Arg, typename... KeyValueArgs>
  static constexpr std::size_t runtime_registration_key_arg_count_v =
      (std::size_t{0} + ... +
       (runtime_registration_args_are_same_key<Arg, KeyValueArgs>::value ? 1U
                                                                         : 0U));

  template <typename Key, typename... KeyValueArgs>
  static constexpr std::size_t
      unqualified_runtime_registration_key_arg_count_v =
          (std::size_t{0} + ... +
           ((std::is_same_v<typename runtime_registration_arg_traits<
                                KeyValueArgs>::key_type,
                            Key> &&
             std::is_void_v<typename runtime_registration_arg_traits<
                 KeyValueArgs>::interface_type>)
                ? 1U
                : 0U));

  template <typename Entry, typename... KeyValueArgs>
  static constexpr std::size_t qualified_runtime_registration_key_arg_count_v =
      (std::size_t{0} + ... +
       (runtime_registration_arg_matches_qualified_entry<Entry,
                                                         KeyValueArgs>::value
            ? 1U
            : 0U));

  template <typename Entries, typename Key>
  struct runtime_lookup_entry_key_count;

  template <typename Key>
  struct runtime_lookup_entry_key_count<type_list<>, Key>
      : std::integral_constant<std::size_t, 0> {};

  template <typename Head, typename... Tail, typename Key>
  struct runtime_lookup_entry_key_count<type_list<Head, Tail...>, Key>
      : std::integral_constant<
            std::size_t,
            (runtime_registration_arg_matches_entry<Head, Key>::value ? 1U
                                                                      : 0U) +
                runtime_lookup_entry_key_count<type_list<Tail...>,
                                               Key>::value> {};

  template <typename InterfaceList, typename Arg>
  struct runtime_registration_key_match_count;

  template <typename Arg>
  struct runtime_registration_key_match_count<type_list<>, Arg>
      : std::integral_constant<std::size_t, 0> {};

  template <typename Interface, typename... Tail, typename Arg>
  struct runtime_registration_key_match_count<type_list<Interface, Tail...>,
                                              Arg>
      : std::integral_constant<
            std::size_t,
            runtime_lookup_entry_key_count<
                runtime_lookup_entries_for_interface_t<Interface>, Arg>::value +
                runtime_registration_key_match_count<type_list<Tail...>,
                                                     Arg>::value> {};

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
    static_assert(type_list_size_v<Entries> > 0,
                  "runtime dingo::key_value<K>{value} registration requires a "
                  "matching runtime-key lookup definition");
    for_each(Entries{}, [](auto element) {
      using entry_type = typename decltype(element)::type;
      using key_type =
          typename runtime_lookup_entry_traits<entry_type>::key_type;
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
};

} // namespace detail
} // namespace dingo
