//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

// The static backend: the bindings are known when the container type is
// formed, the engine selects the binding and the reachable type of a request at
// compile time, and the reachable type is walked and completed directly.

#include <dingo/backend/local.h>
#include <dingo/backend/slot.h>
#include <dingo/base/config.h>
#include <dingo/base/mechanics.h>
#include <dingo/conversion/binding_conflicts.h>
#include <dingo/conversion/convert.h>
#include <dingo/conversion/route.h>
#include <dingo/diagnostics/diagnostic_message.h>
#include <dingo/lookup/annotated_key.h>
#include <dingo/lookup/declarations.h>
#include <dingo/lookup/key_kinds.gen.h>
#include <dingo/memory/retained.h>
#include <dingo/selection/diagnose.h>
#include <dingo/selection/request.h>
#include <dingo/storage/scopes.gen.h>
#include <dingo/type/annotated.h>
#include <dingo/type/collection_traits.h>

#include <cstddef>
#include <tuple>
#include <type_traits>
#include <utility>

namespace dingo::core {

namespace detail {
// The route a request of `Request` by `Key` takes of `Binding`, decided when
// it is compiled, the plan by which the request completes it, and the value
// category of the object the route ends in.
template <typename Binding, typename Request, typename Key>
struct static_route {
  static constexpr route_choice choice =
      select_route(Binding::descriptor, Binding::published,
                   request_traits<Request, Key>::descriptor, Binding::exposed);
  static constexpr request_plan value =
      choice.reachable_type == no_reachable_type
          ? request_plan{}
          : plan_request(plan_completion(
                             Binding::published.items[choice.reachable_type]),
                         choice);
  static constexpr value_category category =
      choice.reachable_type == no_reachable_type ? value_category::prvalue
      : choice.created_count == 0
          ? Binding::published.items[choice.reachable_type].category
          : info(choice.created[choice.created_count - 1].form).to_category;
};

// The local static bindings of a binding, nothing without any.
template <typename Locals> struct static_locals {
  local_static_backend_t<Locals> locals;
};

template <> struct static_locals<type_list<>> {};

// The slot of a binding. A binding whose scope needs an instance takes the
// instance at its position among such bindings, `Ordinal`.
template <std::size_t Index, typename Binding,
          bool Instance = Binding::scope.needs_instance>
struct DINGO_EMPTY_BASES static_holder
    : static_locals<typename Binding::locals> {
  template <typename Instances, std::size_t Ordinal>
  static_holder(Instances & /*instances*/,
                std::integral_constant<std::size_t, Ordinal> /*ordinal*/) {}
  static_holder(static_holder &&) = default;
  static_holder &operator=(static_holder &&) = default;
  DINGO_ALWAYS_INLINE ~static_holder() = default;

  slot_of_t<Binding> slot;
};

template <std::size_t Index, typename Binding>
struct DINGO_EMPTY_BASES static_holder<Index, Binding, true>
    : static_locals<typename Binding::locals> {
  template <typename Instances, std::size_t Ordinal>
  static_holder(Instances &instances,
                std::integral_constant<std::size_t, Ordinal> /*ordinal*/)
      : slot(std::get<Ordinal>(std::move(instances))) {}
  static_holder(static_holder &&) = default;
  static_holder &operator=(static_holder &&) = default;
  DINGO_ALWAYS_INLINE ~static_holder() = default;

  slot_of_t<Binding> slot;
};

// The number of bindings before `Index` whose scope needs an instance.
template <typename... Bindings>
constexpr std::size_t instance_ordinal(std::size_t index) {
  constexpr bool needs[] = {Bindings::scope.needs_instance..., false};
  std::size_t ordinal = 0;
  for (std::size_t before = 0; before < index; ++before) {
    ordinal += needs[before] ? 1 : 0;
  }
  return ordinal;
}

template <typename Indices, typename... Bindings> struct static_holders;

// The transaction of a container is logged only when a binding constructs an
// object it keeps, in a lazy or a two-phase slot.
// The transaction of a static container, whose `Slots` slots that construct
// an object are each recorded once in a transaction, and its retained objects
// once: a transaction of a few slots records them in its frame and never
// grows. A slot class a scope names records what it records, so a container
// with one has a log that grows.
template <std::size_t Slots, bool Custom = false> struct static_transactions {
  static constexpr std::size_t bound = Slots + 1;
  basic_transaction_log<(bound < 8 ? bound : 8), (Custom || bound > 8)> log;
};

// A container of no slot that constructs an object keeps nothing to undo, and
// its empty log adds nothing to its size.
template <> struct static_transactions<0, false> {
  static inline no_transaction_log log;
};

template <std::size_t... Indices, typename... Bindings>
struct DINGO_EMPTY_BASES
    static_holders<std::index_sequence<Indices...>, Bindings...>
    : static_holder<Indices, Bindings>...,
      static_transactions<
          (std::size_t(0) + ... +
           std::size_t(Bindings::scope.storage_slot == slot::two_phase ||
                       Bindings::scope.storage_slot == slot::lazy ||
                       custom_slot_v<Bindings>)),
          (custom_slot_v<Bindings> || ...)> {
  template <typename Instances>
  explicit static_holders(Instances &instances)
      : static_holder<Indices, Bindings>(
            instances,
            std::integral_constant<std::size_t, instance_ordinal<Bindings...>(
                                                    Indices)>())... {}
  static_holders(static_holders &&) = default;
  static_holders &operator=(static_holders &&) = default;
  // The holders are destroyed with the requests that unwind, so every
  // destructor of the container is inlined.
  DINGO_ALWAYS_INLINE ~static_holders() = default;
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

  // The index of the later binding of the first two that conflict, which
  // registering them in order rejects.
  static constexpr std::size_t rejected() {
    return rejected_in(
        std::make_index_sequence<sizeof...(Bindings) * sizeof...(Bindings)>());
  }

  template <std::size_t... Indices>
  static constexpr std::size_t
  rejected_in(std::index_sequence<Indices...> /*pairs*/) {
    constexpr std::size_t count = sizeof...(Bindings);
    // Pair `index` is of the later binding index / count and the earlier
    // index % count, so the pairs are in the order of registration.
    constexpr bool pairs[] = {pair<Indices % count, Indices / count>()...,
                              false};
    for (std::size_t index = 0; index < count * count; ++index) {
      if (pairs[index]) {
        return index / count;
      }
    }
    return 0;
  }

  template <std::size_t Earlier, std::size_t Later>
  static constexpr bool pair() {
    if constexpr (Earlier >= Later) {
      return false;
    } else {
      return conflicts(Table, pack_at_t<Earlier, Bindings...>::descriptor,
                       pack_at_t<Later, Bindings...>::descriptor) &&
             same_fixed_key<typename pack_at_t<Earlier, Bindings...>::key_type,
                            typename pack_at_t<Later, Bindings...>::key_type>();
    }
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

// The dependencies of static bindings that their factories declare: an edge
// from a binding to the one each argument of its factory resolves. A binding
// with local bindings, which may answer its arguments, and an argument by a
// runtime key, whose value picks the binding, add none.
template <typename... Bindings> struct dependency_graph {
  static constexpr std::size_t count = sizeof...(Bindings);

  struct edges {
    bool from_to[count * count + 1];
  };

  // The path of a cycle, its first binding last again; empty without one.
  struct cycle {
    std::size_t length;
    std::size_t nodes[count + 1];
  };

  template <typename Argument> static constexpr std::size_t target() {
    using request = argument_request<Argument>;
    using key = typename request::key;
    if constexpr (key::descriptor.kind == key_kind::runtime) {
      return count;
    } else {
      constexpr std::size_t found = find_binding<key, Bindings...>(
          request_traits<typename request::type, key>::descriptor);
      return found < count ? found : count;
    }
  }

  template <std::size_t Index, typename... Arguments>
  static constexpr void add(edges &graph, type_list<Arguments...> /*args*/) {
    ((graph.from_to[target<Arguments>() == count
                        ? count * count
                        : Index * count + target<Arguments>()] = true),
     ...);
  }

  template <std::size_t Index> static constexpr bool declares() {
    using binding = pack_at_t<Index, Bindings...>;
    return !std::is_void_v<typename declared_arguments<
               typename binding::factory_type,
               leaf_of_t<typename binding::stored_type>>::type> &&
           std::is_same_v<typename binding::locals, type_list<>>;
  }

  template <std::size_t... Indices>
  static constexpr bool any(std::index_sequence<Indices...> /*indices*/) {
    return (declares<Indices>() || ...);
  }

  template <std::size_t Index> static constexpr void add_binding(edges &graph) {
    if constexpr (declares<Index>()) {
      using binding = pack_at_t<Index, Bindings...>;
      add<Index>(graph, typename declared_arguments<
                            typename binding::factory_type,
                            leaf_of_t<typename binding::stored_type>>::type());
    } else {
      (void)graph;
    }
  }

  template <std::size_t... Indices>
  static constexpr edges make(std::index_sequence<Indices...> /*indices*/) {
    edges graph{};
    (add_binding<Indices>(graph), ...);
    return graph;
  }

  // The shortest cycle through the first binding that is not shared_cyclical
  // and reaches itself. spec: graph_rules.cycle
  static constexpr cycle find() {
    constexpr edges graph = make(std::index_sequence_for<Bindings...>());
    constexpr bool legal[] = {
        (Bindings::scope.storage_slot == slot::two_phase)..., false};
    cycle found{};
    for (std::size_t start = 0; start < count; ++start) {
      if (legal[start]) {
        continue;
      }
      std::size_t parent[count + 1] = {};
      bool seen[count + 1] = {};
      std::size_t queue[count + 1] = {};
      std::size_t head = 0;
      std::size_t tail = 0;
      std::size_t node = start;
      do {
        for (std::size_t next = 0; next < count; ++next) {
          if (graph.from_to[node * count + next] && !seen[next]) {
            seen[next] = true;
            parent[next] = node;
            queue[tail++] = next;
          }
        }
        if (seen[start]) {
          std::size_t reversed[count + 1] = {};
          std::size_t length = 0;
          std::size_t at = start;
          do {
            reversed[length++] = at;
            at = parent[at];
          } while (at != start);
          reversed[length++] = start;
          found.length = length;
          for (std::size_t index = 0; index < length; ++index) {
            found.nodes[index] = reversed[length - 1 - index];
          }
          return found;
        }
        node = head < tail ? queue[head] : count;
        ++head;
      } while (head <= tail);
    }
    return found;
  }
};
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
  static_assert((registers<Bindings>() && ...));

public:
  using binding_list = type_list<Bindings...>;

private:
  // Reports the binding a static binding of its key rejects, decided when
  // the container is compiled.
  template <bool Unique> static constexpr bool unique() {
    if constexpr (!Unique) {
      using binding = pack_at_t<
          detail::duplicates<LookupTable::value, Bindings...>::rejected(),
          Bindings...>;
      fail<diag::lookup_already_registered, first_interface_t<binding>,
           typename binding::stored_type,
           typename detail::key_type_of<typename binding::key_type>::type>();
    }
    return true;
  }
  static_assert(
      unique<!detail::duplicates<LookupTable::value, Bindings...>::any(
          std::index_sequence_for<Bindings...>())>());

  // Reports the binding whose key is outside the bound of its domain.
  template <bool Within> static constexpr bool within() {
    if constexpr (!Within) {
      constexpr bool inside[] = {
          detail::within_bound<LookupTable::value, Bindings>()..., false};
      constexpr std::size_t index = [&] {
        std::size_t found = 0;
        while (inside[found]) {
          ++found;
        }
        return found;
      }();
      using binding = pack_at_t<index, Bindings...>;
      fail<diag::type_index_out_of_range,
           typename detail::key_type_of<typename binding::key_type>::type>();
    }
    return true;
  }
  static_assert(
      within<(detail::within_bound<LookupTable::value, Bindings>() && ...)>());

  // Reports a cycle of the dependencies the static bindings declare through a
  // binding that is not shared_cyclical, decided when the container is
  // compiled. spec: graph_rules.cycle
  using graph = detail::dependency_graph<Bindings...>;
  template <std::size_t... Steps>
  static constexpr void report_cycle(std::index_sequence<Steps...> /*steps*/) {
    constexpr typename graph::cycle found = graph::find();
    fail<diag::type_recursion,
         dependency_path<leaf_of_t<typename pack_at_t<
             found.nodes[Steps], Bindings...>::stored_type>...>>();
  }
  template <bool Declares> static constexpr bool acyclic() {
    if constexpr (Declares) {
      constexpr typename graph::cycle found = graph::find();
      if constexpr (found.length != 0) {
        report_cycle(std::make_index_sequence<found.length>());
      }
    }
    return true;
  }
  static_assert(acyclic<graph::any(std::index_sequence_for<Bindings...>())>());

public:
  // Takes the instance of every binding whose scope needs one, in the order of
  // the bindings.
  template <typename... Instances>
  explicit static_backend(Instances &&...instances)
      : static_backend(
            std::forward_as_tuple(std::forward<Instances>(instances)...), 0) {
    static_assert(
        require<sizeof...(Instances) ==
                    (std::size_t(Bindings::scope.needs_instance) + ... + 0),
                diag::static_instances>());
  }

  static_backend(static_backend &&) = default;
  static_backend &operator=(static_backend &&) = default;
  DINGO_ALWAYS_INLINE ~static_backend() = default;

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

  // Whether the one binding a request by a key known when the request is
  // compiled names serves it, a reachable type of it the request accepts.
  template <typename Request, typename Key>
  static constexpr bool serves_named() {
    constexpr std::size_t index = detail::find_binding<Key, Bindings...>(
        request_traits<Request, Key>::descriptor);
    if constexpr (index == detail::not_found || index == detail::ambiguous) {
      return false;
    } else {
      using binding = pack_at_t<index, Bindings...>;
      return detail::static_route<binding, Request, Key>::choice
                 .reachable_type != no_reachable_type;
    }
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
  DINGO_ALWAYS_INLINE Request resolve_named(Resolver &resolver,
                                            const void *key_value) {
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
    std::size_t count = 0;
    each_named<element, Key>(key_value, [&count](auto /*index*/) { ++count; });
    if (count == 0) {
      return;
    }
    append.reserve(collection, count);
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

  // The reachable type is selected by the descriptor of the request by `Key`,
  // which carries the annotation of an annotated request.
  template <typename Request, typename Key, std::size_t Index,
            typename Resolver>
  DINGO_ALWAYS_INLINE Request resolve_from(Resolver &resolver) {
    using binding = pack_at_t<Index, Bindings...>;
    using route = detail::static_route<binding, Request, Key>;
    constexpr std::size_t selected = route::choice.reachable_type;
    static_assert(
        require<selected != no_reachable_type, diag::type_not_convertible,
                Request, typename binding::stored_type>());
    static_assert(walk_supported_v<binding, selected>,
                  "the next implementation does not provide the route of the "
                  "requested type yet");
    using made_type = typename request_traits<Request>::made_type;
    using value_type = typename request_traits<Request>::value_type;
    constexpr const request_plan &plan = route::value;
    auto &holder =
        static_cast<detail::static_holder<Index, binding> &>(holders_);
    auto source = [&]() DINGO_INLINE_LAMBDA -> decltype(auto) {
      if constexpr (std::is_same_v<typename binding::locals, type_list<>>) {
        return holder.slot.get(resolver);
      } else {
        // The object resolves its dependencies through its local bindings.
        local_view<Resolver, typename binding::locals> view(
            resolver, &holder.locals, nullptr);
        return holder.slot.get(view);
      }
    };
    auto make = [&]() DINGO_INLINE_LAMBDA -> decltype(auto) {
      if constexpr (route::choice.created_count != 0 &&
                    plan.kind == completion::address) {
        // The request takes the address of the object the binding refers to,
        // null for the element of a wrapper that holds none.
        return address_from<binding, selected,
                            binding::published.items[selected].rule_count>(
            holder.slot.context(), source, true);
      } else if constexpr (plan.created == 0) {
        return walk_from<binding, selected,
                         binding::published.items[selected].rule_count>(
            holder.slot.context(), source);
      } else {
        using wrappers = planned_wrappers_t<made_type, route>;
        static_assert(wrappers::valid &&
                          std::is_same_v<typename wrappers::boundary,
                                         boundary_t<binding, selected>>,
                      "the requested wrapper cannot be created by the route");
        return wrappers::make([&]() DINGO_INLINE_LAMBDA -> decltype(auto) {
          return walk_to_boundary<binding, selected>(holder.slot.context(),
                                                     source);
        });
      }
    };
    if constexpr (std::is_rvalue_reference_v<Request> &&
                  route::category == value_category::rvalue) {
      resolver.retained().enter(resolver.transactions());
      value_type &kept = resolver.retained().template emplace<value_type>(
          [&] { return value_type(make()); });
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
