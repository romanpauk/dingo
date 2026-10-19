//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/base/exceptions.h>
#include <dingo/binding/binding_selection.h>

#include <cstddef>
#include <utility>

namespace dingo {
namespace detail {

template <typename Binding, typename State = std::nullptr_t>
struct runtime_binding_selection {
  binding_status status = binding_status::not_found;
  Binding *binding = nullptr;
  State state = nullptr;

  constexpr bool found() const { return status == binding_status::found; }

  constexpr bool ambiguous() const {
    return status == binding_status::ambiguous;
  }

  static constexpr runtime_binding_selection found(Binding &binding,
                                                   State state = nullptr) {
    return {binding_status::found, &binding, state};
  }

  static constexpr runtime_binding_selection miss() { return {}; }

  static constexpr runtime_binding_selection ambiguity() {
    return {binding_status::ambiguous, nullptr, nullptr};
  }
};

template <typename Binding, typename State = std::nullptr_t>
constexpr runtime_binding_selection<Binding, State>
make_runtime_selection(Binding *binding, State state = nullptr) {
  return binding
             ? runtime_binding_selection<Binding, State>::found(*binding, state)
             : runtime_binding_selection<Binding, State>::miss();
}

template <typename Binding, typename State = std::nullptr_t, typename Visitor>
constexpr runtime_binding_selection<Binding, State>
make_runtime_selection(Visitor &&visit_candidates) {
  Binding *selected_binding = nullptr;
  State selected_state = nullptr;
  std::size_t matches = 0;

  std::forward<Visitor>(visit_candidates)(
      [&](Binding &binding, State state = nullptr) {
        ++matches;
        if (matches == 1) {
          selected_binding = &binding;
          selected_state = state;
        }
      });

  if (matches == 0) {
    return runtime_binding_selection<Binding, State>::miss();
  }

  if (matches == 1) {
    return runtime_binding_selection<Binding, State>::found(*selected_binding,
                                                            selected_state);
  }

  return runtime_binding_selection<Binding, State>::ambiguity();
}

// Dispatches on the outcome of a lookup: an ambiguous lookup is an error, a
// found binding is resolved by found and a missing one is handled by missing.
// The static side knows the outcome at compile time and branches with
// if constexpr on binding_choice::status instead.
template <typename R, typename Request, typename Binding, typename State,
          typename Context, typename Found, typename Missing>
R resolve_selection(const runtime_binding_selection<Binding, State> &selection,
                    Context &context, Found &&found, Missing &&missing) {
  if (selection.ambiguous()) {
    throw make_type_ambiguous_exception<Request>(context);
  }
  if (selection.found()) {
    return std::forward<Found>(found)();
  }
  return std::forward<Missing>(missing)();
}

} // namespace detail
} // namespace dingo
