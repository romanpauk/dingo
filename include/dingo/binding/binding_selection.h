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

template <binding_status Status, typename Bindings>
struct static_binding_result;

template <typename... Bindings>
struct static_binding_result<binding_status::not_found,
                             type_list<Bindings...>> {
  using type = missing_binding_choice_t;
};

template <typename Binding, typename... Bindings>
struct static_binding_result<binding_status::found,
                             type_list<Binding, Bindings...>> {
  using type = found_binding_choice_t<Binding>;
};

template <typename... Bindings>
struct static_binding_result<binding_status::ambiguous,
                             type_list<Bindings...>> {
  using type = ambiguous_binding_choice_t;
};

template <typename Bindings> struct static_binding;

template <typename... Bindings>
struct static_binding<type_list<Bindings...>>
    : static_binding_result<selection_status_from_count(sizeof...(Bindings)),
                            type_list<Bindings...>> {};

template <typename Bindings>
using static_binding_t = typename static_binding<Bindings>::type;

template <typename Candidate> struct selection_accumulator {
  binding_status status = binding_status::not_found;
  Candidate candidate{};
  std::size_t matches = 0;

  constexpr void add(Candidate value) {
    ++matches;
    if (matches == 1) {
      candidate = value;
      status = binding_status::found;
    } else {
      status = binding_status::ambiguous;
    }
  }

  constexpr binding_status result() const {
    return selection_status_from_count(matches);
  }
};

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

// Resolves a runtime selection. A selection that matched several bindings is
// ambiguous in every container; a selection that matched none is handed to the
// missing callback, which owns parent fallback and auto-construction.
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

template <typename Binding, typename State = std::nullptr_t, typename Visitor>
constexpr runtime_binding_selection<Binding, State>
make_runtime_selection(Visitor &&visit_candidates) {
  selection_accumulator<runtime_binding_selection<Binding, State>> accumulator;

  std::forward<Visitor>(visit_candidates)(
      [&](Binding &binding, State state = nullptr) {
        accumulator.add(
            runtime_binding_selection<Binding, State>::found(binding, state));
      });

  switch (accumulator.result()) {
  case binding_status::not_found:
    return runtime_binding_selection<Binding, State>::miss();
  case binding_status::found:
    return accumulator.candidate;
  case binding_status::ambiguous:
    return runtime_binding_selection<Binding, State>::ambiguity();
  }
  return runtime_binding_selection<Binding, State>::miss();
}

} // namespace detail
} // namespace dingo
