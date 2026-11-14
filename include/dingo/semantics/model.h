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

namespace dingo::semantics {

// A layer around an element.
enum class layer : std::uint8_t {
  pointer,
  shared_ptr,
  unique_ptr,
  optional,
  // An alternative type: its element is the alternative it holds.
  variant,
  // A bounded array of two elements, and an array of unknown bound inside a
  // handle: their element is their first element, through the pointer they
  // decay to.
  array2,
  array,
  // The pointer an array decays to, to its first element.
  array_pointer,
  none,
};

inline constexpr std::size_t layer_count =
    static_cast<std::size_t>(layer::none);

// What a layer owns of what it holds.
enum class owner : std::uint8_t { none, sole, shared };

// A layer, by its properties: the rules name properties, never a layer.
struct layer_traits {
  owner owns;
  // Whether a copy of the layer holds the same element.
  bool shares_element;
  bool copyable;
  // Whether a layer of an element can be made from a value of it.
  bool made_from_value;
  // The layer an owned value of this one converts to, or none.
  layer converts_to = layer::none;
  // Whether it is made only of a bare element, as an alternative type is.
  bool holds_element_only = false;
  bool relocatable = true;
  // Whether its element can be taken from it.
  bool gives_element = true;
  // The layer an array decays to, or none.
  layer decays_to = layer::none;
  // Whether it is an array of a known bound.
  bool bounded = false;
  // For a raw pointer, whether a handle takes it over, and the layer the
  // handle holds around its element: none, or an array.
  bool adopted = false;
  layer adopted_with = layer::none;
  // Whether a handle of it takes over a raw pointer the request owns.
  bool adopts = false;
  // Whether a registration of the abstract interface stores a layer of the
  // leaf as one of the interface.
  bool rebinds = false;
  // Whether it may hold no element, and whether an empty one is a value.
  bool may_be_empty = false;
  bool has_empty_value = false;
};

inline constexpr layer_traits layers_traits[layer_count] = {
    // pointer
    {owner::none, true, true, false, layer::none, false, true, true,
     layer::none, false, true, layer::none, false, true, true, false},
    // shared_ptr
    {owner::shared, true, true, true, layer::none, false, true, true,
     layer::none, false, false, layer::none, true, true, true, false},
    // unique_ptr
    {owner::sole, true, false, true, layer::shared_ptr, false, true, true,
     layer::none, false, false, layer::none, true, true, true, false},
    // optional
    {owner::sole, false, true, true, layer::none, false, true, true,
     layer::none, false, false, layer::none, false, false, true, true},
    // variant
    {owner::sole, false, true, true, layer::none, true, true, true, layer::none,
     false, false, layer::none, false, false, true, false},
    // array2
    {owner::sole, false, false, false, layer::none, false, false, false,
     layer::array_pointer, true, false, layer::none, false, false, false,
     false},
    // array
    {owner::sole, false, false, false, layer::none, false, false, false,
     layer::array_pointer, false, false, layer::none, false, false, false,
     false},
    // array_pointer
    {owner::none, true, true, false, layer::none, false, true, false,
     layer::none, false, true, layer::array, false, false, false, false},
};

constexpr const layer_traits &traits_of(layer name) {
  return layers_traits[static_cast<std::size_t>(name)];
}

// The handles that take over a raw pointer, in the order the specification
// declares them.
inline constexpr layer adopters[] = {layer::shared_ptr, layer::unique_ptr};

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

// What a derivation knows of its registration.
struct context {
  leaf_traits leaf;
  scope_traits scope;
  // Whether the leaf is requested as its abstract interface.
  bool abstract;
  // The limitations applied, as a mask of limitation.
  std::uint8_t limits = 0;

  constexpr bool limited(limitation gap) const {
    return (limits & static_cast<std::uint8_t>(gap)) != 0;
  }
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
  layer contained = layer::none;
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

// How many of the layers a copy or a move of a stack makes anew: those down
// to the first that shares its element; all of them and the element when
// none does.
constexpr std::size_t copied_layers(const thing &of) {
  for (std::size_t depth = 1; depth <= of.depth; ++depth) {
    if (traits_of(of.layers[depth - 1]).shares_element) {
      return depth;
    }
  }
  return of.depth + 1;
}

constexpr bool makes_leaf_anew(const thing &of) {
  return copied_layers(of) > of.depth;
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
  const std::size_t depth = copied_layers(of);
  for (std::size_t index = 0; index < depth && index < of.depth; ++index) {
    if (!traits_of(of.layers[index]).copyable) {
      return false;
    }
  }
  return depth <= of.depth || copies_element(of, with);
}

constexpr bool can_move(const thing &of, const context &with) {
  const std::size_t depth = copied_layers(of);
  for (std::size_t index = 0; index < depth && index < of.depth; ++index) {
    if (!traits_of(of.layers[index]).relocatable) {
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
inline constexpr std::size_t max_transitions = 16;

struct transitions_list {
  transition items[max_transitions] = {};
  std::size_t count = 0;

  constexpr void add(step kind, const thing &reached) {
    items[count++] = {kind, reached};
  }
};

// The deepest stack a derivation makes.
inline constexpr std::size_t derivation_depth = 4;

// The thing of a copy: a new object the request owns. What a layer that
// shares its element holds is not copied.
constexpr thing copied(const thing &of) {
  const std::size_t depth = copied_layers(of);
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
constexpr void empty_safe_steps(const thing &of, const thing &inner,
                                const context &with, transitions_list &out) {
  const layer below = inner.depth ? inner.layers[0] : layer::none;
  const bool decays =
      below != layer::none && traits_of(below).decays_to != layer::none;
  thing pointer = inner;
  if (decays) {
    pointer.layers[0] = traits_of(below).decays_to;
    pointer.lifetimes[0] = of.lifetimes[0];
  } else {
    for (std::size_t index = inner.depth; index > 0; --index) {
      pointer.layers[index] = inner.layers[index - 1];
    }
    for (std::size_t index = inner.depth + 1; index > 0; --index) {
      pointer.lifetimes[index] = inner.lifetimes[index - 1];
    }
    pointer.layers[0] = layer::pointer;
    pointer.lifetimes[0] = of.lifetimes[0];
    pointer.depth = static_cast<std::uint8_t>(inner.depth + 1);
  }
  pointer.from = origin::borrowed;
  pointer.absent = of.absent;
  out.add(step::element_pointer, pointer);
  if (below != layer::none && traits_of(below).has_empty_value &&
      !inner.unconstructed) {
    if (owned(inner.from)) {
      thing same = inner;
      same.absent = of.absent;
      out.add(step::element_or_empty, same);
    } else if (!inner.unconstructed && can_copy(inner, with)) {
      thing copy = copied(inner);
      copy.absent = of.absent;
      out.add(step::element_or_empty_copy, copy);
    }
  }
}

// The steps the semantics allow from a thing.
constexpr transitions_list transitions(const thing &of, const context &with) {
  transitions_list out;
  if (of.unconstructed) {
    return out;
  }
  if (of.depth) {
    const layer outer = of.layers[0];
    const layer_traits &traits = traits_of(outer);
    if (traits.decays_to != layer::none) {
      const lifetime request = lifetime::request;
      out.add(step::decay, restacked(of, &traits.decays_to, 1, 1, &request));
      if (of.from == origin::made) {
        const layer pointer = layer::pointer;
        thing handed = of;
        for (std::size_t index = of.depth; index > 0; --index) {
          handed.layers[index] = of.layers[index - 1];
        }
        for (std::size_t index = of.depth + 1; index > 0; --index) {
          handed.lifetimes[index] = of.lifetimes[index - 1];
        }
        handed.layers[0] = pointer;
        handed.lifetimes[0] = lifetime::request;
        handed.depth = static_cast<std::uint8_t>(of.depth + 1);
        out.add(step::hand_over, handed);
      }
    } else if (traits.adopted && of.from == origin::made) {
      for (layer handle : adopters) {
        const layer front[] = {handle, traits.adopted_with};
        const std::size_t count = traits.adopted_with == layer::none ? 1 : 2;
        const lifetime lives[] = {lifetime::request, lifetime::request};
        out.add(step::adopt, restacked(of, front, count, 1, lives));
      }
    } else if (traits.gives_element) {
      const bool made = of.lifetimes[1] == lifetime::request;
      thing inner = restacked(of, nullptr, 0, 1, nullptr);
      inner.from = traits.owns == owner::sole ? of.from : origin::borrowed;
      inner.contained = of.contained != layer::none ? of.contained
                        : made                      ? outer
                                                    : layer::none;
      inner.unconstructed =
          with.scope.publishes_early && of.instance && of.depth > 1;
      inner.absent = of.absent || (traits.may_be_empty && !of.full);
      inner.full = static_cast<std::uint8_t>(of.full ? of.full - 1 : 0);
      out.add(step::element, inner);
      if (traits.may_be_empty && !of.full && !traits.adopted) {
        empty_safe_steps(of, inner, with, out);
      }
    }
    if (owned(of.from) && traits.converts_to != layer::none) {
      const lifetime request = lifetime::request;
      out.add(step::convert,
              restacked(of, &traits.converts_to, 1, 1, &request));
    }
  }
  if (of.of == element::leaf && with.abstract) {
    const layer_traits *outer = of.depth ? &traits_of(of.layers[0]) : nullptr;
    thing upcast = of;
    upcast.of = element::interface;
    if (outer == nullptr ||
        (of.depth == 1 && outer->shares_element && outer->copyable)) {
      out.add(step::upcast, upcast);
    } else if (of.depth == 1 && outer->shares_element && owned(of.from)) {
      upcast.lifetimes[0] = lifetime::request;
      out.add(step::upcast, upcast);
    } else if (of.depth > 1) {
      const std::size_t depth = copied_layers(of);
      bool copyable = true;
      for (std::size_t index = 0; index < of.depth; ++index) {
        copyable = copyable && traits_of(of.layers[index]).copyable;
      }
      if (depth == of.depth && copyable) {
        upcast.from = origin::copied;
        upcast.instance = false;
        for (std::size_t index = 0; index < depth; ++index) {
          upcast.lifetimes[index] = lifetime::request;
        }
        out.add(step::upcast, upcast);
      }
    }
  }
  if (can_copy(of, with)) {
    out.add(step::copy, copied(of));
  }
  const bool shares = of.from == origin::copied && of.instance;
  if ((of.from == origin::made || shares) && can_move(of, with) &&
      of.depth < derivation_depth) {
    const std::size_t depth = copied_layers(of);
    lifetime lives[max_layers + 1] = {};
    std::size_t count = 0;
    if (shares && of.lifetimes[depth] == lifetime::stable) {
      for (; count < depth + 1; ++count) {
        lives[count] = lifetime::stable;
      }
      for (std::size_t index = depth; index <= of.depth; ++index) {
        lives[count++] = of.lifetimes[index];
      }
    } else {
      lives[count++] = lifetime::request;
      for (std::size_t index = 0; index <= of.depth; ++index) {
        lives[count++] = of.lifetimes[index];
      }
    }
    for (std::size_t index = 0; index < layer_count; ++index) {
      const layer made = static_cast<layer>(index);
      const layer_traits &traits = traits_of(made);
      if (!traits.made_from_value || (traits.holds_element_only && of.depth) ||
          (slices(of) && !traits.shares_element)) {
        continue;
      }
      thing wrapped = of;
      for (std::size_t layer_index = of.depth; layer_index > 0; --layer_index) {
        wrapped.layers[layer_index] = of.layers[layer_index - 1];
      }
      wrapped.layers[0] = made;
      wrapped.depth = static_cast<std::uint8_t>(of.depth + 1);
      for (std::size_t life = 0; life < count && life <= max_layers; ++life) {
        wrapped.lifetimes[life] = lives[life];
      }
      wrapped.full = static_cast<std::uint8_t>(of.full + 1);
      out.add(step::wrap, wrapped);
    }
  }
  return out;
}

// The steps from a thing the library takes: those the semantics allow that no
// limitation blocks. A pointer into a copy a binding converts is not taken.
constexpr transitions_list steps(const thing &of, const context &with) {
  transitions_list allowed = transitions(of, with);
  if (!with.limited(limitation::pointer_into_converted_copy) ||
      of.from != origin::copied) {
    return allowed;
  }
  transitions_list out;
  for (std::size_t index = 0; index < allowed.count; ++index) {
    if (allowed.items[index].kind != step::element_pointer) {
      out.add(allowed.items[index].kind, allowed.items[index].reached);
    }
  }
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

// Whether a copy or a move of the layers of a thing from one of them on
// makes the leaf anew: none of those layers shares its element.
constexpr bool makes_leaf_anew(const thing &of, std::size_t from) {
  for (std::size_t index = from; index < of.depth; ++index) {
    if (traits_of(of.layers[index]).shares_element) {
      return false;
    }
  }
  return true;
}

constexpr effect effect_of(step kind, const thing &of) {
  if (kind == step::element_or_empty_copy) {
    return makes_leaf_anew(of, 1) ? effect::copy : effect::none;
  }
  if ((kind == step::copy || kind == step::wrap) && makes_leaf_anew(of, 0)) {
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

// What a request asks for: the layers around its element.
struct request {
  layer layers[max_layers] = {};
  std::uint8_t depth = 0;
  semantics::element of = semantics::element::leaf;
};

// Whether the layers of a thing may still become the layers of a request: a
// layer is wrapped around a thing only on the way to the request, as its
// layers are, or as a layer that converts to its outermost one. The search
// it directs reaches what satisfies a request at the same cost as the search
// of every thing; the test of the semantics checks it for every request.
constexpr bool may_become(const thing &of, const request &goal) {
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
         traits_of(of.layers[0]).converts_to == goal.layers[offset];
}

// A thing a derivation reaches, with the copies and the moves of the leaf of
// one derivation that reaches it.
struct state {
  thing reached;
  cost made;
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

constexpr search reach(const thing &start, const context &with,
                       const request &goal) {
  search found;
  found.add({start, {}});
  for (std::size_t next = 0; next < found.count; ++next) {
    const state from = found.states[next];
    const transitions_list taken = steps(from.reached, with);
    for (std::size_t index = 0; index < taken.count; ++index) {
      const transition &each = taken.items[index];
      if (each.kind == step::wrap && !may_become(each.reached, goal)) {
        continue;
      }
      found.add({each.reached,
                 affected(from.made, effect_of(each.kind, from.reached))});
    }
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
  if (of.depth - from != goal.depth) {
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
  if (!of.depth || !traits_of(of.layers[0]).adopted ||
      !has_layers(of, goal, 1) || !of.instance) {
    return false;
  }
  if (of.from == origin::made &&
      !(with.limited(limitation::pointer_out_of_made_holder) &&
        of.contained != layer::none)) {
    return true;
  }
  return static_cast<int>(of.lifetimes[1]) >= static_cast<int>(needed);
}

// The fewest copies and moves of the leaf of what satisfies a request, as the
// specification's satisfactions give it, or none.
struct answer {
  bool found = false;
  cost fewest;
  bool overflow = false;
};

constexpr void consider(answer &result, cost made) {
  if (!result.found || made < result.fewest) {
    result.found = true;
    result.fewest = made;
  }
}

constexpr answer satisfy(const search &found, const request &goal,
                         category taken, lifetime needed, const context &with) {
  answer result;
  result.overflow = found.overflow;
  if (refers(taken)) {
    for (std::size_t index = 0; index < found.count; ++index) {
      const state &each = found.states[index];
      const thing &of = each.reached;
      if (of.of == goal.of && has_layers(of, goal) && of.instance &&
          static_cast<int>(of.life()) >= static_cast<int>(needed)) {
        consider(result, each.made);
      }
    }
    if (result.found || !points(taken)) {
      return result;
    }
    for (std::size_t index = 0; index < found.count; ++index) {
      const state &each = found.states[index];
      if (each.reached.of == goal.of &&
          points_to(each.reached, goal, needed, with)) {
        consider(result, each.made);
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
        consider(result, each.made);
      }
      continue;
    }
    if (slices(of) || of.unconstructed) {
      continue;
    }
    const bool anew = makes_leaf_anew(of, 0);
    if (owned(of.from) && can_move(of, with)) {
      consider(result, affected(each.made, anew ? effect::move : effect::none));
    }
    if (can_copy(of, with)) {
      consider(result, affected(each.made, anew ? effect::copy : effect::none));
    }
  }
  return result;
}

constexpr answer fewest(const thing &start, const context &with,
                        const request &goal, category taken, lifetime needed) {
  return satisfy(reach(start, with, goal), goal, taken, needed, with);
}
} // namespace dingo::semantics
