//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

// What a binding derives for a request, without resolving it: whether it
// derives the request for as long as the container, which the caller may take,
// only for as long as the request, or not at all. The static container derives
// it as the code compiles; the runtime container selects its conversion of the
// tables of the binding and of the request as it runs, and the selection is
// compared with the derivation. Nothing of a container is instantiated, so
// many registrations are resolved in one program.

#pragma once

#include "model/probe.h"

#include <dingo/runtime/request.h>
#include <dingo/runtime/route.h>
#include <dingo/static/binding.h>

#include <cstddef>
#include <cstdint>
#include <initializer_list>

namespace dingo::model {

// How C++ itself builds a request of an object the binding holds,
// independent of the library. The stored object is one, which a unique
// scope makes for the request alone; and so is what it dereferences to, down
// to the leaf, which the request owns when it owns the unique_ptr or the
// optional that holds it, or the raw pointer a unique scope made it. An array
// is not dereferenced: its first element is no object of its own.
template <typename T> struct owns_element : std::false_type {};
template <typename T> struct owns_element<T *> : std::true_type {};
template <typename T, typename Deleter>
struct owns_element<std::unique_ptr<T, Deleter>> : std::true_type {};
template <typename T> struct owns_element<std::optional<T>> : std::true_type {};

// The value an optional is made of.
template <typename T> struct optional_value_of {
  using type = void;
};
template <typename T> struct optional_value_of<std::optional<T>> {
  using type = T;
};

template <typename Source, typename = void> struct dereferenced {
  using type = void;
};
template <typename Source>
struct dereferenced<Source, std::void_t<decltype(*std::declval<Source &>())>> {
  using type = std::conditional_t<
      std::is_array_v<Source>, void,
      std::remove_reference_t<decltype(*std::declval<Source &>())>>;
};

// The ways C++ builds a request of an object: binding a reference or a
// pointer to it without a temporary, converting a value of it the request
// owns, copying it into a value, or copying it into a new optional.
enum route : unsigned { refers, moves, copies, wraps };

// A way of building a request of an object at a depth of dereferences, of an
// object that shares nothing with its copy or not, as a bit of a mask.
constexpr unsigned route_bit(route how, std::size_t depth, bool value) {
  return 1u << ((how * 4 + depth) * 2 + (value ? 1 : 0));
}

// The ways C++ builds Request of Source, at a depth: a value converted of it,
// moved when the request owns it and copied otherwise, without an explicit
// constructor; a reference or a pointer bound to it without a temporary, an
// rvalue reference only to what the request owns; or a pointer the source
// is, or an array decays to.
template <typename Request, typename Source, bool Owned, std::size_t Depth>
constexpr unsigned routes_at() {
  static_assert(Depth < 4, "a route_bit holds four depths");
  using object = std::remove_reference_t<Request>;
  using value = std::remove_cv_t<Request>;
  constexpr bool shares_nothing =
      std::is_void_v<typename dereferenced<Source>::type> ||
      owns_element<Source>::value;
  unsigned found = 0;
  if constexpr (std::is_pointer_v<Request>) {
    if (std::is_convertible_v<Source *, Request> ||
        (std::is_array_v<Source> && std::is_convertible_v<Source &, Request>)) {
      found |= route_bit(refers, Depth, shares_nothing);
    }
  } else if constexpr (std::is_reference_v<Request>) {
    if (std::is_convertible_v<Source *, object *> &&
        (Owned || !std::is_rvalue_reference_v<Request>)) {
      found |= route_bit(refers, Depth, shares_nothing);
    }
  }
  // An array is no value of its own: it decays.
  if constexpr (!std::is_array_v<Source> && !std::is_reference_v<Request>) {
    if constexpr (Owned) {
      if (std::is_convertible_v<Source &&, value>) {
        found |= route_bit(moves, Depth, shares_nothing);
      }
    } else if constexpr (std::is_convertible_v<Source &, value>) {
      using made_of = typename optional_value_of<value>::type;
      if constexpr (!std::is_void_v<made_of> &&
                    std::is_convertible_v<Source &, made_of>) {
        found |= route_bit(wraps, Depth, shares_nothing);
      } else {
        found |= route_bit(copies, Depth, shares_nothing);
      }
    }
  }
  return found;
}

// The ways C++ builds Request of Source or of anything Source dereferences
// to.
template <typename Request, typename Source, bool Owned, std::size_t Depth = 0>
constexpr unsigned routes_through() {
  using element = typename dereferenced<Source>::type;
  constexpr unsigned found = routes_at<Request, Source, Owned, Depth>();
  if constexpr (std::is_void_v<element>) {
    return found;
  } else {
    return found |
           routes_through<Request, element,
                          Owned && owns_element<Source>::value, Depth + 1>();
  }
}

// The conversion a container chooses for a request, for a lifetime: whether
// it has one, the copies and the moves of the leaf and the objects it keeps
// and makes, whether what satisfies the
// request may be absent, how the request takes it, whether the address it
// gives lives as long as the container, and how the request takes the value:
// as the conversion satisfies it, or made of a copy of qualifiers quals or of
// a run of depth wraps.
struct chosen_conversion {
  bool found = false;
  std::uint8_t copies = 0;
  std::uint8_t moves = 0;
  std::uint8_t kept = 0;
  std::uint8_t made = 0;
  bool absent = false;
  semantics::access access = semantics::access::refer;
  bool stable_address = false;
  detail::execution::finish last = detail::execution::finish::satisfy;
  std::uint8_t quals = 0;
  std::uint8_t depth = 0;
};

// How a request takes the value of what a conversion reaches, of what tells
// it apart: the qualifiers of a copy, the wraps of a run.
constexpr void finish_conversion(chosen_conversion &made,
                                 detail::execution::finish last,
                                 std::uint8_t quals, std::uint8_t depth) {
  made.last = last;
  made.quals = last == detail::execution::finish::copy ? quals : 0;
  made.depth = last == detail::execution::finish::run ? depth : 0;
}

// The conversion of the derivation of a request, as the static container
// runs it: the address it gives lives as long as the container when what it
// reaches does, or what that points to, for a request that points to it.
constexpr chosen_conversion
derived_conversion(const semantics::derivation &derived,
                   const detail::execution::conversion &compiled) {
  chosen_conversion made;
  if (!derived.found) {
    return made;
  }
  made.found = true;
  made.copies = derived.fewest.copies;
  made.moves = derived.fewest.moves;
  made.kept = derived.fewest.kept;
  made.made = derived.fewest.made;
  made.absent = derived.absent;
  made.access = derived.access;
  const semantics::object &reached = derived.objects[derived.count];
  made.stable_address =
      (derived.access == semantics::access::point
           ? reached.lifetimes[1]
           : reached.lifetimes[0]) == semantics::lifetime::stable;
  finish_conversion(made, compiled.last, compiled.quals, compiled.depth);
  return made;
}

// A state of the closure of a binding as an answer of the closure reaches
// it: the copies and the moves of the leaf that reach it, whether it may be
// absent, and whether a value of it makes the leaf anew, which a move or a
// copy of it then moves or copies.
struct closed_route {
  semantics::cost made;
  bool absent = false;
  bool anew = false;
};

// The routes of each state of the closure of the layer sources Sources of a
// binding.
template <typename Sources> struct closed_routes {
  struct list {
    closed_route entries[Sources::closure.count];
  };
  static constexpr list make() {
    list made{};
    const semantics::context context = Sources::semantic::context();
    for (std::size_t state = 0; state < Sources::closure.count; ++state) {
      const semantics::state &each = Sources::closure.states[state];
      made.entries[state] = {each.made, each.reached.absent,
                             semantics::makes_leaf_anew(each.reached, context)};
    }
    return made;
  }
  static constexpr list value = make();
};

// The conversion the runtime container selects for a request of a binding of
// a layout and the key of its interface, for a lifetime, as a resolution
// selects it. The cost of an answer of the closure is that of the route to its
// state, then of how the request takes it, as satisfy_states counts it; that
// of an extension is the one its selection compares.
inline chosen_conversion
runtime_selected_conversion(const detail::execution::binding_layout *layout,
                            const void *interface_key,
                            const detail::runtime_request &request, bool stable,
                            const closed_route *routes) {
  const detail::selected_conversion selected =
      detail::find_conversion(layout, interface_key, stable, request, [] {
        return detail::selected_conversion{nullptr, nullptr, 0, nullptr};
      });
  chosen_conversion made;
  if (selected.answer != nullptr) {
    const detail::execution::closed_answer &answer = *selected.answer;
    const closed_route &route = routes[answer.route];
    semantics::effect effect = semantics::effect::none;
    if (route.anew && answer.access == semantics::access::move) {
      effect = semantics::effect::move;
    } else if (route.anew && answer.access == semantics::access::copy) {
      effect = semantics::effect::copy;
    }
    const semantics::cost cost = semantics::affected(route.made, effect);
    made.found = true;
    made.copies = cost.copies;
    made.moves = cost.moves;
    made.kept = cost.kept;
    made.made = cost.made;
    made.absent = route.absent;
    made.access = answer.access;
    made.stable_address = answer.stable_address;
    finish_conversion(made,
                      answer.value_route ? detail::execution::finish::copy
                                         : detail::execution::finish::satisfy,
                      answer.quals, 0);
  } else if (selected.derived != nullptr) {
    // The cost of the extension selected, of the same selection again: one
    // that selects another extension is none.
    semantics::extension_cost fewest;
    const detail::selected_conversion cheapest =
        detail::cheapest_extension(*layout, request, stable, fewest);
    const detail::execution::runtime_conversion &derived = *selected.derived;
    made.found = cheapest.derived == selected.derived &&
                 cheapest.route == selected.route;
    made.copies = fewest.fewest.copies;
    made.moves = fewest.fewest.moves;
    made.kept = fewest.fewest.kept;
    made.made = fewest.fewest.made;
    made.absent = fewest.absent;
    made.access = derived.access;
    made.stable_address = derived.stable_address;
    finish_conversion(made, derived.last, derived.quals, derived.depth);
  }
  return made;
}

// Prints the conversions a container chooses for a request, stable and
// transient, as a JSON array of the properties of each.
inline void print_chosen(const chosen_conversion &stable,
                         const chosen_conversion &transient) {
  std::printf("[");
  for (const chosen_conversion *each : {&stable, &transient}) {
    std::printf("%s%u, %u, %u, %u, %u, %u, %u, %u, %u, %u, %u",
                each == &stable ? "" : ", ", each->found ? 1u : 0u,
                unsigned{each->copies}, unsigned{each->moves},
                unsigned{each->kept}, unsigned{each->made},
                each->absent ? 1u : 0u, static_cast<unsigned>(each->access),
                each->stable_address ? 1u : 0u,
                static_cast<unsigned>(each->last), unsigned{each->quals},
                unsigned{each->depth});
  }
  std::printf("]");
}

// The routes of the closure of a binding of the execution Execution, none
// of one the runtime container does not model.
template <typename Execution> constexpr const closed_route *routes_v() {
  if constexpr (Execution::modeled) {
    return closed_routes<typename detail::execution::published_sources<
        Execution>::sources>::value.entries;
  } else {
    return nullptr;
  }
}

// The runtime binding of an interface of a registration, as a resolution
// selects a conversion of it: its layout, the key of its interface and the
// routes of its closure; none of a registration the library does not accept.
struct binding_tables {
  const detail::execution::binding_layout *layout = nullptr;
  const void *interface_key = nullptr;
  const closed_route *routes = nullptr;
};

template <typename Registration, typename Interface>
constexpr binding_tables binding_tables_v() {
  if constexpr (registrable<Registration, false>()) {
    using type = unannotated_t<Interface>;
    using model = detail::binding_model<Registration>;
    using execution = detail::binding_execution<
        typename detail::binding<type, model>::storage_type,
        std::remove_cv_t<type>>;
    return {detail::execution::published_layout<execution>(),
            &detail::execution::shape_key<
                detail::execution::request_interface_t<type>>,
            routes_v<execution>()};
  } else {
    return {};
  }
}

// Records what the runtime container selects for a request of a binding, the
// pair at an index of a program: the conversion for each lifetime, and
// whether it models the request at all.
inline void record_selected(std::size_t pair, const binding_tables &binding,
                            const detail::runtime_request &request) {
  std::printf("{\"pair\": %zu, \"selected\": ", pair);
  print_chosen(
      runtime_selected_conversion(binding.layout, binding.interface_key,
                                  request, true, binding.routes),
      runtime_selected_conversion(binding.layout, binding.interface_key,
                                  request, false, binding.routes));
  std::printf(", \"modeled\": %s}\n",
              binding.layout != nullptr && request.modeled ? "true" : "false");
}

// Records the outcome of a request of Interface the binding of a
// registration resolves, as the code compiles: published when it derives the
// request for as long as the container, placed when only for as long as the
// request, unpublished when not at all. A request the lookup sends to the
// binding of another interface resolves there. Records too the ways C++
// itself builds the request of what the binding holds, which the request
// owns when its scope Makes it for the request alone.
template <typename Model, typename Interface, typename Request, typename Leaf,
          bool Makes>
DINGO_ALWAYS_INLINE void publication(const char *prefix, const char *name) {
  using type = unannotated_t<Interface>;
  using binding = detail::binding<type, Model>;
  using resolved = detail::static_execution<Request, binding>;
  using stored = typename Model::registered_storage_type;
  constexpr bool owned = !std::is_reference_v<stored> && Makes;
  (void)sizeof(Leaf);
  if constexpr (!selects_v<Request, type>) {
    record(prefix, name, "selects_another", nullptr);
  } else {
    const char *outcome = resolved::template derived<true>().found ? "published"
                          : resolved::template derived<false>().found
                              ? "placed"
                              : "unpublished";
    using derivation = typename resolved::derivation;
    constexpr chosen_conversion stable = derived_conversion(
        resolved::template derived<true>(), derivation::stable_conversion);
    constexpr chosen_conversion transient = derived_conversion(
        resolved::template derived<false>(), derivation::transient_conversion);
    std::printf("{\"cell\": \"");
    print_json_text(prefix);
    print_json_text(name);
    std::printf(
        "\", \"outcome\": \"%s\", \"cpp_routes\": %u, \"derived\": ", outcome,
        routes_through<Request, std::remove_reference_t<stored>, owned>());
    print_chosen(stable, transient);
    std::printf("}\n");
  }
}

} // namespace dingo::model
