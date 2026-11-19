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
  return a.origin == b.origin && semantics::same_stack(a, b, true);
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
  // It takes a step of every kind a binding does not take, of every outer
  // layer.
  constexpr bool takes(semantics::step kind, semantics::layer,
                       std::size_t) const {
    return !semantics::binding_kind(kind);
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
  // The operations a runtime conversion calls of each class of states: a
  // sole owner converted to the sharing handle its type traits name, and a
  // handle of an array of a known bound to the handle of an array of unknown
  // bound they name, which a request may bring into the universe; and the
  // wraps that make it, which a request may make it by. Every other step
  // from a state of the closure is the closure's, which its routes take, as
  // the code compiles: a table holds none of them.
  struct mask_list {
    std::uint32_t state_mask[semantics::max_states] = {};
  };
  static constexpr mask_list make_masks() {
    mask_list made;
    for (std::size_t index = 0; index < used.count; ++index) {
      const std::size_t state = used.states[index];
      const semantics::state &each = Sources.states[state];
      made.state_mask[classes.state_class[state]] |=
          operation_mask(semantics::step::transfer) |
          operation_mask(semantics::step::unknown_bound);
      if (state != 0 && step_makes(each.step)) {
        made.state_mask[classes.state_class[state]] |= makers_mask;
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
  // wrap, in the layers whose types the binding has: what a request reaches
  // from. The
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
// closure covers, of each category and qualifiers of the object it is of.
template <typename Semantic, std::uint64_t... Possible>
struct closed_answer_table {
  // The bound is spelled: Clang before 22 does not index an array of a bound
  // deduced from its initializer in a constant expression of the class.
  static constexpr std::uint64_t possible_bits[sizeof...(Possible) + 1] = {
      Possible..., 0};
  static constexpr bool possible(std::size_t state) {
    return ((possible_bits[state / 64] >> (state % 64)) & 1u) != 0;
  }
  static constexpr const semantics::search &closure = Semantic::closure;
  static constexpr const auto &route_finishes = Semantic::route_finishes;
  // The request shapes the closure covers: that of each state C++
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
  static constexpr void add_shape(shape_list &made,
                                  const semantics::request &goal,
                                  std::uint16_t state, bool pointee) {
    for (std::size_t index = 0; index < made.count; ++index) {
      if (semantics::same_stack(made.goals[index], goal, false)) {
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
      semantics::set_stack(goal, object);
      goal.shape_depth = goal.depth;
      add_shape(made, goal, static_cast<std::uint16_t>(state), false);
      if (object.depth && context.kind(object.layers[0]).adopted) {
        semantics::request pointed_request;
        semantics::set_stack(pointed_request, object, 1);
        pointed_request.shape_depth = pointed_request.depth;
        add_shape(made, pointed_request, static_cast<std::uint16_t>(state),
                  true);
      }
    }
    return made;
  }
  static constexpr shape_list shapes = make_shapes();

  // The answer of the closure of a request, as satisfy_states finds it.
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

  // The answers of every shape the closure covers, of each category and
  // qualifiers of the object a request is of, for each lifetime.
  struct answer_list {
    closed_answers entries[shapes.count * shape_answer_count + 1] = {};
  };
  // The answers of a shape for a category, of each qualifiers of the object a
  // request is of. Each is a constant of its own: the answers of a whole
  // closure take more steps than a compiler evaluates in one constant.
  struct category_answers {
    closed_answers entries[4] = {};
  };
  static constexpr category_answers make_category_answers(std::size_t shape,
                                                          unsigned taken) {
    category_answers made;
    const auto category = static_cast<semantics::category>(taken);
    for (std::uint8_t quals = 0; quals < 4; ++quals) {
      semantics::request goal = shapes.goals[shape];
      goal.quals[0] = quals;
      made.entries[quals] = {
          find_answer(goal, category, semantics::lifetime::stable),
          find_answer(goal, category, semantics::lifetime::request)};
    }
    return made;
  }
  template <std::size_t Index>
  static constexpr category_answers category_answers_v =
      make_category_answers(Index / 7, Index % 7);
  template <std::size_t... I>
  static constexpr answer_list make_answers(std::index_sequence<I...>) {
    answer_list made;
    const category_answers *each[] = {&category_answers_v<I>..., nullptr};
    for (std::size_t index = 0; index < sizeof...(I); ++index) {
      for (std::size_t quals = 0; quals < 4; ++quals) {
        made.entries[index * 4 + quals] = each[index]->entries[quals];
      }
    }
    return made;
  }
  static constexpr answer_list answers =
      make_answers(std::make_index_sequence<shapes.count * 7>{});

  // The layer sources an extension of a request the closure does not satisfy
  // starts from: of each type and form, the state of the fewest copies and
  // moves of the leaf, of the states C++ makes.
  struct source_list {
    std::uint16_t states[closure.count + 1] = {};
    std::size_t count = 0;
  };
  static constexpr source_list make_layer_sources() {
    source_list made;
    const semantics::context context = Semantic::context();
    for (std::size_t state = 0; state < closure.count; ++state) {
      const semantics::state &each = closure.states[state];
      if (!possible(state) || !semantics::extensible(each.reached, context)) {
        continue;
      }
      const std::uint16_t code = semantics::object_form(each.reached).code();
      std::size_t found = 0;
      while (found < made.count) {
        const semantics::state &other = closure.states[made.states[found]];
        if (semantics::object_form(other.reached).code() == code &&
            semantics::same_stack(other.reached, each.reached, false)) {
          break;
        }
        ++found;
      }
      if (found == made.count) {
        made.states[made.count++] = static_cast<std::uint16_t>(state);
      } else {
        const semantics::state &other = closure.states[made.states[found]];
        if (semantics::cheaper(each.made, each.reached.absent, other.made,
                               other.reached.absent)) {
          made.states[found] = static_cast<std::uint16_t>(state);
        }
      }
    }
    return made;
  }
  static constexpr source_list layer_sources = make_layer_sources();

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
    for (std::size_t index = 0; index < layer_sources.count; ++index) {
      made.through[layer_sources.states[index]] = true;
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
