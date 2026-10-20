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
#include <dingo/core/backend/local.h>
#include <dingo/core/backend/slot.h>
#include <dingo/core/base/config.h>
#include <dingo/core/base/mechanics.h>
#include <dingo/core/engine/derive.h>
#include <dingo/core/engine/execute.h>
#include <dingo/core/engine/lookup.h>
#include <dingo/core/engine/request.h>
#include <dingo/core/primitives/retained.h>
#include <dingo/core/projection/collection.h>
#include <dingo/core/vocabulary/vocabulary.gen.h>

#include <cstddef>
#include <tuple>
#include <type_traits>
#include <utility>

namespace dingo::core {

namespace detail {
// The local static bindings of a binding, nothing without any.
template <typename Locals> struct static_locals {
  local_static_backend_t<Locals> locals;
};

template <> struct static_locals<type_list<>> {};

// The slot of a binding. A binding whose scope needs an instance takes the
// instance at its position among such bindings, `Ordinal`.
template <std::size_t Index, typename Binding,
          bool Instance = info(Binding::scope).needs_instance>
struct DINGO_EMPTY_BASES static_holder
    : static_locals<typename Binding::locals> {
  template <typename Instances, std::size_t Ordinal>
  static_holder(Instances & /*instances*/,
                std::integral_constant<std::size_t, Ordinal> /*ordinal*/) {}

  binding_slot<Binding> slot;
};

template <std::size_t Index, typename Binding>
struct DINGO_EMPTY_BASES static_holder<Index, Binding, true>
    : static_locals<typename Binding::locals> {
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

// The transaction of a container is logged only when a binding constructs an
// object it keeps, in a lazy or a two-phase slot.
template <bool Keeps> struct static_transactions {
  transaction_log log;
};

template <> struct static_transactions<false> {
  no_transaction_log log;
};

template <std::size_t... Indices, typename... Bindings>
struct DINGO_EMPTY_BASES
    static_holders<std::index_sequence<Indices...>, Bindings...>
    : static_holder<Indices, Bindings>...,
      static_transactions<(
          (info(Bindings::scope).storage_slot == slot::two_phase ||
           info(Bindings::scope).storage_slot == slot::lazy) ||
          ...)> {
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
// value are equal within their domain, and fixed keys compare their values. A
// runtime key compares its value when it is resolved, so it is equal here.
template <typename Left, typename Right> constexpr bool same_fixed_key() {
  if constexpr (Left::descriptor.kind == key_kind::fixed &&
                Right::descriptor.kind == key_kind::fixed) {
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

// Whether two bindings hold a key of a common interface whose domain holds
// one binding, so that a container of both is rejected.
template <const lookup_table &Table, typename... Bindings> struct duplicates {
  template <std::size_t... Indices>
  static constexpr bool any(std::index_sequence<Indices...> /*indices*/) {
    return (row<Indices>(std::index_sequence_for<Bindings...>()) || ...);
  }

  template <std::size_t Index, std::size_t... Others>
  static constexpr bool row(std::index_sequence<Others...> /*others*/) {
    return (
        (Others > Index &&
         conflicts(Table, pack_at_t<Index, Bindings...>::descriptor,
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
  constexpr binding_descriptor descriptors[] = {Bindings::descriptor...,
                                                binding_descriptor{}};
  constexpr bool values[] = {
      same_fixed_key<typename Bindings::key_type, Key>()..., false};
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

namespace detail {
// Whether the fixed key of a static binding, if any, is within the bound of
// the dense array of its domain.
template <const lookup_table &Table, typename Binding>
constexpr bool within_bound() {
  using key = typename Binding::key_type;
  if constexpr (key::descriptor.kind == key_kind::fixed) {
    constexpr std::size_t bound = key_bound(Table, Binding::descriptor);
    return bound == unbounded || key_within(key_store<key>::constant, bound);
  } else {
    return true;
  }
}
} // namespace detail

// A backend of the bindings `Bindings`, with the lookup declarations of the
// table `LookupTable::value`.
template <typename LookupTable, typename... Bindings> class static_backend {
  static_assert(((Bindings::status == registration_status::registered) && ...),
                "a binding cannot be stored by its scope");
  static_assert(!detail::duplicates<LookupTable::value, Bindings...>::any(
                    std::index_sequence_for<Bindings...>()),
                "a static container holds one binding of a key of an "
                "interface: lookup already registered");
  static_assert((detail::within_bound<LookupTable::value, Bindings>() && ...),
                "type index out of range: the key of a static binding is "
                "outside the bound of the dense array of its domain");

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

  // The transaction of the requests of the container in progress.
  auto &transactions() { return holders_.log; }

  // Whether a static binding holds the key `key_value` of a binding of type
  // `Binding` added at runtime, of a common interface whose domain holds one
  // binding.
  template <typename Binding> static bool holds_key_of(const void *key_value) {
    return holds_key_of_in<Binding>(LookupTable::value, key_value);
  }

  // As holds_key_of, by the lookup declarations of `table`.
  template <typename Binding>
  static bool holds_key_of_in(const lookup_table &table,
                              const void *key_value) {
    return holds_key_of_at<Binding>(table, key_value,
                                    std::index_sequence_for<Bindings...>());
  }

  // The number of bindings a request by `Key`, a key known when the request
  // is compiled, names: 0, 1, or 2 for many.
  template <typename Request, typename Key>
  static constexpr std::size_t fixed_count() {
    [[maybe_unused]] constexpr request_descriptor request =
        request_traits<Request, Key>::descriptor;
    constexpr bool named[] = {
        (finds(Bindings::descriptor, request) &&
         detail::same_fixed_key<typename Bindings::key_type, Key>())...,
        false};
    std::size_t found = 0;
    for (std::size_t index = 0; index < sizeof...(Bindings); ++index) {
      found += named[index] ? 1 : 0;
    }
    return found < 2 ? found : 2;
  }

  // The number of bindings a request by `Key` names, whose value a runtime
  // key compares when it is resolved: 0, 1, or 2 for many.
  template <typename Request, typename Key>
  std::size_t count(const void *key_value) const {
    if constexpr (Key::descriptor.kind != key_kind::runtime) {
      return fixed_count<Request, Key>();
    } else {
      std::size_t found = 0;
      each_named<Request, Key>(key_value,
                               [&found](auto /*index*/) { ++found; });
      return found < 2 ? found : 2;
    }
  }

  // Resolves a request from the one binding it names.
  template <typename Request, typename Key, typename Resolver>
  Request resolve_named(Resolver &resolver, const void *key_value) {
    if constexpr (Key::descriptor.kind != key_kind::runtime) {
      constexpr std::size_t index = detail::find_binding<Key, Bindings...>(
          request_traits<Request, Key>::descriptor);
      static_assert(index != detail::not_found && index != detail::ambiguous,
                    "a request resolves from the one binding it names");
      return resolve_from<Request, Key, index>(resolver);
    } else if constexpr (sizeof...(Bindings) == 0) {
      raise_not_found<Request, Key>();
    } else {
      std::size_t found = detail::not_found;
      each_named<Request, Key>(key_value,
                               [&found](auto index) { found = index; });
      return resolve_at<Request, Key>(resolver, found);
    }
  }

  // Adds every binding a request of the element type of `Collection` by
  // `Key` names to `collection`, in the order of the bindings.
  template <typename Collection, typename Key, typename Resolver,
            typename Append>
  void collect(Resolver &resolver, Collection &collection,
               const void *key_value, Append &append) {
    using element = typename collection_traits<Collection>::resolve_type;
    each_named<element, Key>(key_value, [&](auto index) {
      append(collection,
             resolve_from<element, Key, decltype(index)::value>(resolver));
    });
  }

private:
  template <typename Binding, std::size_t... Indices>
  static bool holds_key_of_at([[maybe_unused]] const lookup_table &table,
                              [[maybe_unused]] const void *key_value,
                              std::index_sequence<Indices...> /*indices*/) {
    bool held = false;
    (
        [&] {
          using binding = pack_at_t<Indices, Bindings...>;
          using key_type = typename binding::key_type;
          if constexpr (same_key_domain(binding::descriptor.key,
                                        Binding::descriptor.key)) {
            held =
                held ||
                (conflicts(table, binding::descriptor, Binding::descriptor) &&
                 key_store<key_type>::equal(key_store<key_type>().value(),
                                            key_value));
          }
        }(),
        ...);
    return held;
  }

  // Calls `visit` with the index, as an integral_constant, of every binding a
  // request by `Key` names: by its domain and its fixed key when the request
  // is compiled, and by the value of a runtime key when it is resolved.
  template <typename Request, typename Key, typename Visit>
  void each_named(const void *key_value, Visit &&visit) const {
    each_named_at<Request, Key>(key_value, visit,
                                std::index_sequence_for<Bindings...>());
  }

  template <typename Request, typename Key, typename Visit,
            std::size_t... Indices>
  static void each_named_at([[maybe_unused]] const void *key_value,
                            [[maybe_unused]] Visit &visit,
                            std::index_sequence<Indices...> /*indices*/) {
    [[maybe_unused]] constexpr request_descriptor request =
        request_traits<Request, Key>::descriptor;
    (
        [&] {
          using binding = pack_at_t<Indices, Bindings...>;
          using key_type = typename binding::key_type;
          if constexpr (finds(binding::descriptor, request) &&
                        detail::same_fixed_key<key_type, Key>()) {
            if (Key::descriptor.kind != key_kind::runtime ||
                key_store<key_type>::equal(key_store<key_type>().value(),
                                           key_value)) {
              visit(std::integral_constant<std::size_t, Indices>());
            }
          }
        }(),
        ...);
  }

  // Resolves the request from the binding at the runtime index `index`, one
  // the request names.
  template <typename Request, typename Key, typename Resolver,
            std::size_t Index = 0>
  Request resolve_at(Resolver &resolver, std::size_t index) {
    using binding = pack_at_t<Index, Bindings...>;
    if constexpr (finds(binding::descriptor,
                        request_traits<Request, Key>::descriptor)) {
      if (index == Index) {
        return resolve_from<Request, Key, Index>(resolver);
      }
    }
    if constexpr (Index + 1 < sizeof...(Bindings)) {
      return resolve_at<Request, Key, Resolver, Index + 1>(resolver, index);
    } else {
      raise_not_found<Request, Key>();
    }
  }

  // The term is selected by the descriptor of the request by `Key`, which
  // carries the annotation of an annotated request.
  template <typename Request, typename Key, std::size_t Index,
            typename Resolver>
  Request resolve_from(Resolver &resolver) {
    using binding = pack_at_t<Index, Bindings...>;
    constexpr std::size_t selected =
        select_term(binding::published,
                    request_traits<Request, Key>::descriptor, binding::exposed);
    static_assert(selected != no_term,
                  "static resolution cannot satisfy a request the storage "
                  "does not publish");
    static_assert(walk_supported_v<binding, selected>,
                  "the next implementation does not provide the route of the "
                  "requested type yet");
    using value_type = typename request_traits<Request>::value_type;
    using made_type = typename request_traits<Request>::made_type;
    constexpr const completion_plan &plan = plan_v<binding, selected>;
    auto &holder =
        static_cast<detail::static_holder<Index, binding> &>(holders_);
    auto source = [&]() -> decltype(auto) {
      if constexpr (std::is_same_v<typename binding::locals, type_list<>>) {
        return holder.slot.get(resolver);
      } else {
        // The object resolves its dependencies through its local bindings.
        local_view<Resolver, typename binding::locals> view(
            resolver, &holder.locals, nullptr);
        return holder.slot.get(view);
      }
    };
    auto make = [&]() -> decltype(auto) {
      if constexpr (plan.created == 0) {
        return walk_from<binding, selected,
                         binding::published.items[selected].rule_count>(
            holder.slot.context(), source);
      } else {
        using layers = planned_layers_t<made_type, binding, selected>;
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
      value_type &kept = resolver.retained().template emplace<value_type>(
          [&] { return value_type(make()); });
      resolver.transactions().record(&resolver.retained(),
                                     &retained_objects::undo_last);
      return static_cast<Request>(kept);
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
