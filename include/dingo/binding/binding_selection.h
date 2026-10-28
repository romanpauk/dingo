//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/base/exceptions.h>
#include <dingo/base/type_list.h>

#include <cstddef>
#include <type_traits>
#include <utility>

namespace dingo {
namespace detail {

enum class binding_status {
  found,
  not_found,
  ambiguous,
};

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

// One ambiguity/first-match rule for every candidate set. A static candidate
// set supplies the type-level result; the runtime visitor supplies pointer
// candidates.
constexpr binding_status selection_status_from_count(std::size_t count) {
  return count == 0   ? binding_status::not_found
         : count == 1 ? binding_status::found
                      : binding_status::ambiguous;
}

template <typename Bindings> struct static_binding;

template <> struct static_binding<type_list<>> {
  using type = missing_binding_choice_t;
};

template <typename Binding, typename... Bindings>
struct static_binding<type_list<Binding, Bindings...>> {
  using type = std::conditional_t<sizeof...(Bindings) == 0,
                                  found_binding_choice_t<Binding>,
                                  ambiguous_binding_choice_t>;
};

template <typename Bindings>
using static_binding_t = typename static_binding<Bindings>::type;

// The runtime answer: the selected binding and its status. The caller owns
// everything else about the candidate.
template <typename Binding> struct runtime_binding_selection {
  binding_status status = binding_status::not_found;
  Binding *binding = nullptr;

  constexpr bool found() const { return status == binding_status::found; }

  constexpr bool ambiguous() const {
    return status == binding_status::ambiguous;
  }

  static constexpr runtime_binding_selection found(Binding &binding) {
    return {binding_status::found, &binding};
  }

  static constexpr runtime_binding_selection miss() { return {}; }

  static constexpr runtime_binding_selection ambiguity() {
    return {binding_status::ambiguous, nullptr};
  }
};

template <typename Binding>
constexpr runtime_binding_selection<Binding>
make_runtime_selection(Binding *binding) {
  return binding ? runtime_binding_selection<Binding>::found(*binding)
                 : runtime_binding_selection<Binding>::miss();
}

// Resolves a runtime selection. A selection that matched several bindings is
// ambiguous in every container; a selection that matched none is handed to the
// missing callback, which owns parent fallback and auto-construction.
template <typename R, typename Request, typename Binding, typename Context,
          typename Found, typename Missing>
R resolve_selection(const runtime_binding_selection<Binding> &selection,
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
