//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/base/selected.h>
#include <dingo/base/type_list.h>
#include <dingo/lookup/lookup.h>
#include <dingo/type/key.h>
#include <dingo/type/normalized_type.h>

#include <cstddef>
#include <type_traits>
#include <utility>

namespace dingo {
namespace detail {

template <typename Request>
using binding_dependency_unwrapped_t =
    std::conditional_t<is_selected_v<Request>, selected_type_t<Request>,
                       Request>;

template <typename Request>
using binding_dependency_interface_t =
    normalized_type_t<binding_dependency_unwrapped_t<Request>>;

template <typename Request>
using binding_exact_dependency_interface_t = std::remove_cv_t<
    std::remove_reference_t<binding_dependency_unwrapped_t<Request>>>;

template <typename Request>
using binding_dependency_key_t =
    make_lookup_key_t<selected_selector_t<Request>>;

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

} // namespace detail
} // namespace dingo
