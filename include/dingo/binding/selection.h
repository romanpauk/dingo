//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/base/type_list.h>

#include <cstddef>
#include <type_traits>

namespace dingo {
namespace detail {

enum class binding_status {
  found,
  not_found,
  ambiguous,
};

// A request selects a binding by its exact interface. Only when no binding has
// the exact interface does it select by its normalized interface; several
// bindings of the exact interface make the request ambiguous.
constexpr bool selects_normalized_interface(binding_status exact) {
  return exact == binding_status::not_found;
}

// The selection rules both containers select a binding of a request by: the
// static container as it compiles, over its bindings, the runtime container
// as it resolves, over its lookups.

// The status of a selection of the bindings that answer a request: one is
// found, none is not found, and more are ambiguous.
constexpr binding_status status_of_count(std::size_t count) {
  return count == 0   ? binding_status::not_found
         : count == 1 ? binding_status::found
                      : binding_status::ambiguous;
}

// The selection of a request: by the bindings of its exact interface, or,
// when its exact interface has none, by those of its normalized interface.
// find selects the bindings of an interface; normalizes tells whether the
// normalized interface is another one.
template <typename Find, typename Exact, typename Normalized>
constexpr auto select_exact_or_normalized(Find &&find, Exact exact,
                                          Normalized normalized,
                                          bool normalizes) {
  auto selection = find(exact);
  if (normalizes && selects_normalized_interface(selection.status)) {
    selection = find(normalized);
  }
  return selection;
}

// Whether a level of local bindings that selected a request with a status
// falls back to the next level: only when it binds nothing. A level that binds
// a request ambiguously answers it so.
constexpr bool falls_back(binding_status status) {
  return status == binding_status::not_found;
}

template <binding_status Status, typename Binding = void>
struct binding_choice {
  static constexpr binding_status status = Status;
  using binding_type = Binding;
};

template <typename Binding>
using found_binding_choice_t = binding_choice<binding_status::found, Binding>;

using missing_binding_choice_t = binding_choice<binding_status::not_found>;

using ambiguous_binding_choice_t = binding_choice<binding_status::ambiguous>;

template <typename Bindings> struct static_binding_choice;

template <> struct static_binding_choice<type_list<>> {
  using type = missing_binding_choice_t;
};

template <typename Binding> struct static_binding_choice<type_list<Binding>> {
  using type = found_binding_choice_t<Binding>;
};

template <typename Binding0, typename Binding1, typename... Bindings>
struct static_binding_choice<type_list<Binding0, Binding1, Bindings...>> {
  using type = ambiguous_binding_choice_t;
};

template <typename Bindings>
using static_binding_choice_t = typename static_binding_choice<Bindings>::type;

template <typename Binding> struct runtime_binding_selection {
  binding_status status = binding_status::not_found;
  Binding *binding = nullptr;

  static constexpr runtime_binding_selection found(Binding &binding) {
    return {binding_status::found, &binding};
  }

  static constexpr runtime_binding_selection miss() { return {}; }

  static constexpr runtime_binding_selection ambiguity() {
    return {binding_status::ambiguous, nullptr};
  }
};

} // namespace detail
} // namespace dingo
