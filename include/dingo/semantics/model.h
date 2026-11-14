//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/base/config.h>

#include <cstddef>
#include <cstdint>

// The semantics of resolution as values: what a stored object reaches by the
// steps a binding and a request take, how long each object lives, and what the
// steps copy and move of the leaf. It is the model of spec/semantics.py, step
// by step, of no C++ type: a test compares it with the specification for every
// stored object and request. See docs/architecture/layered-semantics.md.
//
// The rules name no wrapper. A layer is its properties, in the universe of the
// layers a search may make: the layers of C++ itself, a pointer and an array,
// and those of the types a request and a stored object are made of, which
// their type traits describe.

namespace dingo::semantics {

// A layer: its index in the universe of a search.
using layer = std::uint8_t;

inline constexpr layer no_layer = 0xFF;

// The layers of C++ itself, first in every universe: a pointer to an object,
// the pointer an array decays to, to its first element, an array of unknown
// bound inside a handle, and a bounded array. The element of an array is its
// first element, through the pointer it decays to.
inline constexpr layer pointer_layer = 0;
inline constexpr layer array_pointer_layer = 1;
inline constexpr layer array_layer = 2;
inline constexpr layer bounded_array_layer = 3;

// What a layer owns of what it holds.
enum class owner : std::uint8_t { none, sole, shared };

// A layer, by its properties.
struct layer_kind {
  owner owns = owner::sole;
  // Whether a copy of the layer holds the same element.
  bool shares_element = false;
  bool copyable = false;
  // Whether a layer of an element can be made from a value of it.
  bool made_from_value = false;
  // Whether it is made only of a bare element, as an alternative type is.
  bool holds_element_only = false;
  bool relocatable = true;
  // Whether its element can be taken from it.
  bool gives_element = true;
  // Whether it is an array of a known bound.
  bool bounded = false;
  // Whether it may hold no element, and whether an empty one is a value.
  bool may_be_empty = false;
  bool has_empty_value = false;
  // Whether a handle of it takes over a raw pointer the request owns.
  bool adopts = false;
  // Of C++ itself: whether it decays to the pointer to its first element;
  // whether it is a raw pointer a handle takes over, and whether the handle
  // holds an array around its element.
  bool decays = false;
  bool adopted = false;
  bool adopted_array = false;
  // The layers an owned value of it converts to, as a mask of their indices.
  std::uint32_t converts = 0;
};

// Whether a type can be a layer of these properties, as spec/semantics.py
// says of a layer. A layer that owns nothing, or shares what it owns, holds
// an element that lives apart from it: a copy of it holds the same one. A
// layer that owns its element alone and shares it with a copy cannot be
// copied: the copy would share what one of them owns. A layer that owns
// nothing is not made of a value and takes over no pointer. A layer that
// takes over a pointer holds the element apart from it. An empty value is a
// value only of a layer that may be empty.
constexpr bool possible(const layer_kind &kind) {
  return (kind.owns == owner::sole || kind.shares_element) &&
         !(kind.owns == owner::sole && kind.shares_element && kind.copyable) &&
         !(kind.owns == owner::none && (kind.made_from_value || kind.adopts)) &&
         (!kind.adopts || kind.shares_element) &&
         (kind.may_be_empty || !kind.has_empty_value);
}

// Whether an owned value of a layer can convert to another, taking over or
// sharing its element: only a layer that owns its element alone hands it
// over, and only to a layer that owns an element apart from it.
constexpr bool possible_conversion(const layer_kind &source,
                                   const layer_kind &target) {
  return source.owns == owner::sole && source.shares_element &&
         target.owns != owner::none && target.shares_element;
}

inline constexpr std::size_t max_kinds = 32;

// The layers a search may make: those of C++ itself, then those of the types
// it is of.
struct universe {
  layer_kind kinds[max_kinds] = {
      // A pointer to an object.
      {owner::none, true, true, false, false, true, true, false, true, false,
       false, false, true, false},
      // The pointer an array decays to.
      {owner::none, true, true, false, false, true, false, false, false, false,
       false, false, true, true},
      // An array of unknown bound.
      {owner::sole, false, false, false, false, false, false, false, false,
       false, false, true, false, false},
      // A bounded array.
      {owner::sole, false, false, false, false, false, false, true, false,
       false, false, true, false, false}};
  std::size_t count = 4;

  constexpr layer add(const layer_kind &kind) {
    kinds[count] = kind;
    return static_cast<layer>(count++);
  }
};

// What the leaf can do.
struct leaf_traits {
  bool copyable;
  bool movable;
  bool default_constructible = true;
  bool made_by_factory = false;
};

// What a scope does with its instance: makes a new one the request owns, is
// given it, or publishes it before constructing it in place.
struct scope_traits {
  bool makes;
  bool given = false;
  bool publishes_early = false;
};

// How long an object lives: kept as long as the container, or made for the
// request.
enum class lifetime : std::uint8_t { request = 1, stable = 2 };

// Whose an object is: kept by another, made for the request, or a copy the
// request made.
enum class origin : std::uint8_t { borrowed, made, copied };

constexpr bool owned(origin of) { return of != origin::borrowed; }

// The element of a stack of layers.
enum class element : std::uint8_t { leaf, interface };

// The known gaps of the library, which the expectations apply on top of the
// rules.
enum class limitation : std::uint8_t {
  pointer_out_of_made_holder = 1 << 0,
  pointer_into_converted_copy = 1 << 1,
};

// What a derivation knows of its registration, and the layers it may make.
struct context {
  leaf_traits leaf;
  scope_traits scope;
  // Whether the leaf is requested as its abstract interface.
  bool abstract;
  // The limitations applied, as a mask of limitation.
  std::uint8_t limits = 0;
  const universe *layers = nullptr;

  constexpr bool limited(limitation gap) const {
    return (limits & static_cast<std::uint8_t>(gap)) != 0;
  }

  constexpr const layer_kind &kind(layer of) const { return layers->kinds[of]; }
};

// The most layers a thing has: a request has at most three wrappers around
// its interface, which may be a layer of its own, and an adopted pointer of
// an array becomes a handle of the array.
inline constexpr std::size_t max_layers = 6;

// An object a derivation reaches, as spec/semantics.py describes a Thing.
struct thing {
  layer layers[max_layers] = {};
  std::uint8_t depth = 0;
  semantics::element of = semantics::element::leaf;
  // How long each layer and the element live, outermost first.
  lifetime lifetimes[max_layers + 1] = {};
  semantics::origin from = semantics::origin::borrowed;
  bool instance = false;
  // The holder made for the request it is the element of, or none.
  layer contained = no_layer;
  bool unconstructed = false;
  bool absent = false;
  std::uint8_t full = 0;

  constexpr lifetime life() const { return lifetimes[0]; }
};

constexpr bool operator==(const thing &a, const thing &b) {
  if (a.depth != b.depth || a.of != b.of || a.from != b.from ||
      a.instance != b.instance || a.contained != b.contained ||
      a.unconstructed != b.unconstructed || a.absent != b.absent ||
      a.full != b.full) {
    return false;
  }
  for (std::size_t index = 0; index < a.depth; ++index) {
    if (a.layers[index] != b.layers[index]) {
      return false;
    }
  }
  for (std::size_t index = 0; index <= a.depth; ++index) {
    if (a.lifetimes[index] != b.lifetimes[index]) {
      return false;
    }
  }
  return true;
}

// A thing with its outer layers replaced: the layers of front, then those of
// the thing from below on; the lifetimes given for front, then those of the
// thing from below on.
constexpr thing restacked(const thing &from, const layer *front,
                          std::size_t count, std::size_t below,
                          const lifetime *lives) {
  thing made = from;
  std::size_t depth = 0;
  for (std::size_t index = 0; index < count; ++index) {
    made.lifetimes[depth] = lives[index];
    made.layers[depth++] = front[index];
  }
  for (std::size_t index = below; index <= from.depth; ++index) {
    made.lifetimes[depth] = from.lifetimes[index];
    if (index < from.depth) {
      made.layers[depth] = from.layers[index];
    }
    ++depth;
  }
  made.depth = static_cast<std::uint8_t>(depth - 1);
  for (std::size_t index = made.depth; index < max_layers; ++index) {
    made.layers[index] = layer{};
  }
  for (std::size_t index = depth; index <= max_layers; ++index) {
    made.lifetimes[index] = lifetime{};
  }
  return made;
}

// A thing with one more layer around it, which lives as the given lifetime.
constexpr thing wrapped(const thing &of, layer outer, lifetime life) {
  return restacked(of, &outer, 1, 0, &life);
}

// How many of the layers from one of them on a copy or a move of a stack
// makes anew: those down to the first that shares its element; all of them
// and the element when none does.
constexpr std::size_t copied_layers(const thing &of, const context &with,
                                    std::size_t from = 0) {
  for (std::size_t depth = 1; from + depth <= of.depth; ++depth) {
    if (with.kind(of.layers[from + depth - 1]).shares_element) {
      return depth;
    }
  }
  return of.depth - from + 1;
}

constexpr bool makes_leaf_anew(const thing &of, const context &with,
                               std::size_t from = 0) {
  return copied_layers(of, with, from) > of.depth - from;
}

// Whether a new object of a thing would slice the leaf: the interface
// without a layer is the base the leaf is.
constexpr bool slices(const thing &of) {
  return of.of == element::interface && of.depth == 0;
}

constexpr bool copies_element(const thing &of, const context &with) {
  if (with.scope.publishes_early && of.instance && of.from != origin::copied) {
    return false;
  }
  return with.leaf.copyable;
}

constexpr bool can_copy(const thing &of, const context &with) {
  if (slices(of)) {
    return false;
  }
  const std::size_t depth = copied_layers(of, with);
  for (std::size_t index = 0; index < depth && index < of.depth; ++index) {
    if (!with.kind(of.layers[index]).copyable) {
      return false;
    }
  }
  return depth <= of.depth || copies_element(of, with);
}

constexpr bool can_move(const thing &of, const context &with) {
  const std::size_t depth = copied_layers(of, with);
  for (std::size_t index = 0; index < depth && index < of.depth; ++index) {
    if (!with.kind(of.layers[index]).relocatable) {
      return false;
    }
  }
  return depth <= of.depth || with.leaf.movable;
}

// A kind of step, as the specification names it.
enum class step : std::uint8_t {
  decay,
  hand_over,
  adopt,
  element,
  element_pointer,
  element_or_empty,
  element_or_empty_copy,
  convert,
  upcast,
  copy,
  wrap,
};

struct transition {
  step kind;
  thing reached;
};

// The most steps from one thing.
inline constexpr std::size_t max_transitions = 2 * max_kinds + 8;

struct transitions_list {
  transition items[max_transitions] = {};
  std::size_t count = 0;

  constexpr void add(step kind, const thing &reached) {
    items[count++] = {kind, reached};
  }

  constexpr void operator()(step kind, const thing &reached) {
    add(kind, reached);
  }
};

// The deepest stack a derivation makes.
inline constexpr std::size_t derivation_depth = 4;

// The thing of a copy: a new object the request owns. What a layer that
// shares its element holds is not copied.
constexpr thing copied(const thing &of, const context &with) {
  const std::size_t depth = copied_layers(of, with);
  thing made = of;
  for (std::size_t index = 0; index < depth && index <= of.depth; ++index) {
    made.lifetimes[index] = lifetime::request;
  }
  made.from = origin::copied;
  made.instance = of.instance && depth <= of.depth;
  return made;
}

// What is derived of the element of a layer that may be empty without
// failing when it is: a pointer to the element, and the optional the layer
// holds.
template <typename Sink>
constexpr void empty_safe_steps(const thing &of, const thing &inner,
                                const context &with, Sink &out) {
  const layer below = inner.depth ? inner.layers[0] : no_layer;
  thing pointer;
  if (below != no_layer && with.kind(below).decays) {
    pointer = inner;
    pointer.layers[0] = array_pointer_layer;
    pointer.lifetimes[0] = of.lifetimes[0];
  } else {
    pointer = wrapped(inner, pointer_layer, of.lifetimes[0]);
  }
  pointer.from = origin::borrowed;
  pointer.absent = of.absent;
  out(step::element_pointer, pointer);
  if (below != no_layer && with.kind(below).has_empty_value &&
      !inner.unconstructed) {
    if (owned(inner.from)) {
      thing same = inner;
      same.absent = of.absent;
      out(step::element_or_empty, same);
    } else if (can_copy(inner, with)) {
      thing copy = copied(inner, with);
      copy.absent = of.absent;
      out(step::element_or_empty_copy, copy);
    }
  }
}

// The steps the semantics allow from a thing.
// They are passed to a sink, out(kind, thing), one by one: a constant
// expression returns no list of them, which a compiler would keep.
template <typename Sink>
constexpr void each_transition(const thing &of, const context &with,
                               Sink &out) {
  if (of.unconstructed) {
    return;
  }
  const lifetime request = lifetime::request;
  if (of.depth) {
    const layer outer = of.layers[0];
    const layer_kind &kind = with.kind(outer);
    if (kind.decays) {
      out(step::decay, restacked(of, &array_pointer_layer, 1, 1, &request));
      if (of.from == origin::made) {
        out(step::hand_over, wrapped(of, pointer_layer, request));
      }
    } else if (kind.adopted && of.from == origin::made) {
      for (std::size_t index = 0; index < with.layers->count; ++index) {
        if (!with.layers->kinds[index].adopts) {
          continue;
        }
        const layer front[] = {static_cast<layer>(index), array_layer};
        const lifetime lives[] = {request, request};
        out(step::adopt,
            restacked(of, front, kind.adopted_array ? 2 : 1, 1, lives));
      }
    } else if (kind.gives_element) {
      const bool made = of.lifetimes[1] == lifetime::request;
      thing inner = restacked(of, nullptr, 0, 1, nullptr);
      inner.from = kind.owns == owner::sole ? of.from : origin::borrowed;
      inner.contained = of.contained != no_layer ? of.contained
                        : made                   ? outer
                                                 : no_layer;
      inner.unconstructed =
          with.scope.publishes_early && of.instance && of.depth > 1;
      inner.absent = of.absent || (kind.may_be_empty && !of.full);
      inner.full = static_cast<std::uint8_t>(of.full ? of.full - 1 : 0);
      out(step::element, inner);
      if (kind.may_be_empty && !of.full && !kind.adopted) {
        empty_safe_steps(of, inner, with, out);
      }
    }
    if (owned(of.from)) {
      for (std::size_t index = 0; index < with.layers->count; ++index) {
        if (kind.converts & (std::uint32_t{1} << index)) {
          const layer to = static_cast<layer>(index);
          out(step::convert, restacked(of, &to, 1, 1, &request));
        }
      }
    }
  }
  if (of.of == element::leaf && with.abstract) {
    const layer_kind *outer = of.depth ? &with.kind(of.layers[0]) : nullptr;
    thing upcast = of;
    upcast.of = element::interface;
    if (outer != nullptr && !outer->gives_element) {
      // The pointer to the first element of an array is a pointer to that
      // element as the interface, not to an array of the interface: the
      // request does not own what it points to.
      if (of.depth == 1 && outer->shares_element) {
        upcast.layers[0] = pointer_layer;
        upcast.from = origin::borrowed;
        out(step::upcast, upcast);
      }
    } else if (outer == nullptr ||
               (of.depth == 1 && outer->shares_element && outer->copyable)) {
      out(step::upcast, upcast);
    } else if (of.depth == 1 && outer->shares_element && owned(of.from)) {
      upcast.lifetimes[0] = request;
      out(step::upcast, upcast);
    } else if (of.depth > 1) {
      const std::size_t depth = copied_layers(of, with);
      bool copyable = true;
      for (std::size_t index = 0; index < of.depth; ++index) {
        copyable = copyable && with.kind(of.layers[index]).copyable;
      }
      if (depth == of.depth && copyable) {
        upcast.from = origin::copied;
        upcast.instance = false;
        for (std::size_t index = 0; index < depth; ++index) {
          upcast.lifetimes[index] = request;
        }
        out(step::upcast, upcast);
      }
    }
  }
  if (can_copy(of, with)) {
    out(step::copy, copied(of, with));
  }
  const bool shares = of.from == origin::copied && of.instance;
  if ((of.from == origin::made || shares) && can_move(of, with) &&
      of.depth < derivation_depth) {
    const std::size_t depth = copied_layers(of, with);
    thing inner = of;
    lifetime outer = request;
    if (shares && of.lifetimes[depth] == lifetime::stable) {
      for (std::size_t index = 0; index < depth; ++index) {
        inner.lifetimes[index] = lifetime::stable;
      }
      outer = lifetime::stable;
    }
    for (std::size_t index = 0; index < with.layers->count; ++index) {
      const layer_kind &kind = with.layers->kinds[index];
      if (!kind.made_from_value || (kind.holds_element_only && of.depth) ||
          (slices(of) && !kind.shares_element)) {
        continue;
      }
      thing made = wrapped(inner, static_cast<layer>(index), outer);
      made.full = static_cast<std::uint8_t>(of.full + 1);
      out(step::wrap, made);
    }
  }
}

constexpr transitions_list transitions(const thing &of, const context &with) {
  transitions_list out;
  each_transition(of, with, out);
  return out;
}

// The steps from a thing the library takes: those the semantics allow that no
// limitation blocks. A pointer into a copy a binding converts is not taken.
// What a sink of the steps the library takes passes on: a step the
// limitations do not block.
template <typename Sink> struct unblocked {
  const thing &from;
  const context &with;
  Sink &out;

  constexpr void operator()(step kind, const thing &reached) {
    if (kind == step::element_pointer && from.from == origin::copied &&
        with.limited(limitation::pointer_into_converted_copy)) {
      return;
    }
    out(kind, reached);
  }
};

template <typename Sink>
constexpr void each_step(const thing &of, const context &with, Sink &out) {
  unblocked<Sink> filtered{of, with, out};
  each_transition(of, with, filtered);
}

constexpr transitions_list steps(const thing &of, const context &with) {
  transitions_list out;
  each_step(of, with, out);
  return out;
}

// The copies and the moves of the leaf a derivation makes.
struct cost {
  std::uint8_t copies = 0;
  std::uint8_t moves = 0;
};

constexpr bool operator==(cost a, cost b) {
  return a.copies == b.copies && a.moves == b.moves;
}

constexpr bool operator<(cost a, cost b) {
  return a.copies != b.copies ? a.copies < b.copies : a.moves < b.moves;
}

// What a step does to the leaf.
enum class effect : std::uint8_t { none, copy, move };

constexpr effect effect_of(step kind, const thing &of, const context &with) {
  if (kind == step::element_or_empty_copy) {
    return makes_leaf_anew(of, with, 1) ? effect::copy : effect::none;
  }
  if ((kind == step::copy || kind == step::wrap) && makes_leaf_anew(of, with)) {
    return kind == step::copy ? effect::copy : effect::move;
  }
  return effect::none;
}

// The copies and the moves after an effect: a copy is made where it is moved
// to.
constexpr cost affected(cost before, effect made) {
  if (made == effect::copy) {
    return {1, before.moves};
  }
  if (made == effect::move && !before.copies) {
    return {before.copies, 1};
  }
  return before;
}

// What a request asks for: the layers around its element, of which the
// outer shape_depth are of the shape of the request and the rest of the
// interface it is of.
struct request {
  layer layers[max_layers] = {};
  std::uint8_t depth = 0;
  semantics::element of = semantics::element::leaf;
  std::uint8_t shape_depth = 0;
};

// Whether the layers of a thing may still become the layers of a request: a
// layer is wrapped around a thing only on the way to the request, as its
// layers are, or as a layer that converts to its outermost one. A layer the
// binding keeps with a copy that shares what it keeps is wrapped around
// anything that may become the request too: its element lives as long as
// the binding. The search it directs reaches what satisfies a request at the
// same cost as the search of every thing; the test of the semantics checks
// it for every request.
constexpr bool may_become(const thing &of, const request &goal,
                          const context &with) {
  if (of.depth > goal.depth) {
    return false;
  }
  const std::size_t offset = goal.depth - of.depth;
  for (std::size_t index = 1; index < of.depth; ++index) {
    if (of.layers[index] != goal.layers[offset + index]) {
      return false;
    }
  }
  return of.depth == 0 || of.layers[0] == goal.layers[offset] ||
         (with.kind(of.layers[0]).converts &
          (std::uint32_t{1} << goal.layers[offset])) != 0;
}

inline constexpr std::uint16_t no_state = 0xFFFF;

// A thing a derivation reaches, with the copies and the moves of the leaf of
// one derivation that reaches it, and the state and the step it reached the
// thing from first.
struct state {
  thing reached;
  cost made;
  std::uint16_t parent = no_state;
  step via = step::decay;
};

inline constexpr std::size_t max_states = 512;

// The states a stored object reaches on the way to a request.
struct search {
  state states[max_states] = {};
  std::size_t count = 0;
  // Whether there were more states than the search holds: its answer is not
  // to be trusted.
  bool overflow = false;

  constexpr bool add(const state &found) {
    for (std::size_t index = 0; index < count; ++index) {
      if (states[index].made == found.made &&
          states[index].reached == found.reached) {
        return false;
      }
    }
    if (count == max_states) {
      overflow = true;
      return false;
    }
    states[count++] = found;
    return true;
  }
};

// What reach passes a step to: a wrap only into a layer on the way to the
// request, or one the binding keeps.
struct reached_sink {
  search &found;
  const state &from;
  std::uint16_t index;
  const request &goal;
  const context &with;

  constexpr void operator()(step kind, const thing &reached) {
    if (kind == step::wrap && !may_become(reached, goal, with) &&
        !(reached.lifetimes[0] == lifetime::stable &&
          may_become(from.reached, goal, with))) {
      return;
    }
    found.add({reached,
               affected(from.made, effect_of(kind, from.reached, with)), index,
               kind});
  }
};

constexpr search reach(const thing &start, const context &with,
                       const request &goal) {
  search found;
  found.add({start, {}, no_state, step::decay});
  for (std::size_t next = 0; next < found.count; ++next) {
    const state from = found.states[next];
    reached_sink sink{found, from, static_cast<std::uint16_t>(next), goal,
                      with};
    each_step(from.reached, with, sink);
  }
  return found;
}

// How a request takes the layers it asks for.
enum class category : std::uint8_t {
  value,
  const_value,
  lvalue_reference,
  const_lvalue_reference,
  rvalue_reference,
  pointer,
  const_pointer,
};

constexpr bool refers(category of) {
  return of == category::lvalue_reference ||
         of == category::const_lvalue_reference || of == category::pointer ||
         of == category::const_pointer;
}

constexpr bool points(category of) {
  return of == category::pointer || of == category::const_pointer;
}

constexpr bool has_layers(const thing &of, const request &goal,
                          std::size_t from = 0) {
  if (of.depth != goal.depth + from) {
    return false;
  }
  for (std::size_t index = 0; index < goal.depth; ++index) {
    if (of.layers[from + index] != goal.layers[index]) {
      return false;
    }
  }
  return true;
}

// Whether a thing is a pointer value a pointer request takes: one the request
// owns, or one whose pointee lives long enough.
constexpr bool points_to(const thing &of, const request &goal, lifetime needed,
                         const context &with) {
  if (!of.depth || !with.kind(of.layers[0]).adopted ||
      !has_layers(of, goal, 1) || !of.instance) {
    return false;
  }
  if (of.from == origin::made &&
      !(with.limited(limitation::pointer_out_of_made_holder) &&
        of.contained != no_layer)) {
    return true;
  }
  return static_cast<int>(of.lifetimes[1]) >= static_cast<int>(needed);
}

// How a thing satisfies a request: a reference or a pointer refers to it, a
// pointer request takes it as the pointer value it is, an rvalue reference
// takes what is made for the request, or a value is moved or copied of it.
enum class satisfaction : std::uint8_t { refer, point, take, move, copy };

// The fewest copies and moves of the leaf of what satisfies a request, as the
// specification's satisfactions give it, or none; the state of the search
// that satisfies it at that cost, the first one found, and how.
struct answer {
  bool found = false;
  cost fewest;
  bool overflow = false;
  std::uint16_t state = no_state;
  satisfaction how = satisfaction::refer;
  // Whether what satisfies it may not exist, as the element of a layer that
  // may be empty: the request fails when it does not.
  bool absent = false;
};

// The answer of what satisfies a request at fewer copies and moves, or at as
// few and defined when a layer is empty where the answer so far is not, as
// the specification's fewest prefers it.
constexpr void consider(answer &result, cost made, std::size_t index,
                        satisfaction how, bool absent) {
  if (!result.found || made < result.fewest ||
      (made == result.fewest && result.absent && !absent)) {
    result.found = true;
    result.fewest = made;
    result.state = static_cast<std::uint16_t>(index);
    result.how = how;
    result.absent = absent;
  }
}

// What satisfying a request does to the leaf, of the copies and the moves
// that reach what satisfies it and the effect of satisfying it of that.
struct affected_cost {
  constexpr cost operator()(cost made, effect of, const thing &) const {
    return affected(made, of);
  }
};

template <typename Finish>
constexpr answer satisfy_by(const search &found, const request &goal,
                            category taken, lifetime needed,
                            const context &with, Finish finish) {
  answer result;
  result.overflow = found.overflow;
  if (refers(taken)) {
    for (std::size_t index = 0; index < found.count; ++index) {
      const state &each = found.states[index];
      const thing &of = each.reached;
      if (of.of == goal.of && has_layers(of, goal) && of.instance &&
          static_cast<int>(of.life()) >= static_cast<int>(needed)) {
        consider(result, finish(each.made, effect::none, of), index,
                 satisfaction::refer, of.absent);
      }
    }
    if (result.found || !points(taken)) {
      return result;
    }
    for (std::size_t index = 0; index < found.count; ++index) {
      const state &each = found.states[index];
      if (each.reached.of == goal.of &&
          points_to(each.reached, goal, needed, with)) {
        consider(result, finish(each.made, effect::none, each.reached), index,
                 satisfaction::point, each.reached.absent);
      }
    }
    return result;
  }
  for (std::size_t index = 0; index < found.count; ++index) {
    const state &each = found.states[index];
    const thing &of = each.reached;
    if (of.of != goal.of || !has_layers(of, goal)) {
      continue;
    }
    if (taken == category::rvalue_reference) {
      if (of.from == origin::made && can_move(of, with)) {
        consider(result, finish(each.made, effect::none, of), index,
                 satisfaction::take, of.absent);
      }
      continue;
    }
    if (slices(of) || of.unconstructed) {
      continue;
    }
    const bool anew = makes_leaf_anew(of, with);
    if (owned(of.from) && can_move(of, with)) {
      consider(result,
               finish(each.made, anew ? effect::move : effect::none, of), index,
               satisfaction::move, of.absent);
    }
    if (can_copy(of, with)) {
      consider(result,
               finish(each.made, anew ? effect::copy : effect::none, of), index,
               satisfaction::copy, of.absent);
    }
  }
  return result;
}

constexpr answer satisfy(const search &found, const request &goal,
                         category taken, lifetime needed, const context &with) {
  return satisfy_by(found, goal, taken, needed, with, affected_cost{});
}

constexpr answer fewest(const thing &start, const context &with,
                        const request &goal, category taken, lifetime needed) {
  return satisfy(reach(start, with, goal), goal, taken, needed, with);
}

inline constexpr std::size_t max_plan = 16;

// The steps from a stored object to what satisfies a request, and the things
// they reach, the first the stored object; and how the last satisfies it.
struct plan {
  bool found = false;
  step steps[max_plan] = {};
  thing things[max_plan + 1] = {};
  std::size_t count = 0;
  satisfaction how = satisfaction::refer;
  cost fewest;
};

constexpr plan plan_of(const search &found, const answer &result) {
  plan made;
  if (!result.found) {
    return made;
  }
  std::size_t path[max_plan + 1] = {};
  std::size_t length = 0;
  for (std::uint16_t at = result.state; at != no_state;
       at = found.states[at].parent) {
    if (length == max_plan + 1) {
      // A plan longer than a plan holds is none.
      return made;
    }
    path[length++] = at;
  }
  made.found = true;
  made.count = length - 1;
  for (std::size_t index = 0; index < length; ++index) {
    const state &each = found.states[path[length - 1 - index]];
    made.things[index] = each.reached;
    if (index) {
      made.steps[index - 1] = each.via;
    }
  }
  made.how = result.how;
  made.fewest = result.fewest;
  return made;
}

// Where a request stands: itself, the dependency of an unregistered class,
// or the dependency of a class a shared or a cyclical scope keeps.
enum class position : std::uint8_t { direct, consumer, shared, cyclical };

// The entry point a request is made through.
enum class entry : std::uint8_t { resolve, invoke, construct };

// What a request is, beside what it asks for: where it stands, the entry
// point it is made through, whether a key or an annotation selects it, and
// whether the runtime container resolves it.
struct request_rules {
  position at;
  entry through;
  bool selected;
  bool runtime;
};

// Whether a request in a category holds the shape by value: a resolved
// rvalue reference is returned by value.
constexpr bool by_value(category of) {
  return of == category::value || of == category::const_value ||
         of == category::rvalue_reference;
}

// How long an object a request refers to has to live: a reference that is
// passed away, returned by a resolve or a construct or held by a class one of
// them returns, as long as the container; one a callable takes, or a class
// a shared or a cyclical scope keeps, while the request does.
constexpr lifetime required_lifetime(position at, entry through) {
  if (at == position::shared || at == position::cyclical ||
      through == entry::invoke) {
    return lifetime::request;
  }
  return lifetime::stable;
}

// The entry a request is resolved through: construct<T>() of a reference, of
// an rvalue reference, or of a request a key or an annotation selects,
// resolves it.
constexpr entry entry_of(entry through, position at, bool selected,
                         category of) {
  if (at == position::direct && through == entry::construct &&
      (selected || !by_value(of) || of == category::rvalue_reference)) {
    return entry::resolve;
  }
  return through;
}

// The layers of a request as a thing, to ask what its stack can do.
constexpr thing stack_of(const request &goal) {
  thing of;
  for (std::size_t index = 0; index < goal.depth; ++index) {
    of.layers[index] = goal.layers[index];
  }
  of.depth = goal.depth;
  of.of = goal.of;
  return of;
}

// Whether what a copy or a move of a stack makes anew can be: each layer
// down to the first that shares its element, and the element when none does.
template <typename Can>
constexpr bool stack_can(const thing &of, const context &with, Can layer_can,
                         bool element_can) {
  const std::size_t depth = copied_layers(of, with);
  for (std::size_t index = 0; index < depth && index < of.depth; ++index) {
    if (!layer_can(with.kind(of.layers[index]))) {
      return false;
    }
  }
  return depth <= of.depth || element_can;
}

struct copyable_layer {
  constexpr bool operator()(const layer_kind &kind) const {
    return kind.copyable;
  }
};

struct relocatable_layer {
  constexpr bool operator()(const layer_kind &kind) const {
    return kind.relocatable;
  }
};

// Whether a value of the stack of a request can be copied.
constexpr bool copyable_shape(const request &goal, const context &with) {
  return stack_can(stack_of(goal), with, copyable_layer{}, with.leaf.copyable);
}

// Whether a value of the stack of a request can be moved or copied.
constexpr bool stack_relocatable(const request &goal, const context &with) {
  return stack_can(stack_of(goal), with, relocatable_layer{},
                   with.leaf.movable) ||
         copyable_shape(goal, with);
}

// Whether a request a key or an annotation selects cannot be held by value
// in its selector, which it is passed in: a const value is copied into it,
// another one moved or copied. A direct resolve returns the value instead.
constexpr bool selector_rejects(const request &goal, category of, position at,
                                entry through, bool selected,
                                const context &with) {
  if (!selected || (of != category::value && of != category::const_value) ||
      (at == position::direct && through == entry::resolve)) {
    return false;
  }
  if (of == category::const_value) {
    return !copyable_shape(goal, with);
  }
  return !stack_relocatable(goal, with);
}

// The copies and the moves after a request is handed what satisfies it: a
// runtime container hands a value over from its context, and one that holds
// a leaf moved for the request is moved once more.
constexpr cost handed_over(cost before, effect made, bool runtime) {
  if (runtime && made == effect::move && !before.copies) {
    return {before.copies, static_cast<std::uint8_t>(before.moves + 1)};
  }
  return affected(before, made);
}

// The cost of satisfying a request as a container hands it over:
// resolve<T &&>() and construct<T &&>() return the value moved out of what
// the request refers to.
struct handed_cost {
  bool returned;
  bool runtime;
  const context *with;

  constexpr cost operator()(cost made, effect of, const thing &reached) const {
    return handed_over(
        made, returned && makes_leaf_anew(reached, *with) ? effect::move : of,
        runtime);
  }
};

// The fewest copies and moves of the leaf a request resolves with, as
// spec/semantics.py's fewest gives it, and the plan of it; none when it does
// not resolve. construct<T>() of a value it cannot resolve makes T of the
// leaf the container resolves by value: a new handle, optional or array of
// it, the rest of a bounded array value-initialized. The value is moved into
// it once it is resolved.
// The search of the request may be given, made once for several rules.
constexpr answer resolve(const thing &start, const context &with,
                         const request &goal, category taken,
                         const request_rules &rules,
                         const search *searched = nullptr) {
  const entry through =
      entry_of(rules.through, rules.at, rules.selected, taken);
  if (selector_rejects(goal, taken, rules.at, through, rules.selected, with)) {
    return {};
  }
  const bool returned =
      rules.at == position::direct &&
      (through == entry::resolve || through == entry::construct) &&
      taken == category::rvalue_reference;
  const search found = searched ? *searched : reach(start, with, goal);
  answer result =
      satisfy_by(found, goal, taken, required_lifetime(rules.at, through), with,
                 handed_cost{returned, rules.runtime, &with});
  if (result.found || found.overflow) {
    return result;
  }
  bool bounded = false;
  for (std::size_t index = 0; index < goal.shape_depth; ++index) {
    bounded = bounded || with.kind(goal.layers[index]).bounded;
  }
  if (rules.at == position::direct && through == entry::construct &&
      goal.of == element::leaf && goal.shape_depth != 0 &&
      (with.leaf.default_constructible || !bounded)) {
    request leaf;
    answer value = resolve(
        start, with, leaf, category::value,
        {position::direct, entry::resolve, rules.selected, rules.runtime});
    if (value.found) {
      value.fewest.moves = static_cast<std::uint8_t>(value.fewest.moves + 1);
    }
    return value;
  }
  return result;
}
} // namespace dingo::semantics
