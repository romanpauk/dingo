//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/base/config.h>
#include <dingo/semantics/model.h>

#include <cstddef>
#include <cstdint>
#include <initializer_list>

// The derivation of a request of a binding in two halves, as both containers
// derive it, of no C++ type. A request the closure of the layer sources of a
// binding does not satisfy is reached from a state of the closure of a type the
// request spells: the request below some of its outer layers, or the pointer a
// handle of it adopts or observes, of the qualifiers it spells below that or of
// none. What the request reaches from the state depends on that type and on
// the form of the state alone: an extension is derived of the request for
// each, in the universe of its own layers, and the closure of a binding of any
// type gives the states it extends. A test compares the derivation with the
// specification for every stored object and request.

namespace dingo::semantics {

// The form of a layer source, as an extension depends on it: whether it is
// borrowed, made or a copy, whether it is an instance, whether its outer
// layer was made of what it holds, whether it owns what its pointer points to,
// and which of its levels live as long as the container, as a mask of their
// indices. The tables of a runtime resolution key an extension on its code.
struct source_form {
  semantics::origin origin = semantics::origin::borrowed;
  bool instance = false;
  bool made_layer = false;
  bool owns_pointee = false;
  std::uint8_t stable_levels = 0;

  constexpr std::uint16_t code() const {
    return static_cast<std::uint16_t>(
        static_cast<unsigned>(origin) | (instance ? 4u : 0u) |
        (made_layer ? 8u : 0u) | (owns_pointee ? 16u : 0u) |
        static_cast<unsigned>(stable_levels) << 5);
  }
};

constexpr source_form decoded_form(std::uint16_t code) {
  return {static_cast<semantics::origin>(code & 3u), (code & 4u) != 0,
          (code & 8u) != 0, (code & 16u) != 0,
          static_cast<std::uint8_t>(code >> 5)};
}

// The form of a state of a closure, as an extension depends on it: both
// containers key an extension on it. A borrowed object that is no instance
// extends as a copy that is none, of an outer level that lives as long as the
// request: nothing a request takes refers or points to it, it is not wrapped,
// and a layer a request converts shares its element, so that a move of it
// and a copy make the same. A copy of a pointer that owns what it points to
// extends as the pointer made: it is taken over or handed over alike, and
// never wrapped.
constexpr source_form object_form(const semantics::object &object) {
  source_form made{object.origin, object.instance, object.made_layer,
                   object.owns_pointee, 0};
  for (std::size_t index = 0; index <= object.depth; ++index) {
    if (object.lifetimes[index] == lifetime::stable) {
      made.stable_levels |= static_cast<std::uint8_t>(1u << index);
    }
  }
  if (made.origin == origin::borrowed && !made.instance) {
    made.origin = origin::copied;
    made.stable_levels &= static_cast<std::uint8_t>(~1u);
  }
  if (made.origin == origin::copied && made.owns_pointee) {
    made.origin = origin::made;
  }
  return made;
}

// Whether a state of a closure is a layer source an extension starts from: an
// object of no qualifiers of its own, constructed and not converted to a more
// qualified element, that does not hold the leaf a request of the interface
// cannot name.
constexpr bool extensible(const semantics::object &object,
                          const semantics::context &context) {
  return !object.unconstructed && !object.qualified && object.quals[0] == 0 &&
         !names_leaf(object, context);
}

// The layer source of a type and a form: of the layers, the element and the
// qualifiers below its own of the type, and of the form; the rest of an object
// is the same for every one, as no extension depends on it.
constexpr semantics::object source_object(const semantics::object &type,
                                          const source_form &form) {
  semantics::object made;
  set_stack(made, type);
  made.origin = form.origin;
  made.instance = form.instance;
  made.made_layer = form.made_layer;
  made.owns_pointee = form.owns_pointee;
  for (std::size_t index = 0; index <= type.depth; ++index) {
    made.lifetimes[index] = (form.stable_levels >> index) & 1u
                                ? lifetime::stable
                                : lifetime::request;
  }
  return made;
}

// The kinds of nested source of a request, a type it spells: the request
// below its outer layers, of its own qualifiers below the outer one or of
// none; and the pointer a handle of the request adopts or observes, of the
// qualifiers of the request below it or of none.
enum class nested_kind : std::uint8_t {
  spelled,
  unqualified,
  pointer,
  unqualified_pointer
};

constexpr bool pointer_kind(nested_kind kind) {
  return kind == nested_kind::pointer ||
         kind == nested_kind::unqualified_pointer;
}

// The nested source of a kind of a request below some of its outer layers,
// if it has one.
struct nested_source {
  bool exists = false;
  semantics::object type;
};

constexpr nested_source request_nested_source(const request &goal,
                                              const semantics::context &context,
                                              std::size_t offset,
                                              nested_kind kind) {
  nested_source made;
  semantics::object &type = made.type;
  if (!pointer_kind(kind)) {
    set_stack(type, goal, offset);
  } else {
    if (offset >= goal.depth) {
      return made;
    }
    const layer_kind &outer = context.kind(goal.layers[offset]);
    if (!outer.adopts && !outer.observes) {
      return made;
    }
    // A handle of an array is made of the pointer to its first element.
    // The pointer is in place of the outer layer, or of the outer layer and
    // the array below it.
    const bool array =
        offset + 1 < goal.depth && goal.layers[offset + 1] == array_layer;
    set_stack(type, goal, offset + (array ? 1 : 0));
    type.layers[0] = array ? array_pointer_layer : pointer_layer;
  }
  // A type of no qualifiers below its own, which a request converts to the
  // one it spells, is a nested source apart from it when that is of some.
  const bool unqualified = kind == nested_kind::unqualified ||
                           kind == nested_kind::unqualified_pointer;
  bool qualified = false;
  for (std::size_t index = 1; index <= type.depth; ++index) {
    qualified = qualified || type.quals[index] != 0;
    if (unqualified) {
      type.quals[index] = 0;
    }
  }
  made.exists = !unqualified || qualified;
  return made;
}

// A nested source of a request: below how many of its outer layers, of which
// kind, and the type.
struct nested_entry {
  std::uint8_t offset = 0;
  nested_kind kind = nested_kind::spelled;
  semantics::object type;
};

// The nested sources of a request, each of its offsets and kinds.
inline constexpr std::size_t max_nested = 4 * (max_layers + 1);
struct nested_list {
  nested_entry entries[max_nested] = {};
  std::size_t count = 0;
};

constexpr nested_list nested_sources(const request &goal,
                                     const semantics::context &context) {
  nested_list made;
  for (std::size_t offset = 0; offset <= goal.depth; ++offset) {
    for (const nested_kind kind :
         {nested_kind::spelled, nested_kind::unqualified, nested_kind::pointer,
          nested_kind::unqualified_pointer}) {
      // The type the request spells is extended by no step: one keeps
      // its layers, and its qualifiers are those the request names.
      if (offset == 0 && kind == nested_kind::spelled) {
        continue;
      }
      const nested_source type =
          request_nested_source(goal, context, offset, kind);
      if (type.exists) {
        made.entries[made.count++] = {static_cast<std::uint8_t>(offset), kind,
                                      type.type};
      }
    }
  }
  return made;
}

// The forms a layer source of a type may have, as object_form gives them:
// borrowed, an instance; made, of its outer layer made of what it holds or
// not; or a copy, an instance or not, of its outer layer made of what it holds
// or not. A level never outlives one it holds, and what is made or copied
// lives as long as the request. A pointer is owned when it was made, by a
// handle that adopts it, or borrowed, by one that observes it.
inline constexpr std::size_t max_forms = 9 * (max_layers + 1) + 1;
struct form_list {
  std::uint16_t forms[max_forms] = {};
  std::size_t count = 0;
};

// NOLINTNEXTLINE(readability-function-cognitive-complexity,readability-function-size)
constexpr form_list source_forms(const semantics::object &type, bool pointer,
                                 bool adopts, bool observes) {
  form_list made;
  for (const semantics::origin whose :
       {origin::borrowed, origin::made, origin::copied}) {
    const bool borrowed = whose == origin::borrowed;
    for (std::size_t request_levels = borrowed ? 0 : 1;
         request_levels <= type.depth + 1u; ++request_levels) {
      // The levels past the outer request_levels live as long as the
      // container.
      const auto stable_levels = static_cast<std::uint8_t>(
          (1u << (type.depth + 1u)) - (1u << request_levels));
      for (unsigned instance = whose == origin::copied ? 0u : 1u; instance < 2u;
           ++instance) {
        for (unsigned made_layer = 0; made_layer < (borrowed ? 1u : 2u);
             ++made_layer) {
          for (unsigned owns = 0; owns < (pointer ? 2u : 1u); ++owns) {
            if (pointer &&
                (owns ? !adopts || whose != origin::made : !observes)) {
              continue;
            }
            const source_form form{whose, instance != 0, made_layer != 0,
                                   owns != 0, stable_levels};
            made.forms[made.count++] = form.code();
          }
        }
      }
    }
  }
  return made;
}

// The forms of a nested source of a request: of a pointer, those the handle
// it is the pointer of adopts or observes.
constexpr form_list nested_forms(const nested_entry &group, const request &goal,
                                 const semantics::context &context) {
  if (!pointer_kind(group.kind)) {
    return source_forms(group.type, false, false, false);
  }
  const layer_kind &outer = context.kind(goal.layers[group.offset]);
  return source_forms(group.type, true, outer.adopts, outer.observes);
}

// The states an extension reaches at most: of one object, by the steps of
// the layers of a request, of which it reaches few.
inline constexpr std::size_t extension_states = 48;

// Extends a search of a nested source of a form, of the layer source alone,
// which it holds alone, by every step of a request but those into the layers
// of C++ itself, which the closure of the binding takes. The search is filled
// in place: a constant expression that returned it would be kept.
template <typename Search>
constexpr void
extend_source(Search &found, const nested_entry &group, std::uint16_t code,
              const semantics::context &context, const request &goal) {
  found.add({source_object(group.type, decoded_form(code)),
             {},
             no_state,
             step::decay});
  found.closed = 1;
  extend_reach(found, 1, language_layers, context, goal);
}

// What satisfies a request of the extension of a nested source, for a
// lifetime. Of the type the request spells, a step at least: what satisfies
// it of the layer source alone is the closure's, in the context of the
// binding. Of a type of fewer qualifiers, the layer source may satisfy it as
// C++ converts a pointer to more of them.
template <typename Search>
constexpr answer nested_answer(const Search &found, const nested_entry &group,
                               const semantics::context &context,
                               const request &goal, category taken,
                               lifetime needed) {
  const answer reached = satisfy(found, goal, taken, needed, context);
  if (group.kind == nested_kind::spelled && reached.found &&
      reached.state == 0) {
    return {};
  }
  return reached;
}

// What satisfies a request of a nested source of a form, for both lifetimes,
// of the semantics alone, so that requests of the same layers derive it once.
// The answers alone: a constant expression of them is kept for each nested
// source and form a state of a closure has.
DINGO_COLD constexpr lifetime_pair<answer>
nested_answers(const nested_entry &group, std::uint16_t code,
               const semantics::context &context, const request &goal,
               category taken) {
  basic_search<extension_states> found;
  extend_source(found, group, code, context, goal);
  return {nested_answer(found, group, context, goal, taken, lifetime::stable),
          nested_answer(found, group, context, goal, taken, lifetime::request)};
}

// The derivations of the extension of a nested source of a form, for both
// lifetimes, of one search, and whether it reached more states than it holds
// or a derivation takes more steps than one holds.
struct extension_derivations {
  lifetime_pair<derivation> derivations;
  bool overflow = false;
};

DINGO_COLD constexpr extension_derivations
nested_extension(const nested_entry &group, std::uint16_t code,
                 const semantics::context &context, const request &goal,
                 category taken) {
  basic_search<extension_states> found;
  extend_source(found, group, code, context, goal);
  extension_derivations made;
  made.overflow = found.overflow;
  for (const lifetime needed : {lifetime::stable, lifetime::request}) {
    const answer reached =
        nested_answer(found, group, context, goal, taken, needed);
    if (reached.found) {
      made.derivations[needed] = answer_derivation(found, reached);
      made.overflow = made.overflow || !made.derivations[needed].found;
    }
  }
  return made;
}

// The cheapest extension of a state of a closure of those considered: the
// cost of the route to the state followed by that of the extension, and
// whether what satisfies the request may be absent.
struct extension_cost {
  bool found = false;
  cost fewest;
  bool absent = false;

  // Considers the extension of a state: of the cost of the route to the state
  // and of the extension, and whether either may be absent. Whether it is
  // cheaper than every one before, which it then replaces.
  constexpr bool consider(cost route, bool route_absent, cost extended,
                          bool extended_absent) {
    const cost made = followed(route, extended);
    const bool made_absent = route_absent || extended_absent;
    if (found && !cheaper(made, made_absent, fewest, absent)) {
      return false;
    }
    found = true;
    fewest = made;
    absent = made_absent;
    return true;
  }
};

// The layer of one universe each layer of another is: of the universe of a
// binding, each of the universe of the layers of a request type.
struct layer_map {
  layer indices[max_kinds] = {};
};

constexpr semantics::object mapped(const layer_map &layers,
                                   semantics::object object) {
  for (std::size_t index = 0; index < object.depth; ++index) {
    object.layers[index] = layers.indices[object.layers[index]];
  }
  if (object.contained != no_layer) {
    object.contained = layers.indices[object.contained];
  }
  return object;
}

// The derivation of a request of a binding for a lifetime, in two halves: of
// the closure of the layer sources of the binding, when it satisfies the
// request; else of the state of the closure the request extends of the fewest
// copies and moves, followed by the extension of the type and the form of the
// state. The closure, its context and the request are of the universe of the
// binding. The extensions are of the context and the request of the request
// type alone (own, own_goal), in the universe of its own layers, which the map
// places in the universe of the binding. Only the pairs of a nested source and
// a form a state of the closure has are extended, each by nested_answers, of
// the semantics alone, and the cheapest by nested_extension. A derivation of
// more steps than a derivation holds overflows.
constexpr derivation
split_derivation(const search &closed, const semantics::context &context,
                 const request &goal, category taken, lifetime needed,
                 const semantics::context &own, const request &own_goal,
                 const layer_map &layers) {
  const answer direct =
      satisfy_states(closed.states, closed.count, closed.overflow, goal, taken,
                     needed, context, affected_cost{});
  if (direct.found || direct.overflow) {
    derivation made = answer_derivation(closed, direct);
    made.overflow = made.overflow || made.found != direct.found;
    return made;
  }
  const nested_list nested = nested_sources(own_goal, own);
  extension_cost fewest;
  std::size_t route_state = 0;
  std::size_t chosen = 0;
  std::uint16_t chosen_code = 0;
  for (std::size_t nested_index = 0; nested_index < nested.count;
       ++nested_index) {
    const semantics::object type =
        mapped(layers, nested.entries[nested_index].type);
    for (std::size_t source = 0; source < closed.count; ++source) {
      const semantics::state &source_state = closed.states[source];
      if (!same_stack(source_state.reached, type, false) ||
          !extensible(source_state.reached, context)) {
        continue;
      }
      const std::uint16_t code = object_form(source_state.reached).code();
      const answer reached = nested_answers(nested.entries[nested_index], code,
                                            own, own_goal, taken)[needed];
      if (reached.found &&
          fewest.consider(source_state.made, source_state.reached.absent,
                          reached.fewest, reached.absent)) {
        route_state = source;
        chosen = nested_index;
        chosen_code = code;
      }
    }
  }
  if (!fewest.found) {
    // GCC 13 rejects copy-list-initialization of this aggregate from {}.
    return derivation{};
  }
  const extension_derivations chosen_extension = nested_extension(
      nested.entries[chosen], chosen_code, own, own_goal, taken);
  const derivation &extended = chosen_extension.derivations[needed];
  answer route;
  route.found = true;
  route.state = static_cast<std::uint16_t>(route_state);
  derivation made = answer_derivation(closed, route);
  if (!made.found || !extended.found ||
      made.count + extended.count > max_steps) {
    derivation overflowed;
    overflowed.overflow = true;
    return overflowed;
  }
  for (std::size_t index = 0; index < extended.count; ++index) {
    made.steps[made.count] = extended.steps[index];
    made.objects[made.count + 1] = mapped(layers, extended.objects[index + 1]);
    made.states[made.count + 1] =
        static_cast<std::uint16_t>(closed.count + index + 1);
    ++made.count;
  }
  made.access = extended.access;
  made.fewest = fewest.fewest;
  made.absent = fewest.absent;
  made.overflow = extended.overflow;
  return made;
}

} // namespace dingo::semantics
