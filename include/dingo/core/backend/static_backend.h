//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

// The static backend: the bindings are known when the container type is
// formed, the engine selects the binding and the term of a request at compile
// time, and the term is walked and completed directly.

#include <dingo/core/backend/slot.h>
#include <dingo/core/base/config.h>
#include <dingo/core/base/mechanics.h>
#include <dingo/core/engine/derive.h>
#include <dingo/core/engine/execute.h>
#include <dingo/core/engine/request.h>
#include <dingo/core/primitives/retained.h>
#include <dingo/core/vocabulary/vocabulary.gen.h>

#include <cstddef>
#include <tuple>
#include <type_traits>
#include <utility>

namespace dingo::core {

namespace detail {
// The slot of a binding. A binding whose scope needs an instance takes the
// instance at its position among such bindings, `Ordinal`.
template <std::size_t Index, typename Binding,
          bool Instance = info(Binding::scope).needs_instance>
struct static_holder {
  template <typename Instances, std::size_t Ordinal>
  static_holder(Instances & /*instances*/,
                std::integral_constant<std::size_t, Ordinal> /*ordinal*/) {}

  binding_slot<Binding> slot;
};

template <std::size_t Index, typename Binding>
struct static_holder<Index, Binding, true> {
  template <typename Instances, std::size_t Ordinal>
  static_holder(Instances &instances,
                std::integral_constant<std::size_t, Ordinal> /*ordinal*/)
      : slot(std::get<Ordinal>(std::move(instances))) {}

  binding_slot<Binding> slot;
};

// The number of bindings before `Index` whose scope needs an instance.
template <typename... Bindings>
constexpr std::size_t instance_ordinal(std::size_t index) {
  constexpr bool needs[] = {info(Bindings::scope).needs_instance..., false};
  std::size_t ordinal = 0;
  for (std::size_t before = 0; before < index; ++before) {
    ordinal += needs[before] ? 1 : 0;
  }
  return ordinal;
}

template <typename Indices, typename... Bindings> struct static_holders;

// The objects a request of an rvalue reference constructs are retained only by
// a container with a consuming binding.
template <bool Retains> struct static_retention {
  retained_objects retained;
};

template <> struct static_retention<false> {};

template <std::size_t... Indices, typename... Bindings>
struct DINGO_EMPTY_BASES
    static_holders<std::index_sequence<Indices...>, Bindings...>
    : static_holder<Indices, Bindings>...,
      static_retention<(Bindings::consumes || ...)> {
  template <typename Instances>
  explicit static_holders(Instances &instances)
      : static_holder<Indices, Bindings>(
            instances,
            std::integral_constant<std::size_t, instance_ordinal<Bindings...>(
                                                    Indices)>())... {}
};

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
  // Takes the instance of every binding whose scope needs one, in the order of
  // the bindings.
  template <typename... Instances>
  explicit static_backend(Instances &&...instances)
      : static_backend(
            std::forward_as_tuple(std::forward<Instances>(instances)...), 0) {
    static_assert(
        sizeof...(Instances) ==
            (std::size_t(info(Bindings::scope).needs_instance) + ... + 0),
        "a static container takes the instance of every external "
        "binding, in the order of the bindings");
  }

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
    using value_type = typename request_traits<Request>::value_type;
    constexpr const completion_plan &plan = plan_v<binding, selected>;
    auto &holder =
        static_cast<detail::static_holder<Index, binding> &>(holders_);
    auto source = [&]() -> decltype(auto) { return holder.slot.get(*this); };
    auto make = [&]() -> decltype(auto) {
      if constexpr (plan.created == 0) {
        return walk_from<binding, selected,
                         binding::published.items[selected].rule_count>(
            holder.slot.context(), source);
      } else {
        using layers = planned_layers_t<value_type, binding, selected>;
        static_assert(layers::valid &&
                          std::is_same_v<typename layers::boundary,
                                         boundary_t<binding, selected>>,
                      "the requested wrapper cannot be created by the route");
        return layers::make([&]() -> decltype(auto) {
          return walk_to_boundary<binding, selected>(holder.slot.context(),
                                                     source);
        });
      }
    };
    if constexpr (std::is_rvalue_reference_v<Request> &&
                  binding::published.items[selected].category ==
                      value_category::rvalue) {
      return static_cast<Request>(
          holders_.retained.template emplace<value_type>(
              [&] { return value_type(make()); }));
    } else {
      return static_cast<Request>(make());
    }
  }

  template <typename Instances>
  static_backend(Instances &&instances, int /*delegated*/)
      : holders_(instances) {}

  detail::static_holders<std::index_sequence_for<Bindings...>, Bindings...>
      holders_;
};

} // namespace dingo::core
