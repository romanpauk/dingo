//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

// The static backend: the bindings are known when the container type is
// formed, the engine selects the binding and the term of a request at compile
// time, and the term is walked directly.

#include <dingo/core/backend/slot.h>
#include <dingo/core/base/config.h>
#include <dingo/core/base/mechanics.h>
#include <dingo/core/engine/derive.h>
#include <dingo/core/engine/execute.h>
#include <dingo/core/engine/request.h>

#include <cstddef>
#include <utility>

namespace dingo::core {

namespace detail {
template <std::size_t Index, typename Binding> struct static_holder {
  binding_slot<Binding> slot;
};

template <typename Indices, typename... Bindings> struct static_holders;

template <std::size_t... Indices, typename... Bindings>
struct DINGO_EMPTY_BASES
    static_holders<std::index_sequence<Indices...>, Bindings...>
    : static_holder<Indices, Bindings>... {};

inline constexpr std::size_t not_found = static_cast<std::size_t>(-1);
inline constexpr std::size_t ambiguous = static_cast<std::size_t>(-2);

// The index of the only binding a request finds, not_found, or ambiguous.
template <typename... Bindings>
constexpr std::size_t find_binding(const request_descriptor &request) {
  constexpr binding_descriptor descriptors[] = {Bindings::descriptor...};
  std::size_t found = not_found;
  for (std::size_t index = 0; index < sizeof...(Bindings); ++index) {
    if (finds(descriptors[index], request)) {
      if (found != not_found) {
        return ambiguous;
      }
      found = index;
    }
  }
  return found;
}
} // namespace detail

template <typename... Bindings> class static_backend {
  static_assert(((Bindings::status == registration_status::registered) && ...),
                "a binding cannot be stored by its scope");

public:
  template <typename Request> Request resolve() {
    if constexpr (sizeof...(Bindings) == 0) {
      static_assert(dependent_false<Request>,
                    "static_container cannot resolve an unbound type");
    } else {
      constexpr request_descriptor request =
          request_traits<Request>::descriptor;
      constexpr std::size_t index = detail::find_binding<Bindings...>(request);
      static_assert(index != detail::not_found,
                    "static_container cannot resolve an unbound type");
      static_assert(index != detail::ambiguous,
                    "static_container resolves an ambiguous type");
      return resolve_from<Request, index>();
    }
  }

private:
  template <typename Request, std::size_t Index> Request resolve_from() {
    using binding = pack_at_t<Index, Bindings...>;
    constexpr std::size_t selected =
        select_term(binding::published, request_traits<Request>::descriptor,
                    binding::exposed);
    static_assert(selected != no_term,
                  "the binding cannot serve the requested type");
    static_assert(walk_supported_v<binding, selected>,
                  "the next implementation does not provide the route of the "
                  "requested type yet");
    auto &holder =
        static_cast<detail::static_holder<Index, binding> &>(holders_);
    return static_cast<Request>(
        walk<binding, selected>(holder.slot.get(*this)));
  }

  detail::static_holders<std::index_sequence_for<Bindings...>, Bindings...>
      holders_;
};

} // namespace dingo::core
