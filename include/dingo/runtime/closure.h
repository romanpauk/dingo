//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/base/config.h>
#include <dingo/request/conversion.h>
#include <dingo/runtime/layout.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <type_traits>
#include <utility>

// The closure of the layer sources of a binding, as the code compiles: the
// classes of its states, and the answers of the requests it satisfies, which
// every binding of its shape shares.

namespace dingo::detail::execution {

// The C++ types of the states of a search, and its steps between them: a
// state is of the class of the first state with its layers, its element, its
// qualifiers and its origin, and a step into a state is the first step of its
// kind between their classes.
struct state_classes {
  std::uint16_t state_class[semantics::max_states] = {};
  std::uint16_t state_edge[semantics::max_states] = {};
  std::uint16_t edges[semantics::max_states] = {};
  std::size_t edge_count = 0;
};

constexpr bool same_type(const semantics::object &a,
                         const semantics::object &b) {
  if (a.depth != b.depth || a.element != b.element || a.origin != b.origin) {
    return false;
  }
  for (std::size_t index = 0; index < a.depth; ++index) {
    if (a.layers[index] != b.layers[index]) {
      return false;
    }
  }
  for (std::size_t index = 0; index <= a.depth; ++index) {
    if (a.quals[index] != b.quals[index]) {
      return false;
    }
  }
  return true;
}

constexpr std::uint32_t type_hash(const semantics::object &object) {
  std::uint32_t hash = 2166136261u;
  hash = (hash ^ object.depth) * 16777619u;
  hash = (hash ^ static_cast<unsigned>(object.element)) * 16777619u;
  hash = (hash ^ static_cast<unsigned>(object.origin)) * 16777619u;
  for (std::size_t index = 0; index < object.depth; ++index) {
    hash = (hash ^ object.layers[index]) * 16777619u;
  }
  for (std::size_t index = 0; index <= object.depth; ++index) {
    hash = (hash ^ object.quals[index]) * 16777619u;
  }
  return hash;
}

// Whether each state of the sources of a binding is on the way to what a
// request takes: in a context that requests the interface, a request takes
// nothing of what holds the leaf, which is only on the way to what holds
// the interface.
constexpr void used_states(const semantics::search &found, bool abstract,
                           bool *used) {
  for (std::size_t index = 0; index < found.count; ++index) {
    used[index] = !abstract;
  }
  for (std::size_t index = found.count; index-- > 0;) {
    const semantics::state &each = found.states[index];
    if (each.reached.element == semantics::element::abstract) {
      used[index] = true;
    }
    if (used[index] && each.parent != semantics::no_state) {
      used[each.parent] = true;
    }
  }
}

constexpr state_classes classify_states(const semantics::search &found,
                                        bool abstract = false) {
  state_classes made;
  std::uint16_t types[semantics::search_slots] = {};
  std::uint16_t steps[semantics::search_slots] = {};
  bool used[semantics::max_states] = {};
  used_states(found, abstract, used);
  for (std::size_t index = 0; index < found.count; ++index) {
    const semantics::state &each = found.states[index];
    std::size_t slot = type_hash(each.reached) % semantics::search_slots;
    while (types[slot] != 0 &&
           !same_type(found.states[types[slot] - 1].reached, each.reached)) {
      slot = (slot + 1) % semantics::search_slots;
    }
    if (types[slot] == 0) {
      types[slot] = static_cast<std::uint16_t>(index + 1);
    }
    made.state_class[index] = static_cast<std::uint16_t>(types[slot] - 1);
    if (index == 0 || !used[index]) {
      continue;
    }
    const std::uint16_t source = made.state_class[each.parent];
    const std::uint16_t target = made.state_class[index];
    slot = ((source * 31u + target) * 31u + static_cast<unsigned>(each.step)) %
           semantics::search_slots;
    while (steps[slot] != 0) {
      const std::uint16_t edge = made.edges[steps[slot] - 1];
      const semantics::state &other = found.states[edge];
      if (made.state_class[other.parent] == source &&
          made.state_class[edge] == target && other.step == each.step) {
        break;
      }
      slot = (slot + 1) % semantics::search_slots;
    }
    if (steps[slot] == 0) {
      made.edges[made.edge_count++] = static_cast<std::uint16_t>(index);
      steps[slot] = static_cast<std::uint16_t>(made.edge_count);
    }
    made.state_edge[index] = static_cast<std::uint16_t>(steps[slot] - 1);
  }
  return made;
}

// The layer sources of a binding, as it publishes them: those a request
// takes something of, and those on the way to them, in their order; and the
// table of the type of each, which the steps into it are checked against as
// the code compiles.
// Whether a step makes a layer of what it is taken from, which the table of
// what it makes has.
constexpr bool step_makes(semantics::step kind) {
  return kind == semantics::step::wrap || kind == semantics::step::adopt ||
         kind == semantics::step::observe;
}

// The kinds of the steps a request takes from an object, as a mask.
struct step_kinds_sink {
  std::uint32_t kinds = 0;
  // It takes a step of every outer layer.
  constexpr bool takes(semantics::step, semantics::layer, std::size_t) const {
    return true;
  }
  constexpr bool takes_quals(std::size_t, std::uint8_t, std::size_t) const {
    return true;
  }
  constexpr void operator()(semantics::step kind, const semantics::object &) {
    kinds |= std::uint32_t{1} << static_cast<unsigned>(kind);
  }
};

constexpr std::uint32_t request_kinds(const semantics::object &object,
                                      const semantics::context &context) {
  step_kinds_sink sink;
  semantics::each_request_step(object, context, sink);
  return sink.kinds;
}

// The semantics of the closure of a binding of a shape, of no C++ type: the
// classes of the states of its layer sources, those a request takes, the
// published states closed under every step a request takes but a wrap, and
// the derivation and the conversion of the route of each state. Bindings of
// every type of a shape share them.
template <typename Shape> struct closed_semantics {
  static constexpr const semantics::search &Sources = Shape::sources;
  static constexpr bool abstract = Shape::context.abstract;
  static constexpr semantics::context context() { return Shape::context; }
  static constexpr state_classes classes = classify_states(Sources, abstract);
  struct used_list {
    bool value[semantics::max_states] = {};
    std::uint16_t index[semantics::max_states] = {};
    std::uint16_t states[semantics::max_states] = {};
    std::size_t count = 0;
  };
  static constexpr used_list make_used() {
    used_list made;
    used_states(Sources, abstract, made.value);
    for (std::size_t index = 0; index < Sources.count; ++index) {
      if (made.value[index]) {
        made.index[index] = static_cast<std::uint16_t>(made.count);
        made.states[made.count++] = static_cast<std::uint16_t>(index);
      }
    }
    return made;
  }
  static constexpr used_list used = make_used();
  // The operations each class of states needs: the steps the binding takes
  // from it, the steps a request may take from it, and the wraps that make
  // it. A sole owner converts to the sharing handle its type traits name, and
  // a handle of an array of a known bound to the handle of an array of unknown
  // bound they name, which a request may bring into the universe: of every
  // source that is one.
  struct mask_list {
    std::uint32_t state_mask[semantics::max_states] = {};
  };
  static constexpr mask_list make_masks() {
    mask_list made;
    constexpr std::uint32_t taken =
        operation_mask(semantics::step::copy) |
        operation_mask(semantics::step::decay) |
        operation_mask(semantics::step::hand_over) |
        operation_mask(semantics::step::transfer) |
        operation_mask(semantics::step::unknown_bound);
    const semantics::context context = Shape::context;
    for (std::size_t index = 0; index < used.count; ++index) {
      const std::size_t state = used.states[index];
      const semantics::state &each = Sources.states[state];
      made.state_mask[classes.state_class[state]] |=
          (request_kinds(each.reached, context) & taken) |
          operation_mask(semantics::step::transfer) |
          operation_mask(semantics::step::unknown_bound);
      if (state == 0) {
        continue;
      }
      if (step_makes(each.step)) {
        made.state_mask[classes.state_class[state]] |= makers_mask;
      } else if (each.step == semantics::step::release &&
                 Sources.states[each.parent].reached.layers[0] ==
                     semantics::array_pointer_layer) {
        made.state_mask[classes.state_class[each.parent]] |= release_array_mask;
      } else {
        made.state_mask[classes.state_class[each.parent]] |=
            operation_mask(each.step);
        // A request makes the copy of what maybe_element gives where it
        // takes it.
        if (each.step == semantics::step::maybe_element_copy) {
          made.state_mask[classes.state_class[each.parent]] |=
              operation_mask(semantics::step::maybe_element);
        }
      }
    }
    return made;
  }
  static constexpr mask_list masks = make_masks();
  // How many wraps make a state at once, down to the first step that is
  // not one.
  static constexpr std::size_t run_length(std::size_t state) {
    std::size_t run = 0;
    while (Sources.states[state].step == semantics::step::wrap) {
      ++run;
      state = Sources.states[state].parent;
    }
    return run;
  }
  // The published states closed under every step a request takes but a
  // wrap, in the layers the binding knows: what a request reaches from. The
  // binding has the tables of the layer sources alone: a type a step of a
  // request makes is one the request spells, and C++ may not make every one
  // the semantics allow of a type it never spells.
  static constexpr semantics::search make_closure() {
    semantics::search found;
    for (std::size_t index = 0; index < used.count; ++index) {
      semantics::state each = Sources.states[used.states[index]];
      if (each.parent != semantics::no_state) {
        each.parent = used.index[each.parent];
      }
      found.add(each);
    }
    semantics::close_layer_sources(found, Shape::context);
    return found;
  }
  static constexpr semantics::search closure = make_closure();
  static_assert(!closure.overflow,
                "the stored object publishes more layer sources than "
                "semantics::max_states");

  template <std::size_t... I>
  static constexpr auto close(std::index_sequence<I...>) {
    struct list {
      semantics::state states[sizeof...(I)];
    };
    return list{{closure.states[I]...}};
  }
  static constexpr auto closed =
      close(std::make_index_sequence<closure.count>{});
  // The route of a state of the closure: the conversion of its derivation from
  // the stored object, run as the code compiles, to what it reaches; of a value
  // the request takes (Value), which it makes of what the conversion reaches
  // when the step into the state is a copy.
  static constexpr semantics::answer route_answer(std::size_t state,
                                                  bool value) {
    semantics::answer made;
    made.found = true;
    made.state = static_cast<std::uint16_t>(state);
    made.access = value ? semantics::access::copy : semantics::access::refer;
    return made;
  }
  template <std::size_t State, bool Value>
  static constexpr semantics::derivation route_derivation =
      semantics::answer_derivation(closure, route_answer(State, Value));
  template <std::size_t State, bool Value>
  static constexpr conversion route_conversion =
      compile_conversion(route_derivation<State, Value>, used.count);
  template <std::size_t State> static constexpr route_finish route_finish_v() {
    return {route_conversion<State, true>.last == finish::copy,
            route_conversion<State, true>.quals};
  }
  template <std::size_t... I>
  static constexpr auto make_finishes(std::index_sequence<I...>) {
    struct list {
      route_finish entries[sizeof...(I)];
    };
    return list{{route_finish_v<I>()...}};
  }
  static constexpr auto route_finishes =
      make_finishes(std::make_index_sequence<closure.count>{});
};

// The answers of the closure of a binding of a shape, of the states C++
// makes of it, as the bits of Possible: of each shape of a request the
// closure serves, of each category and qualifiers of the object it is of.
template <typename Semantic, std::uint64_t... Possible>
struct closed_answer_table {
  static constexpr std::uint64_t possible_bits[] = {Possible..., 0};
  static constexpr bool possible(std::size_t state) {
    return ((possible_bits[state / 64] >> (state % 64)) & 1u) != 0;
  }
  static constexpr const semantics::search &closure = Semantic::closure;
  static constexpr const auto &route_finishes = Semantic::route_finishes;
  // The shapes the closure serves a request of: that of each state C++
  // makes, and what each raw pointer of them points to, of the qualifiers of
  // every level below the object; with a state of the shape, and whether it
  // is the pointer to it.
  static constexpr std::size_t shape_bound = 2 * closure.count;
  struct shape_list {
    semantics::request goals[shape_bound] = {};
    std::uint16_t states[shape_bound] = {};
    bool pointee[shape_bound] = {};
    std::size_t count = 0;
  };
  static constexpr bool same_goal(const semantics::request &a,
                                  const semantics::request &b) {
    if (a.depth != b.depth || a.element != b.element) {
      return false;
    }
    for (std::size_t index = 0; index < a.depth; ++index) {
      if (a.layers[index] != b.layers[index]) {
        return false;
      }
    }
    for (std::size_t index = 1; index <= a.depth; ++index) {
      if (a.quals[index] != b.quals[index]) {
        return false;
      }
    }
    return true;
  }
  static constexpr void add_shape(shape_list &made,
                                  const semantics::request &goal,
                                  std::uint16_t state, bool pointee) {
    for (std::size_t index = 0; index < made.count; ++index) {
      if (same_goal(made.goals[index], goal)) {
        return;
      }
    }
    made.goals[made.count] = goal;
    made.states[made.count] = state;
    made.pointee[made.count] = pointee;
    ++made.count;
  }
  static constexpr shape_list make_shapes() {
    shape_list made;
    const semantics::context context = Semantic::context();
    for (std::size_t state = 0; state < closure.count; ++state) {
      if (!possible(state)) {
        continue;
      }
      const semantics::object &object = closure.states[state].reached;
      semantics::request goal;
      goal.depth = object.depth;
      goal.element = object.element;
      goal.shape_depth = object.depth;
      for (std::size_t index = 0; index < object.depth; ++index) {
        goal.layers[index] = object.layers[index];
      }
      for (std::size_t index = 1; index <= object.depth; ++index) {
        goal.quals[index] = object.quals[index];
      }
      add_shape(made, goal, static_cast<std::uint16_t>(state), false);
      if (object.depth && context.kind(object.layers[0]).adopted) {
        semantics::request pointed;
        pointed.depth = static_cast<std::uint8_t>(object.depth - 1);
        pointed.element = object.element;
        pointed.shape_depth = pointed.depth;
        for (std::size_t index = 1; index < object.depth; ++index) {
          pointed.layers[index - 1] = object.layers[index];
        }
        for (std::size_t index = 2; index <= object.depth; ++index) {
          pointed.quals[index - 1] = object.quals[index];
        }
        add_shape(made, pointed, static_cast<std::uint16_t>(state), true);
      }
    }
    return made;
  }
  static constexpr shape_list shapes = make_shapes();

  // The answer of the closure of a request, as closed_plan makes it.
  static constexpr closed_answer find_answer(const semantics::request &goal,
                                             semantics::category taken,
                                             semantics::lifetime needed) {
    const semantics::answer result = semantics::satisfy_states(
        closure.states, closure.count, closure.overflow, goal, taken, needed,
        Semantic::context(), semantics::affected_cost{});
    closed_answer made;
    if (!result.found || !possible(result.state)) {
      return made;
    }
    made.found = true;
    made.route = result.state;
    made.access = result.access;
    const route_finish finished = route_finishes.entries[result.state];
    made.value_route = (result.access == semantics::access::move ||
                        result.access == semantics::access::copy) &&
                       finished.copies;
    made.quals = finished.quals;
    const semantics::object &reached = closure.states[result.state].reached;
    made.stable_address =
        (result.access == semantics::access::point
             ? reached.lifetimes[1]
             : reached.lifetimes[0]) == semantics::lifetime::stable;
    return made;
  }

  // The answers of every shape the closure serves, of each category and
  // qualifiers of the object a request is of, that it answers for a
  // lifetime.
  struct answer_list {
    closed_answers entries[shapes.count * shape_answer_count + 1] = {};
  };
  static constexpr answer_list make_answers() {
    answer_list made;
    for (std::size_t shape = 0; shape < shapes.count; ++shape) {
      for (unsigned taken = 0; taken < 7; ++taken) {
        for (std::uint8_t quals = 0; quals < 4; ++quals) {
          semantics::request goal = shapes.goals[shape];
          goal.quals[0] = quals;
          const auto category = static_cast<semantics::category>(taken);
          made.entries[shape * shape_answer_count + taken * 4 + quals] = {
              find_answer(goal, category, semantics::lifetime::stable),
              find_answer(goal, category, semantics::lifetime::request)};
        }
      }
    }
    return made;
  }
  static constexpr answer_list answers = make_answers();

  // Whether a step a request takes of what the binding does not know may
  // start from a state: a wrap of what it owns, a handle made of a raw
  // pointer, or a handle it owns converted.
  static constexpr bool starts_steps(const semantics::object &object) {
    const semantics::context context = Semantic::context();
    const bool wraps =
        (object.origin == semantics::origin::made ||
         (object.origin == semantics::origin::copied && object.instance)) &&
        semantics::can_move(object, context) &&
        object.depth < semantics::derivation_depth && !object.owns_pointee;
    const bool pointer = object.depth && context.kind(object.layers[0]).adopted;
    const bool handle = object.depth && semantics::owned(object.origin);
    const bool bound =
        object.depth >= 2 && context.kind(object.layers[1]).bounded;
    return wraps || pointer || handle || bound;
  }

  // The states a route of the binding reaches, of a value (Value) or not: of
  // an answer, or of a derivation that takes steps of the request after it.
  struct routed_list {
    bool through[closure.count + 1] = {};
    bool value[closure.count + 1] = {};
  };
  static constexpr routed_list make_routed() {
    routed_list made;
    for (std::size_t index = 0; index < shapes.count * shape_answer_count;
         ++index) {
      for (const closed_answer &each :
           {answers.entries[index].stable, answers.entries[index].transient}) {
        if (each.found) {
          (each.value_route ? made.value : made.through)[each.route] = true;
        }
      }
    }
    for (std::size_t index = 0; index < closure.count; ++index) {
      if (possible(index) && starts_steps(closure.states[index].reached)) {
        made.through[index] = true;
      }
    }
    return made;
  }
  static constexpr routed_list routed = make_routed();

  // The pointers an answer gives what they point to of.
  struct pointed_list {
    bool entries[closure.count + 1] = {};
  };
  static constexpr pointed_list make_pointed() {
    pointed_list made;
    for (std::size_t index = 0; index < shapes.count * shape_answer_count;
         ++index) {
      for (const closed_answer &each :
           {answers.entries[index].stable, answers.entries[index].transient}) {
        if (each.found && each.access == semantics::access::point) {
          made.entries[each.route] = true;
        }
      }
    }
    return made;
  }
  static constexpr pointed_list pointed = make_pointed();
  static constexpr bool any_routed_value() {
    for (std::size_t index = 0; index < closure.count; ++index) {
      if (routed.value[index]) {
        return true;
      }
    }
    return false;
  }
  static constexpr bool any_value = any_routed_value();
};

} // namespace dingo::detail::execution
