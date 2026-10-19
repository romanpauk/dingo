//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/base/type_list.h>
#include <dingo/key/key.h>
#include <dingo/lookup/lookup.h>
#include <dingo/lookup/tags.h>
#include <dingo/static/index.h>

#include <cstddef>
#include <type_traits>

namespace dingo {
namespace detail {

struct key_value_lookup_not_required {};

template <typename InterfaceBinding, typename Entries,
          typename LookupKey = typename InterfaceBinding::key_type>
struct key_value_lookup_cardinality {
  using type = key_value_lookup_not_required;
};

template <typename InterfaceBinding, typename Entries, typename Key, auto Value>
struct key_value_lookup_cardinality<InterfaceBinding, Entries,
                                    lookup_key<::dingo::key_type<Key, Value>>> {
private:
  using key_domain = typename InterfaceBinding::key_type::definition_type;
  using entry =
      selected_lookup_entry_t<typename InterfaceBinding::interface_type,
                              key_domain, Entries>;

  template <typename Entry, bool Found = !std::is_void_v<Entry>>
  struct entry_cardinality {
    using type = void;
  };

  template <typename Entry> struct entry_cardinality<Entry, true> {
    using type = typename Entry::cardinality;
  };

public:
  using type = typename entry_cardinality<entry>::type;
};

template <typename InterfaceBinding, typename Entries>
using key_value_lookup_cardinality_t =
    typename key_value_lookup_cardinality<InterfaceBinding, Entries>::type;

template <typename InterfaceBinding, typename Entries>
struct key_value_binding_is_declared
    : std::bool_constant<!std::is_void_v<
          key_value_lookup_cardinality_t<InterfaceBinding, Entries>>> {};

template <typename InterfaceBindings, typename Entries>
struct key_value_bindings_are_declared;

template <typename... InterfaceBindings, typename Entries>
struct key_value_bindings_are_declared<type_list<InterfaceBindings...>, Entries>
    : std::bool_constant<(
          key_value_binding_is_declared<InterfaceBindings, Entries>::value &&
          ...)> {};

template <typename InterfaceBinding, typename Other>
struct key_value_duplicate_storage
    : std::bool_constant<
          std::is_same_v<typename InterfaceBinding::interface_type,
                         typename Other::interface_type> &&
          std::is_same_v<
              typename InterfaceBinding::binding_model_type::storage_type::type,
              typename Other::binding_model_type::storage_type::type> &&
          lookup_keys_match<typename InterfaceBinding::key_type,
                            typename Other::key_type>::value> {};

template <typename InterfaceBinding, typename InterfaceBindings>
struct key_value_duplicate_storage_count;

template <typename InterfaceBinding, typename... InterfaceBindings>
struct key_value_duplicate_storage_count<InterfaceBinding,
                                         type_list<InterfaceBindings...>>
    : std::integral_constant<
          size_t, (size_t{0} + ... +
                   (key_value_duplicate_storage<InterfaceBinding,
                                                InterfaceBindings>::value
                        ? size_t{1}
                        : size_t{0}))> {};

template <typename InterfaceBindings, typename Entries>
struct key_value_bindings_are_unique;

template <typename InterfaceBinding, typename InterfaceBindings,
          typename Entries,
          typename Cardinality =
              key_value_lookup_cardinality_t<InterfaceBinding, Entries>>
struct key_value_binding_is_unique : std::false_type {};

template <typename InterfaceBinding, typename InterfaceBindings,
          typename Entries>
struct key_value_binding_is_unique<InterfaceBinding, InterfaceBindings, Entries,
                                   key_value_lookup_not_required>
    : std::true_type {};

template <typename InterfaceBinding, typename... InterfaceBindings,
          typename Entries>
struct key_value_binding_is_unique<
    InterfaceBinding, type_list<InterfaceBindings...>, Entries, one>
    : std::bool_constant<
          binding_count_v<typename InterfaceBinding::interface_type,
                          typename InterfaceBinding::key_type,
                          type_list<InterfaceBindings...>> == 1> {};

template <typename InterfaceBinding, typename... InterfaceBindings,
          typename Entries>
struct key_value_binding_is_unique<
    InterfaceBinding, type_list<InterfaceBindings...>, Entries, many>
    : std::bool_constant<
          key_value_duplicate_storage_count<
              InterfaceBinding, type_list<InterfaceBindings...>>::value == 1> {
};

template <typename... InterfaceBindings, typename Entries>
struct key_value_bindings_are_unique<type_list<InterfaceBindings...>, Entries>
    : std::bool_constant<(
          key_value_binding_is_unique<InterfaceBindings,
                                      type_list<InterfaceBindings...>,
                                      Entries>::value &&
          ...)> {};

} // namespace detail
} // namespace dingo
