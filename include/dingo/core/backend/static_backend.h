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

#include <dingo/core/backend/diagnose.h>
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

// The publications of two-phase slots are logged only by a container with a
// two-phase binding.
template <bool Publishes> struct static_publications {
  publication_log publications;
};

template <> struct static_publications<false> {};

template <std::size_t... Indices, typename... Bindings>
struct DINGO_EMPTY_BASES
    static_holders<std::index_sequence<Indices...>, Bindings...>
    : static_holder<Indices, Bindings>...,
      static_retention<(Bindings::consumes || ...)>,
      static_publications<(
          (info(Bindings::scope).storage_slot == slot::two_phase) || ...)> {
  template <typename Instances>
  explicit static_holders(Instances &instances)
      : static_holder<Indices, Bindings>(
            instances,
            std::integral_constant<std::size_t, instance_ordinal<Bindings...>(
                                                    Indices)>())... {}
};

inline constexpr std::size_t not_found = static_cast<std::size_t>(-1);
inline constexpr std::size_t ambiguous = static_cast<std::size_t>(-2);

// Whether two keys known at compile time have equal values: keys without a
// value are equal within their domain, and fixed keys compare their values.
template <typename Left, typename Right> constexpr bool same_fixed_key() {
  if constexpr (has_value(Left::descriptor.kind) &&
                has_value(Right::descriptor.kind)) {
    if constexpr (std::is_same_v<typename Left::value_type,
                                 typename Right::value_type>) {
      return Left::value() == Right::value();
    } else {
      return false;
    }
  } else {
    return true;
  }
}

// Whether two bindings hold a key of a common interface, so that a container
// of both is rejected.
template <typename... Bindings> struct duplicates {
  template <std::size_t... Indices>
  static constexpr bool any(std::index_sequence<Indices...> /*indices*/) {
    return (row<Indices>(std::index_sequence_for<Bindings...>()) || ...);
  }

  template <std::size_t Index, std::size_t... Others>
  static constexpr bool row(std::index_sequence<Others...> /*others*/) {
    return (
        (Others > Index &&
         share_domain(pack_at_t<Index, Bindings...>::descriptor,
                      pack_at_t<Others, Bindings...>::descriptor) &&
         same_fixed_key<typename pack_at_t<Index, Bindings...>::key_type,
                        typename pack_at_t<Others, Bindings...>::key_type>()) ||
        ...);
  }
};

// The index of the only binding a request by a key known at compile time
// finds, not_found, or ambiguous.
template <typename Key, typename... Bindings>
constexpr std::size_t find_binding(const request_descriptor &request) {
  constexpr binding_descriptor descriptors[] = {Bindings::descriptor...};
  constexpr bool values[] = {
      same_fixed_key<typename Bindings::key_type, Key>()...};
  std::size_t found = not_found;
  for (std::size_t index = 0; index < sizeof...(Bindings); ++index) {
    if (finds(descriptors[index], request) && values[index]) {
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
  static_assert(!detail::duplicates<Bindings...>::any(
                    std::index_sequence_for<Bindings...>()),
                "a static container holds one binding of a key of an "
                "interface: lookup already registered");

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

  // The publications of the two-phase slots in progress.
  publication_log &publications() { return holders_.publications; }

  // Resolves a request; a keyed request carries its key, which is known at
  // compile time.
  template <typename Request> Request resolve() {
    if constexpr (is_keyed<Request>::value) {
      return Request(
          resolve_by<typename Request::type, typename Request::key>());
    } else {
      return resolve_by<Request, no_key>();
    }
  }

  // Resolves a request by a key value supplied at runtime, compared with the
  // fixed keys of the bindings of its domain when it is resolved.
  template <typename Request, typename Value> Request resolve(Value &&value) {
    using runtime = runtime_key_of<std::decay_t<Value>>;
    return resolve_by_value<Request, typename runtime::type>(
        runtime::value(value), std::index_sequence_for<Bindings...>());
  }

private:
  template <typename Request, typename Key> Request resolve_by() {
    if constexpr (sizeof...(Bindings) == 0) {
      static_assert(dependent_false<Request>,
                    "static_container cannot resolve an unbound type");
    } else {
      constexpr request_descriptor request =
          request_traits<Request, Key>::descriptor;
      constexpr std::size_t index =
          detail::find_binding<Key, Bindings...>(request);
      static_assert(index != detail::not_found,
                    "static_container cannot resolve an unbound type");
      static_assert(
          index != detail::ambiguous,
          "static_container cannot resolve an ambiguously bound type");
      return resolve_from<Request, index>();
    }
  }

  template <typename Request, typename Key, typename Value,
            std::size_t... Indices>
  Request resolve_by_value(const Value &value,
                           std::index_sequence<Indices...> /*indices*/) {
    constexpr request_descriptor request =
        request_traits<Request, Key>::descriptor;
    std::size_t found = detail::not_found;
    std::size_t count = 0;
    (
        [&] {
          using binding = pack_at_t<Indices, Bindings...>;
          if constexpr (finds(binding::descriptor, request)) {
            if (key_store<typename binding::key_type>::equal(
                    key_store<typename binding::key_type>().value(), &value)) {
              found = Indices;
              ++count;
            }
          }
        }(),
        ...);
    if (count == 0) {
      raise_not_found<Request, Key>();
    }
    if (count > 1) {
      raise_ambiguous<Request>();
    }
    return resolve_at<Request, Key>(found);
  }

  // Resolves the request from the binding at the runtime index `index`, one
  // the request finds.
  template <typename Request, typename Key, std::size_t Index = 0>
  Request resolve_at(std::size_t index) {
    using binding = pack_at_t<Index, Bindings...>;
    if constexpr (finds(binding::descriptor,
                        request_traits<Request, Key>::descriptor)) {
      if (index == Index) {
        return resolve_from<Request, Index>();
      }
    }
    if constexpr (Index + 1 < sizeof...(Bindings)) {
      return resolve_at<Request, Key, Index + 1>(index);
    } else {
      raise_not_found<Request, Key>();
    }
  }

  template <typename Request, std::size_t Index> Request resolve_from() {
    using binding = pack_at_t<Index, Bindings...>;
    constexpr std::size_t selected =
        select_term(binding::published, request_traits<Request>::descriptor,
                    binding::exposed);
    static_assert(selected != no_term,
                  "static resolution cannot satisfy a request the storage "
                  "does not publish");
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
