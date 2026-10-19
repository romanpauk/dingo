//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/base/type_list.h>
#include <dingo/binding/binding_selection.h>
#include <dingo/static/lookup.h>

#include <type_traits>
#include <utility>

namespace dingo {
namespace detail {

template <binding_status Status, typename Binding = void>
struct binding_choice {
  static constexpr binding_status status = Status;
  static constexpr bool found = Status == binding_status::found;
  using binding_type = Binding;
};

template <typename Binding>
using found_binding_choice_t = binding_choice<binding_status::found, Binding>;

using missing_binding_choice_t = binding_choice<binding_status::not_found>;

using ambiguous_binding_choice_t = binding_choice<binding_status::ambiguous>;

template <typename Bindings> struct static_binding;

template <> struct static_binding<type_list<>> {
  using type = missing_binding_choice_t;
};

template <typename Binding> struct static_binding<type_list<Binding>> {
  using type = found_binding_choice_t<Binding>;
};

template <typename Binding0, typename Binding1, typename... Bindings>
struct static_binding<type_list<Binding0, Binding1, Bindings...>> {
  using type = ambiguous_binding_choice_t;
};

template <typename Bindings>
using static_binding_t = typename static_binding<Bindings>::type;

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
struct binding_selection {
  using type =
      static_binding_t<bindings_t<Interface, LookupKey, InterfaceBindings>>;
};

template <typename Interface, typename LookupKey, typename InterfaceBindings>
struct binding_selection<Interface, LookupKey, InterfaceBindings, true>
    : indexed_binding_selection<Interface, LookupKey, InterfaceBindings> {};

template <typename Interface, typename LookupKey, typename InterfaceBindings>
using binding_selection_t =
    typename binding_selection<Interface, LookupKey, InterfaceBindings>::type;

template <typename Interface, typename LookupKey, typename InterfaceBindings>
using binding_t = typename binding_selection_t<Interface, LookupKey,
                                               InterfaceBindings>::binding_type;

} // namespace detail
} // namespace dingo
