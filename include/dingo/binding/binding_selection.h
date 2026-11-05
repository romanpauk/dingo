//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/base/type_list.h>

#include <type_traits>

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

template <typename Binding> struct runtime_binding_selection {
  binding_status status = binding_status::not_found;
  Binding *binding = nullptr;

  constexpr bool found() const { return status == binding_status::found; }

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
