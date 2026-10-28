//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

// Type-level lookup selection over the interface bindings of a static source:
// filter by interface and key, index a keyless lookup by overload resolution,
// and turn the matches into a found/missing/ambiguous choice. It knows nothing
// about dependencies or storage; registry.h maps a request to these lookups.

#include <dingo/base/type_list.h>
#include <dingo/binding/binding_selection.h>
#include <dingo/type/key.h>

#include <cstddef>
#include <type_traits>
#include <utility>

namespace dingo {
namespace detail {

template <typename Interface, typename LookupKey, typename InterfaceBinding>
struct binding_matches
    : std::bool_constant<
          std::is_same_v<Interface,
                         typename InterfaceBinding::interface_type> &&
          lookup_keys_match<LookupKey,
                            typename InterfaceBinding::key_type>::value> {};

template <typename Interface, typename LookupKey, typename InterfaceBindings>
struct bindings;

template <typename Interface, typename LookupKey, typename... InterfaceBindings>
struct bindings<Interface, LookupKey, type_list<InterfaceBindings...>> {
  using type = type_list_cat_t<std::conditional_t<
      binding_matches<Interface, LookupKey, InterfaceBindings>::value,
      type_list<InterfaceBindings>, type_list<>>...>;
};

template <typename Interface, typename LookupKey, typename InterfaceBindings>
using bindings_t =
    typename bindings<Interface, LookupKey, InterfaceBindings>::type;

template <typename Interface, typename LookupKey, typename InterfaceBindings>
struct binding_count;

template <typename Interface, typename LookupKey, typename... InterfaceBindings>
struct binding_count<Interface, LookupKey, type_list<InterfaceBindings...>>
    : std::integral_constant<size_t, (size_t{0} + ... +
                                      (binding_matches<Interface, LookupKey,
                                                       InterfaceBindings>::value
                                           ? size_t{1}
                                           : size_t{0}))> {};

template <typename Interface, typename LookupKey, typename InterfaceBindings>
inline constexpr size_t binding_count_v =
    binding_count<Interface, LookupKey, InterfaceBindings>::value;

template <typename Interface, typename LookupKey> struct binding_lookup_tag {};

template <size_t Index, typename InterfaceBinding> struct binding_lookup_hit {
  using type = InterfaceBinding;
  static constexpr size_t index = Index;
};

// The result of a lookup that no binding answers. It carries the no-binding
// index of the static graph.
struct binding_lookup_miss {
  static constexpr size_t index = static_cast<size_t>(-1);
};

template <size_t Index, typename InterfaceBinding>
struct binding_lookup_index_entry {
  static binding_lookup_hit<Index, InterfaceBinding>
      select(binding_lookup_tag<typename InterfaceBinding::interface_type,
                                typename InterfaceBinding::key_type>);
};

// The overload set of the bindings answers a lookup by overload resolution: a
// unique binding is the exact match, no binding falls through to the ellipsis,
// and several bindings of the same interface and key are ambiguous.
template <typename InterfaceBindings, typename Indexes>
struct binding_lookup_index_impl;

template <typename... InterfaceBindings, size_t... Indexes>
struct binding_lookup_index_impl<type_list<InterfaceBindings...>,
                                 std::index_sequence<Indexes...>>
    : binding_lookup_index_entry<Indexes, InterfaceBindings>... {
  using binding_lookup_index_entry<Indexes, InterfaceBindings>::select...;
  static binding_lookup_miss select(...);
};

template <typename InterfaceBindings>
using binding_lookup_index = binding_lookup_index_impl<
    InterfaceBindings,
    std::make_index_sequence<type_list_size_v<InterfaceBindings>>>;

template <typename Hit> struct indexed_binding_choice {
  using type = found_binding_choice_t<typename Hit::type>;
};

template <> struct indexed_binding_choice<binding_lookup_miss> {
  using type = missing_binding_choice_t;
};

// The bindings of a lookup without a key are indexed by interface, so the
// choice is read from the overload set instead of counting the matches of every
// binding. An ambiguous lookup has no viable overload.
template <typename Interface, typename LookupKey, typename InterfaceBindings,
          typename = void>
struct indexed_binding_selection {
  using type = ambiguous_binding_choice_t;
};

template <typename Interface, typename LookupKey, typename InterfaceBindings>
struct indexed_binding_selection<
    Interface, LookupKey, InterfaceBindings,
    std::void_t<decltype(binding_lookup_index<InterfaceBindings>::select(
        binding_lookup_tag<Interface, std::decay_t<LookupKey>>{}))>>
    : indexed_binding_choice<
          decltype(binding_lookup_index<InterfaceBindings>::select(
              binding_lookup_tag<Interface, std::decay_t<LookupKey>>{}))> {};

template <typename Interface, typename LookupKey, typename InterfaceBindings,
          bool UseIndex = is_no_lookup_key_v<LookupKey>>
struct static_selection {
  using type =
      static_binding_t<bindings_t<Interface, LookupKey, InterfaceBindings>>;
};

template <typename Interface, typename LookupKey, typename InterfaceBindings>
struct static_selection<Interface, LookupKey, InterfaceBindings, true>
    : indexed_binding_selection<Interface, LookupKey, InterfaceBindings> {};

template <typename Interface, typename LookupKey, typename InterfaceBindings>
using static_selection_t =
    typename static_selection<Interface, LookupKey, InterfaceBindings>::type;

// Backward-compatible spellings of the static selection.
template <typename Interface, typename LookupKey, typename InterfaceBindings>
using binding_selection_t =
    static_selection_t<Interface, LookupKey, InterfaceBindings>;

template <typename Interface, typename LookupKey, typename InterfaceBindings>
using binding_t = typename binding_selection_t<Interface, LookupKey,
                                               InterfaceBindings>::binding_type;

} // namespace detail
} // namespace dingo
