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
  // Whether a registration bound only to the abstract interface stores a
  // layer of the leaf as one of the interface: a binding wraps what holds the
  // leaf into it, to give the interface.
  bool rebinds = false;
  // Whether it is made of a raw pointer to its element, which it does not
  // own, as an observer handle is.
  bool observes = false;
  // Whether a value of it converts to one of a more qualified element, as a
  // pointer to the element converts: a raw pointer, and a handle whose
  // converting constructor does.
  bool qualifies = false;
  // The layers a value of it that holds an array of a known bound converts
  // to, holding the same array as one of unknown bound, as a mask of their
  // indices.
  std::uint32_t to_unknown_bound = 0;
  // Whether a handle of it takes over a raw pointer to an array of a known
  // bound, which it deletes as an array. It is last, as the layers of C++
  // itself list the properties before it.
  bool adopts_bounded = false;
};

// Whether a type can be a layer of these properties, as spec/semantics.py
// says of a layer. A layer that owns nothing, or shares what it owns, holds
// an element that lives apart from it: a copy of it holds the same one. A
// layer that owns its element alone and shares it with a copy cannot be
// copied: the copy would share what one of them owns. A layer that owns
// nothing is not made of a value and takes over no pointer, and only one that
// takes over a pointer takes over one to an array of a known bound. A layer
// that takes over a pointer holds the element apart from it, and so do a
// layer the leaf is rebound to the interface in and a layer that converts to
// one of a more qualified element or of an array of unknown bound. An empty
// value is a value only of a layer that may be empty.
constexpr bool possible(const layer_kind &kind) {
  return (kind.owns == owner::sole || kind.shares_element) &&
         !(kind.owns == owner::sole && kind.shares_element && kind.copyable) &&
         !(kind.owns == owner::none && (kind.made_from_value || kind.adopts)) &&
         (!kind.adopts || kind.shares_element) &&
         (!kind.adopts_bounded || kind.adopts) &&
         (!kind.rebinds || kind.shares_element) &&
         (!kind.qualifies || kind.shares_element) &&
         (!kind.to_unknown_bound || kind.shares_element) &&
         (!kind.observes || (kind.owns == owner::none && kind.may_be_empty)) &&
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
       false, false, true, false, 0, true, false, true},
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
  // Whether a volatile one can be copied, as a scalar can.
  bool copies_volatile = false;
};

// The qualifiers of an object, as a mask.
inline constexpr std::uint8_t const_qual = 1;
inline constexpr std::uint8_t volatile_qual = 2;

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
enum class element : std::uint8_t { leaf, abstract };

// The known gaps of the library, which the expectations apply on top of the
// rules.
enum class limitation : std::uint8_t {
  pointer_out_of_made_holder = 1 << 0,
  pointer_into_converted_copy = 1 << 1,
  qualified_below_element = 1 << 2,
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
  // The qualifiers of each layer and of the element, outermost first: of the
  // outermost as it is reached, of the others as the type of the layer above
  // declares them.
  std::uint8_t quals[max_layers + 1] = {};
  // Whether the request owns what its outer raw pointer points to, which it
  // deletes or hands over.
  bool owns_pointee = false;
  // Whether the derivation made its outer layer of a value or of a pointer,
  // which it never converts.
  bool made_layer = false;
  // Whether its outer layer is converted to one of a more qualified element,
  // of all the qualifiers it is converted to at once.
  bool qualified = false;

  constexpr lifetime life() const { return lifetimes[0]; }
};

constexpr bool operator==(const thing &a, const thing &b) {
  if (a.depth != b.depth || a.of != b.of || a.from != b.from ||
      a.instance != b.instance || a.contained != b.contained ||
      a.unconstructed != b.unconstructed || a.absent != b.absent ||
      a.full != b.full || a.owns_pointee != b.owns_pointee ||
      a.made_layer != b.made_layer || a.qualified != b.qualified) {
    return false;
  }
  for (std::size_t index = 0; index < a.depth; ++index) {
    if (a.layers[index] != b.layers[index]) {
      return false;
    }
  }
  for (std::size_t index = 0; index <= a.depth; ++index) {
    if (a.lifetimes[index] != b.lifetimes[index] ||
        a.quals[index] != b.quals[index]) {
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
    made.quals[depth] = 0;
    made.layers[depth++] = front[index];
  }
  for (std::size_t index = below; index <= from.depth; ++index) {
    made.lifetimes[depth] = from.lifetimes[index];
    made.quals[depth] = from.quals[index];
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
    made.quals[index] = 0;
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
  return of.of == element::abstract && of.depth == 0;
}

// The qualifiers of the element of the outer layer of a thing: as the layer
// declares them, and those of the layer too when it holds the element in
// place.
constexpr std::uint8_t held_quals(const thing &of, const layer_kind &kind) {
  return static_cast<std::uint8_t>(of.quals[1] |
                                   (kind.shares_element ? 0 : of.quals[0]));
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
  // What a copy makes anew is not volatile, but a leaf that copies a
  // volatile one.
  for (std::size_t index = 0; index < depth && index <= of.depth; ++index) {
    if ((of.quals[index] & volatile_qual) &&
        !(index == of.depth && with.leaf.copies_volatile)) {
      return false;
    }
  }
  return depth <= of.depth || copies_element(of, with);
}

// A const or a volatile object is not moved from.
constexpr bool can_move(const thing &of, const context &with) {
  const std::size_t depth = copied_layers(of, with);
  for (std::size_t index = 0; index < depth && index < of.depth; ++index) {
    if (!with.kind(of.layers[index]).relocatable) {
      return false;
    }
  }
  for (std::size_t index = 0; index < depth && index <= of.depth; ++index) {
    if (of.quals[index]) {
      return false;
    }
  }
  return depth <= of.depth || with.leaf.movable;
}

// A kind of step, as the specification names it.
enum class step : std::uint8_t {
  // What a pointer a binding made points to, released by the binding.
  release,
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
  // A handle that does not own its element, made of a raw pointer to it.
  observe,
  // A pointer or a handle converted to one of a more qualified element.
  qualify,
  // A pointer or a handle of an array of a known bound converted to one of
  // an array of unknown bound.
  unknown_bound,
};

struct transition {
  step kind;
  thing reached;
};

// The most steps from one thing.
inline constexpr std::size_t max_transitions = 5 * max_kinds + 16;

struct transitions_list {
  transition items[max_transitions] = {};
  std::size_t count = 0;

  constexpr void add(step kind, const thing &reached) {
    items[count++] = {kind, reached};
  }

  // It takes a step of every kind, of every outer layer and qualifier.
  constexpr bool takes(step, layer, std::size_t) const { return true; }
  constexpr bool takes_quals(std::size_t, std::uint8_t, std::size_t) const {
    return true;
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
  made.quals[0] = 0;
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
    pointer.quals[1] = held_quals(inner, with.kind(below));
    pointer.quals[0] = 0;
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

// Whether the outer layer of a thing is made as the layer a request needs, of
// the qualifiers of its element too: one the request made of a value or of a
// pointer. A handle of the interface is one the binding made of the leaf and
// upcast, which a request converts.
constexpr bool made_at_once(const thing &of) {
  return of.made_layer && !(of.of == element::abstract && of.depth == 1);
}

// The things a pointer or a handle converts to of a more qualified element,
// as a pointer to the element converts, from the level of the thing on: each
// level of qualifiers that may gain more, the element always, a level below
// it only where every level above it is a raw pointer made const.
template <typename Sink>
constexpr void qualify_levels(const thing &of, const context &with, thing &made,
                              std::size_t level, bool may_gain, bool gained,
                              Sink &out) {
  if (level > of.depth || level > max_layers) {
    if (gained) {
      out(step::qualify, made);
    }
    return;
  }
  const std::uint8_t got = of.quals[level];
  for (std::uint8_t wanted = 0; wanted < 4; ++wanted) {
    if ((got & ~wanted) || (wanted != got && !may_gain) ||
        !out.takes_quals(level, wanted, of.depth)) {
      continue;
    }
    const bool below =
        level < of.depth && with.kind(of.layers[level]).adopted &&
        !with.kind(of.layers[level]).adopted_array && (wanted & const_qual);
    made.quals[level] = wanted;
    qualify_levels(of, with, made, level + 1, may_gain && below,
                   gained || wanted != got, out);
  }
  made.quals[level] = got;
}

// A pointer or a handle converted to one of a more qualified element: a new
// object, which holds the same element, moved out of one the request owns or
// copied of another. A pointer the request owns is handed over or taken over
// instead, never converted.
template <typename Sink>
constexpr void qualify_steps(const thing &of, const context &with, Sink &out) {
  if (!of.depth || !with.kind(of.layers[0]).qualifies || of.qualified ||
      of.owns_pointee || !out.takes(step::qualify, of.layers[0], of.depth)) {
    return;
  }
  const bool moved = owned(of.from) && can_move(of, with);
  if (!moved && !can_copy(of, with)) {
    return;
  }
  thing made = of;
  // A layer made at once is the same object, of the qualifiers it needs.
  if (!made_at_once(of)) {
    made.lifetimes[0] = lifetime::request;
  }
  made.from = moved ? of.from : origin::copied;
  made.quals[0] = 0;
  made.qualified = true;
  qualify_levels(of, with, made, 1, true, false, out);
}

// A pointer or a handle of an array of a known bound converted to one of an
// array of unknown bound, which holds the same array: a new object, moved out
// of one the request owns or copied of another. One converted to a more
// qualified element is not converted again, and a pointer the request owns is
// handed over or taken over instead.
template <typename Sink>
constexpr void unknown_bound_steps(const thing &of, const context &with,
                                   Sink &out) {
  if (of.depth < 2 || !with.kind(of.layers[1]).bounded || of.qualified ||
      of.owns_pointee) {
    return;
  }
  const std::uint32_t targets = with.kind(of.layers[0]).to_unknown_bound;
  if (!targets) {
    return;
  }
  const bool moved = owned(of.from) && can_move(of, with);
  if (!moved && !can_copy(of, with)) {
    return;
  }
  for (std::size_t index = 0; index < with.layers->count; ++index) {
    if ((targets & (std::uint32_t{1} << index)) &&
        out.takes(step::unknown_bound, static_cast<layer>(index), of.depth)) {
      thing made = of;
      made.layers[0] = static_cast<layer>(index);
      made.layers[1] = array_layer;
      made.lifetimes[0] = lifetime::request;
      made.from = moved ? of.from : origin::copied;
      made.quals[0] = 0;
      made.made_layer = false;
      out(step::unknown_bound, made);
    }
  }
}

// What a sink of the steps from a thing passes on: a step that makes another
// outer layer makes one not converted to a more qualified element.
template <typename Sink> struct unqualified_sink {
  Sink &out;
  // It takes the steps what it passes them to takes.
  constexpr bool takes(step kind, layer outer, std::size_t depth) const {
    return out.takes(kind, outer, depth);
  }
  constexpr bool takes_quals(std::size_t level, std::uint8_t quals,
                             std::size_t depth) const {
    return out.takes_quals(level, quals, depth);
  }

  constexpr void operator()(step kind, const thing &reached) {
    if (reached.qualified && kind != step::qualify) {
      thing made = reached;
      made.qualified = false;
      out(kind, made);
    } else {
      out(kind, reached);
    }
  }
};

template <typename Sink>
constexpr void each_layer_transition(const thing &of, const context &with,
                                     Sink &out);

// The steps the semantics allow from a thing.
// They are passed to a sink, out(kind, thing), one by one: a constant
// expression returns no list of them, which a compiler would keep.
template <typename Sink>
constexpr void each_transition(const thing &of, const context &with,
                               Sink &out) {
  unqualified_sink<Sink> reset{out};
  each_layer_transition(of, with, reset);
}

template <typename Sink>
constexpr void each_layer_transition(const thing &of, const context &with,
                                     Sink &out) {
  if (of.unconstructed) {
    return;
  }
  const lifetime request = lifetime::request;
  if (of.depth) {
    const layer outer = of.layers[0];
    const layer_kind &kind = with.kind(outer);
    if (kind.adopted && !kind.adopted_array && !of.owns_pointee) {
      // A handle that does not own its element, made of a raw pointer to it
      // that owns nothing either: a new object, which points to what the
      // pointer does.
      for (std::size_t index = 0; index < with.layers->count; ++index) {
        if (with.layers->kinds[index].observes &&
            out.takes(step::observe, static_cast<layer>(index), of.depth)) {
          const layer to = static_cast<layer>(index);
          thing observer = restacked(of, &to, 1, 1, &request);
          observer.from = origin::made;
          observer.made_layer = true;
          out(step::observe, observer);
        }
      }
    }
    if (kind.decays) {
      if (out.takes(step::decay, array_pointer_layer, of.depth)) {
        thing decayed = restacked(of, &array_pointer_layer, 1, 1, &request);
        decayed.quals[1] = held_quals(of, kind);
        decayed.owns_pointee = of.from == origin::made;
        decayed.made_layer = false;
        out(step::decay, decayed);
      }
      if (of.from == origin::made &&
          out.takes(step::hand_over, pointer_layer, of.depth + 1u)) {
        thing handed = wrapped(of, pointer_layer, request);
        handed.owns_pointee = true;
        handed.made_layer = false;
        out(step::hand_over, handed);
      }
    } else if (kind.adopted && of.owns_pointee) {
      for (std::size_t index = 0; index < with.layers->count; ++index) {
        // A handle that does not delete an array of a known bound as an
        // array takes over no pointer to one.
        const bool bounded = !kind.adopted_array && of.depth > 1 &&
                             with.kind(of.layers[1]).bounded;
        if (!with.layers->kinds[index].adopts ||
            (bounded && !with.layers->kinds[index].adopts_bounded) ||
            !out.takes(step::adopt, static_cast<layer>(index),
                       of.depth + (kind.adopted_array ? 1u : 0u))) {
          continue;
        }
        const layer front[] = {static_cast<layer>(index), array_layer};
        const lifetime lives[] = {request, request};
        thing adopted =
            restacked(of, front, kind.adopted_array ? 2 : 1, 1, lives);
        adopted.from = origin::made;
        adopted.owns_pointee = false;
        adopted.made_layer = true;
        out(step::adopt, adopted);
      }
    } else if (kind.gives_element && !of.full && !of.qualified &&
               out.takes(step::element, outer, of.depth)) {
      // A layer the derivation wrapped a value in is not taken apart: its
      // element is what it was made of, as it was; nor is one it converted
      // to a more qualified element: its element is that of what it was
      // converted of.
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
      inner.quals[0] = held_quals(of, kind);
      inner.owns_pointee = false;
      inner.made_layer = false;
      out(step::element, inner);
      if (kind.may_be_empty && !of.full && !kind.adopted) {
        empty_safe_steps(of, inner, with, out);
      }
    }
    // Not a layer the derivation made, which it makes as the other at once,
    // nor one it converted to a more qualified element.
    if (owned(of.from) && !of.quals[0] && !of.made_layer && !of.qualified) {
      for (std::size_t index = 0; index < with.layers->count; ++index) {
        if ((kind.converts & (std::uint32_t{1} << index)) &&
            out.takes(step::convert, static_cast<layer>(index), of.depth)) {
          const layer to = static_cast<layer>(index);
          out(step::convert, restacked(of, &to, 1, 1, &request));
        }
      }
    }
  }
  if (of.of == element::leaf && with.abstract &&
      out.takes(step::upcast, no_layer, of.depth)) {
    const layer_kind *outer = of.depth ? &with.kind(of.layers[0]) : nullptr;
    thing upcast = of;
    upcast.of = element::abstract;
    if (outer != nullptr && !outer->gives_element) {
      // The pointer to the first element of an array is a pointer to that
      // element as the interface, not to an array of the interface: the
      // request does not own what it points to.
      if (of.depth == 1 && outer->shares_element) {
        upcast.layers[0] = pointer_layer;
        upcast.from = origin::borrowed;
        upcast.owns_pointee = false;
        out(step::upcast, upcast);
      }
    } else if (outer == nullptr) {
      // The leaf as the base it is: the same object.
      out(step::upcast, upcast);
    } else if (of.depth == 1 && outer->shares_element && outer->copyable &&
               !(of.quals[0] & volatile_qual)) {
      // A new pointer or handle of the interface.
      upcast.quals[0] = 0;
      out(step::upcast, upcast);
    } else if (of.depth == 1 && outer->shares_element && owned(of.from) &&
               !of.quals[0]) {
      upcast.lifetimes[0] = request;
      out(step::upcast, upcast);
    } else if (of.depth > 1) {
      const std::size_t depth = copied_layers(of, with);
      bool copyable = true;
      for (std::size_t index = 0; index < of.depth; ++index) {
        copyable = copyable && with.kind(of.layers[index]).copyable;
      }
      for (std::size_t index = 0; index < depth; ++index) {
        copyable = copyable && !(of.quals[index] & volatile_qual);
      }
      if (depth == of.depth && copyable) {
        upcast.from = origin::copied;
        upcast.instance = false;
        upcast.quals[0] = 0;
        for (std::size_t index = 0; index < depth; ++index) {
          upcast.lifetimes[index] = request;
        }
        out(step::upcast, upcast);
      }
    }
  }
  // A conversion to a more qualified element is not copied: what it converts
  // is.
  if (!of.qualified &&
      out.takes(step::copy, of.depth ? of.layers[0] : no_layer, of.depth) &&
      can_copy(of, with)) {
    out(step::copy, copied(of, with));
  }
  qualify_steps(of, with, out);
  unknown_bound_steps(of, with, out);
  const bool shares = of.from == origin::copied && of.instance;
  // A raw pointer that owns what it points to is not wrapped: it is handed
  // over, or taken over by a handle.
  if ((of.from == origin::made || shares) && can_move(of, with) &&
      of.depth < derivation_depth && !of.owns_pointee) {
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
          (slices(of) && !kind.shares_element) ||
          !out.takes(step::wrap, static_cast<layer>(index), of.depth + 1u)) {
        continue;
      }
      thing made = wrapped(inner, static_cast<layer>(index), outer);
      made.full = static_cast<std::uint8_t>(of.full + 1);
      made.owns_pointee = false;
      made.made_layer = true;
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
// limitation blocks. A pointer into a copy a binding converts is not taken,
// nor is a pointer or a handle converted to one more qualified below its
// element.
// What a sink of the steps the library takes passes on: a step the
// limitations do not block.
template <typename Sink> struct unblocked {
  const thing &from;
  const context &with;
  Sink &out;

  // It takes the steps what it passes them to takes.
  constexpr bool takes(step kind, layer outer, std::size_t depth) const {
    return out.takes(kind, outer, depth);
  }
  constexpr bool takes_quals(std::size_t level, std::uint8_t quals,
                             std::size_t depth) const {
    return out.takes_quals(level, quals, depth);
  }

  constexpr void operator()(step kind, const thing &reached) {
    if (kind == step::element_pointer && from.from == origin::copied &&
        with.limited(limitation::pointer_into_converted_copy)) {
      return;
    }
    if (kind == step::qualify &&
        with.limited(limitation::qualified_below_element)) {
      for (std::size_t level = 2; level <= reached.depth; ++level) {
        if (reached.quals[level] != from.quals[level]) {
          return;
        }
      }
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
// The cost of a derivation: the copies and the moves of the leaf, the
// objects it keeps as long as the container, and the objects it makes for
// the request, compared in this order.
struct cost {
  std::uint8_t copies = 0;
  std::uint8_t moves = 0;
  std::uint8_t kept = 0;
  std::uint8_t made = 0;
};

constexpr bool operator==(cost a, cost b) {
  return a.copies == b.copies && a.moves == b.moves && a.kept == b.kept &&
         a.made == b.made;
}

// Whether two costs are of the same copies and moves of the leaf.
constexpr bool same_leaf(cost a, cost b) {
  return a.copies == b.copies && a.moves == b.moves;
}

constexpr bool operator<(cost a, cost b) {
  if (a.copies != b.copies) {
    return a.copies < b.copies;
  }
  if (a.moves != b.moves) {
    return a.moves < b.moves;
  }
  return a.kept != b.kept ? a.kept < b.kept : a.made < b.made;
}

// Whether a cost of a thing is no better than another of it: of the same
// copies and moves of the leaf, and of no fewer objects. A derivation from
// it costs no less, so a search that drops it ends.
constexpr bool dominates(cost a, cost b) {
  return same_leaf(a, b) && a.kept <= b.kept && a.made <= b.made;
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
    before.copies = 1;
  } else if (made == effect::move && !before.copies) {
    before.moves = 1;
  }
  return before;
}

// The cost after a step from a thing to what it reaches: of the leaf, and of
// the object it makes, kept when it lives as long as the container. Every
// step but the element, the upcast of the leaf itself, and the qualifiers of
// a layer made at once, makes one.
constexpr cost stepped(cost before, step kind, const thing &of,
                       const thing &reached, const context &with) {
  cost made = affected(before, effect_of(kind, of, with));
  const bool makes = kind != step::element && kind != step::element_or_empty &&
                     !(kind == step::upcast && of.depth == 0) &&
                     !(kind == step::qualify && made_at_once(of));
  if (makes) {
    if (reached.lifetimes[0] == lifetime::stable) {
      ++made.kept;
    } else {
      ++made.made;
    }
  }
  return made;
}

// What a request asks for: the layers around its element, of which the
// outer shape_depth are of the shape of the request and the rest of the
// interface it is of.
struct request {
  layer layers[max_layers] = {};
  std::uint8_t depth = 0;
  semantics::element of = semantics::element::leaf;
  std::uint8_t shape_depth = 0;
  // The qualifiers of the object the request is of and of each layer below,
  // outermost first.
  std::uint8_t quals[max_layers + 1] = {};
};

// Whether the layers of a thing may still become the layers of a request:
// those below its outermost are those of the request below the same depth,
// and its outermost is the request's, or converts to it. A wrap or an
// observer is made only as the layer of the request at its depth: neither
// is converted, and taking its element gives what it was made of, so any
// other leads nowhere.
// The search it directs reaches what satisfies a request at the same cost as
// the search of every thing; the test of the semantics checks it for every
// request.
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

// Whether a request may still reach its goal from a thing by its steps. Each
// adds an outer layer or converts the outermost, and the conversion to an
// array of unknown bound converts the one below it too, so a thing is of no
// use when it has more layers than the request, but for a raw pointer to
// what the request is, which a request of a pointer takes, or an array that
// decays to one; or when a layer below its outermost two is not the
// request's at the same depth.
constexpr bool may_satisfy(const thing &of, const request &goal,
                           const context &with) {
  if (of.depth > goal.depth + 1 ||
      (of.depth == goal.depth + 1 && !with.kind(of.layers[0]).adopted &&
       !with.kind(of.layers[0]).decays)) {
    return false;
  }
  // The layer of the request below a layer of the thing at an index past the
  // outermost: the thing has at most one layer more.
  const std::size_t offset = goal.depth + 1 - of.depth;
  for (std::size_t index = 1; index < of.depth; ++index) {
    const layer wanted = goal.layers[offset + index - 1];
    if (of.layers[index] != wanted &&
        !(index == 1 && with.kind(of.layers[1]).bounded &&
          wanted == array_layer)) {
      return false;
    }
  }
  return true;
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

// The states a search holds. The code compiles a search of the layer sources
// of every binding and of every request a static container plans, which a
// compiler may evaluate element by element: the searches of the semantics
// domains reach at most 83 states. One the code compiles that reaches more
// does not compile; a runtime resolution that reaches more fails.
inline constexpr std::size_t max_states = 256;

// The states a stored object reaches on the way to a request.
// A hash of a thing and the copies and the moves of its leaf, which a state
// is found by.
constexpr std::uint32_t state_hash(const state &of) {
  const thing &t = of.reached;
  std::uint32_t hash = 2166136261u;
  auto mix = [&hash](unsigned value) { hash = (hash ^ value) * 16777619u; };
  mix(t.depth);
  for (std::size_t index = 0; index < t.depth; ++index) {
    mix(t.layers[index]);
  }
  for (std::size_t index = 0; index <= t.depth; ++index) {
    mix(static_cast<unsigned>(t.lifetimes[index]));
    mix(t.quals[index]);
  }
  mix(static_cast<unsigned>(t.of));
  mix(static_cast<unsigned>(t.from));
  mix(t.instance);
  mix(t.contained);
  mix(t.unconstructed);
  mix(t.absent);
  mix(t.full);
  mix(t.owns_pointee);
  mix(t.made_layer);
  mix(t.qualified);
  mix(of.made.copies);
  mix(of.made.moves);
  return hash;
}

inline constexpr std::size_t search_slots = 2 * max_states;

struct search {
  state states[max_states] = {};
  std::size_t count = 0;
  // Whether there were more states than the search holds: its answer is not
  // to be trusted.
  bool overflow = false;
  // How many of the first states are the closed layer sources of a binding
  // the search reached from, if any.
  std::size_t closed = 0;
  // The states by their hash, open addressed: an index, or none.
  std::uint16_t slots[search_slots] = {};

  // Holds no state again. A state past the count is never read, so only the
  // count and the slots are reset.
  constexpr void clear() {
    count = 0;
    overflow = false;
    closed = 0;
    for (std::uint16_t &slot : slots) {
      slot = 0;
    }
  }

  // Adds a state, unless one of its thing and of no greater cost is found
  // already.
  constexpr bool add(const state &found) {
    std::size_t slot = state_hash(found) % search_slots;
    while (slots[slot] != 0) {
      const state &other = states[slots[slot] - 1];
      if (dominates(other.made, found.made) && other.reached == found.reached) {
        return false;
      }
      slot = (slot + 1) % search_slots;
    }
    if (count == max_states) {
      overflow = true;
      return false;
    }
    states[count++] = found;
    slots[slot] = static_cast<std::uint16_t>(count);
    return true;
  }
};

// Whether a thing converted to a more qualified element may still become the
// request: its layers may, or a raw pointer may become the observer of the
// request at its depth, which takes the qualifiers of what the pointer points
// to; and below its outermost its qualifiers are those of the request at the
// same depth, which no later step changes.
constexpr bool quals_satisfy(const thing &of, const request &goal,
                             const context &with) {
  if (of.depth == 0 || of.depth > goal.depth) {
    return false;
  }
  const std::size_t offset = goal.depth - of.depth;
  const layer_kind &outer = with.kind(of.layers[0]);
  const bool observed = outer.adopted && !outer.adopted_array &&
                        with.kind(goal.layers[offset]).observes;
  if (!may_become(of, goal, with) && !observed) {
    return false;
  }
  for (std::size_t index = 1; index < of.depth; ++index) {
    if (of.layers[index] != goal.layers[offset + index]) {
      return false;
    }
  }
  for (std::size_t level = 1; level <= of.depth; ++level) {
    if (of.quals[level] != goal.quals[offset + level]) {
      return false;
    }
  }
  return true;
}

// What reach passes a step to: a wrap or an observer only as the layer of
// the request at its depth, and a pointer or a handle converted to a more
// qualified element only as the qualifiers of the request at its depth. An
// adopted handle may give its element.
struct reached_sink {
  search &found;
  const state &from;
  std::uint16_t index;
  const request &goal;
  const context &with;

  // A wrap or an observer is taken only as the layer of the request at its
  // depth, which it is asked before the step is made.
  constexpr bool takes(step kind, layer outer, std::size_t depth) const {
    if (kind != step::wrap && kind != step::observe) {
      return true;
    }
    return depth <= goal.depth && outer == goal.layers[goal.depth - depth];
  }

  // A conversion to a more qualified element is taken only to the
  // qualifiers of the request at each level.
  constexpr bool takes_quals(std::size_t level, std::uint8_t quals,
                             std::size_t depth) const {
    return depth <= goal.depth &&
           quals == goal.quals[goal.depth - depth + level];
  }

  constexpr void operator()(step kind, const thing &reached) {
    if ((kind == step::wrap || kind == step::observe) &&
        !(may_become(reached, goal, with) &&
          reached.layers[0] == goal.layers[goal.depth - reached.depth])) {
      return;
    }
    if (kind == step::qualify && !quals_satisfy(reached, goal, with)) {
      return;
    }
    found.add({reached, stepped(from.made, kind, from.reached, reached, with),
               index, kind});
  }
};

// What a closure passes a step to: every step, to the search.
template <typename Search> struct any_sink {
  Search &found;
  const state &from;
  std::uint16_t index;
  const context &with;

  // It takes a step of every kind, of every outer layer and qualifier.
  constexpr bool takes(step, layer, std::size_t) const { return true; }
  constexpr bool takes_quals(std::size_t, std::uint8_t, std::size_t) const {
    return true;
  }

  constexpr void operator()(step kind, const thing &reached) {
    found.add({reached, stepped(from.made, kind, from.reached, reached, with),
               index, kind});
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

// Whether a thing is of the shape of a request: its layers, and below the
// object it is the qualifiers of each.
constexpr bool has_shape(const thing &of, const request &goal,
                         std::size_t from = 0) {
  if (!has_layers(of, goal, from)) {
    return false;
  }
  for (std::size_t index = 1; index <= goal.depth; ++index) {
    if (of.quals[from + index] != goal.quals[index]) {
      return false;
    }
  }
  return true;
}

// The qualifiers a reference or a pointer of a request may refer to: those
// of the object it is of, with const for a category of a const one.
constexpr std::uint8_t referable_quals(const request &goal, category taken) {
  const bool constant = taken == category::const_lvalue_reference ||
                        taken == category::const_pointer;
  return static_cast<std::uint8_t>(goal.quals[0] | (constant ? const_qual : 0));
}

// Whether a pointer to the object of a thing at a level, of the layers below,
// converts to a pointer of a request, as C++ converts one: the object may be
// of more qualifiers, of the mask referable, and a level below it may be of
// more when every level above it, from the object down, is a raw pointer the
// request makes const.
constexpr bool converts_quals(const thing &of, std::size_t from,
                              const request &goal, std::uint8_t referable,
                              const context &with) {
  if (of.quals[from] & ~referable) {
    return false;
  }
  bool through = true;
  for (std::size_t level = 1; from + level <= of.depth; ++level) {
    const std::uint8_t above = level == 1 ? referable : goal.quals[level - 1];
    const layer_kind &kind = with.kind(of.layers[from + level - 1]);
    through =
        through && kind.adopted && !kind.adopted_array && (above & const_qual);
    const std::uint8_t got = of.quals[from + level];
    const std::uint8_t wanted = goal.quals[level];
    if (got != wanted && ((got & ~wanted) || !through)) {
      return false;
    }
  }
  return true;
}

// Whether a thing is a pointer value a pointer request takes: one whose
// pointee lives long enough; or one whose pointee the request owns, which
// hands that over, only where nothing is referred to.
constexpr bool pointer_satisfies(const thing &of, const request &goal,
                                 lifetime needed, const context &with,
                                 std::uint8_t referable, bool referred) {
  if (!of.depth || !with.kind(of.layers[0]).adopted ||
      !has_layers(of, goal, 1) || !of.instance ||
      !converts_quals(of, 1, goal, referable, with)) {
    return false;
  }
  if (of.owns_pointee) {
    return !referred &&
           !(with.limited(limitation::pointer_out_of_made_holder) &&
             of.contained != no_layer);
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

// Whether a cost of what may be absent is cheaper than another: by the
// copies and the moves of the leaf, then by what is defined when a layer is
// empty, then by the objects kept and made.
constexpr bool cheaper(cost made, bool absent, cost other, bool other_absent) {
  if (!same_leaf(made, other)) {
    return made < other;
  }
  if (absent != other_absent) {
    return !absent;
  }
  return made < other;
}

// The answer of what satisfies a request at the least cost, as the
// specification's fewest prefers it.
constexpr void consider(answer &result, cost made, std::size_t index,
                        satisfaction how, bool absent) {
  if (!result.found || cheaper(made, absent, result.fewest, result.absent)) {
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
constexpr answer satisfy_states(const state *states, std::size_t count,
                                bool overflow, const request &goal,
                                category taken, lifetime needed,
                                const context &with, Finish finish) {
  answer result;
  result.overflow = overflow;
  if (refers(taken)) {
    const std::uint8_t referable = referable_quals(goal, taken);
    for (std::size_t index = 0; index < count; ++index) {
      const state &each = states[index];
      const thing &of = each.reached;
      // A reference adds qualifiers only to what it refers to; a pointer
      // converts as C++ converts one.
      const bool qualified =
          points(taken) ? has_layers(of, goal) &&
                              converts_quals(of, 0, goal, referable, with)
                        : has_shape(of, goal) && !(of.quals[0] & ~referable);
      if (of.of == goal.of && qualified && of.instance &&
          static_cast<int>(of.life()) >= static_cast<int>(needed)) {
        consider(result, finish(each.made, effect::none, of), index,
                 satisfaction::refer, of.absent);
      }
    }
    // A pointer value satisfies a pointer request besides what it refers
    // to, which may be absent where a pointer to it is null.
    if (!points(taken)) {
      return result;
    }
    const bool referred = result.found;
    for (std::size_t index = 0; index < count; ++index) {
      const state &each = states[index];
      if (each.reached.of == goal.of &&
          pointer_satisfies(each.reached, goal, needed, with, referable,
                            referred)) {
        consider(result, finish(each.made, effect::none, each.reached), index,
                 satisfaction::point, each.reached.absent);
      }
    }
    return result;
  }
  for (std::size_t index = 0; index < count; ++index) {
    const state &each = states[index];
    const thing &of = each.reached;
    if (of.of != goal.of || !has_shape(of, goal)) {
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

// What satisfies a request of what a search reached: of the closed layer
// sources it reached from alone, when they satisfy it, as no plan through a
// wrap is cheaper than one of them; of every state otherwise.
template <typename Finish>
constexpr answer satisfy_closed(const search &found, const request &goal,
                                category taken, lifetime needed,
                                const context &with, Finish finish) {
  if (found.closed != 0) {
    const answer closed =
        satisfy_states(found.states, found.closed, found.overflow, goal, taken,
                       needed, with, finish);
    if (closed.found) {
      return closed;
    }
  }
  return satisfy_states(found.states, found.count, found.overflow, goal, taken,
                        needed, with, finish);
}

constexpr answer satisfy(const search &found, const request &goal,
                         category taken, lifetime needed, const context &with) {
  return satisfy_closed(found, goal, taken, needed, with, affected_cost{});
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
  // The state of the search each thing is.
  std::uint16_t states[max_plan + 1] = {};
  std::size_t count = 0;
  satisfaction how = satisfaction::refer;
  cost fewest;
  // Whether the search held fewer states than it reached: the plan is not to
  // be trusted.
  bool overflow = false;
};

constexpr plan answer_plan(const search &found, const answer &result);

// The plan of the steps to a state of a search, which satisfies nothing.
constexpr plan state_plan(const search &found, std::uint16_t state) {
  answer reached;
  reached.found = true;
  reached.state = state;
  return answer_plan(found, reached);
}

constexpr plan answer_plan(const search &found, const answer &result) {
  plan made;
  made.overflow = result.overflow;
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
    made.states[index] = static_cast<std::uint16_t>(path[length - 1 - index]);
    if (index) {
      made.steps[index - 1] = each.via;
    }
  }
  made.how = result.how;
  made.fewest = result.fewest;
  return made;
}

// Who takes a step, as the specification splits them. A binding knows the
// type it stores, a request only the interface it asks for: the binding takes
// the steps that need the type it stores, which go into what it stores, and
// every step from what holds the leaf, when the request asks for the
// interface, which a request cannot name. What they reach are its layer
// sources, which it publishes, of no request. A request takes every other
// step from them, each of which adds a layer or converts one.
constexpr bool binding_kind(step kind) {
  return kind == step::element || kind == step::element_pointer ||
         kind == step::element_or_empty ||
         kind == step::element_or_empty_copy || kind == step::upcast;
}

constexpr bool names_leaf(const thing &of, const context &with) {
  return with.abstract && of.of == element::leaf;
}

// Whether the layers of a thing are the stored layers.
constexpr bool holds_stored(const thing &of, const thing &stored) {
  if (!stored.depth || of.depth != stored.depth) {
    return false;
  }
  for (std::size_t index = 0; index < stored.depth; ++index) {
    if (of.layers[index] != stored.layers[index]) {
      return false;
    }
  }
  return true;
}

// What a sink of the steps a binding takes passes on: a step that needs the
// stored type, or one from what holds the leaf a request of the interface
// cannot name that the upcast to the interface needs, which wraps the leaf
// itself into a layer that rebinds, or the stored layers as they are stored;
// and what a pointer the binding made points to, released. A request
// converts a handle of the interface to one of a more qualified element,
// once the binding upcasts it.
template <typename Sink> struct binding_steps_sink {
  const thing &from;
  const thing &stored;
  const context &with;
  Sink &out;

  // It takes the steps what it passes them to takes.
  constexpr bool takes(step kind, layer outer, std::size_t depth) const {
    return out.takes(kind, outer, depth);
  }
  constexpr bool takes_quals(std::size_t level, std::uint8_t quals,
                             std::size_t depth) const {
    return out.takes_quals(level, quals, depth);
  }

  constexpr void operator()(step kind, const thing &reached) {
    if (binding_kind(kind)) {
      out(kind, reached);
    } else if (kind != step::qualify && names_leaf(from, with) &&
               (kind != step::wrap ||
                (from.depth == 0 && with.kind(reached.layers[0]).rebinds) ||
                holds_stored(from, stored))) {
      out(kind, reached);
    }
    if (kind == step::adopt &&
        with.kind(reached.layers[0]).owns == owner::sole) {
      released_sink released{out};
      each_step(reached, with, released);
    }
  }

  struct released_sink {
    Sink &out;
    // It passes on the element of a handle alone.
    constexpr bool takes(step kind, layer, std::size_t) const {
      return kind == step::element;
    }
    constexpr bool takes_quals(std::size_t, std::uint8_t, std::size_t) const {
      return false;
    }

    constexpr void operator()(step kind, const thing &reached) {
      if (kind == step::element) {
        out(step::release, reached);
      }
    }
  };
};

template <typename Sink>
constexpr void each_binding_step(const thing &of, const thing &stored,
                                 const context &with, Sink &out) {
  binding_steps_sink<Sink> filtered{of, stored, with, out};
  each_step(of, with, filtered);
}

// What a sink of the steps a request takes passes on: every step a binding
// does not take, from what does not hold the leaf a request of the interface
// cannot name; but no wrap of the bare interface, as only the binding names
// the leaf it is.
template <typename Sink> struct request_steps_sink {
  const thing &from;
  Sink &out;
  // It takes no step a binding takes, and of the others those what it passes
  // them to takes.
  constexpr bool takes(step kind, layer outer, std::size_t depth) const {
    return !binding_kind(kind) && out.takes(kind, outer, depth);
  }
  constexpr bool takes_quals(std::size_t level, std::uint8_t quals,
                             std::size_t depth) const {
    return out.takes_quals(level, quals, depth);
  }

  constexpr void operator()(step kind, const thing &reached) {
    if (!binding_kind(kind) && !(kind == step::wrap && slices(from))) {
      out(kind, reached);
    }
  }
};

template <typename Sink>
constexpr void each_request_step(const thing &of, const context &with,
                                 Sink &out) {
  if (names_leaf(of, with)) {
    return;
  }
  request_steps_sink<Sink> filtered{of, out};
  each_step(of, with, filtered);
}

// What a binding publishes of its stored object: the things its steps reach,
// each with the copies and the moves of the leaf that reach it. They are of
// no request.
template <typename Search>
constexpr void add_layer_sources(Search &found, const thing &start,
                                 const context &with) {
  found.add({start, {}, no_state, step::decay});
  for (std::size_t next = 0; next < found.count; ++next) {
    const state from = found.states[next];
    any_sink<Search> sink{found, from, static_cast<std::uint16_t>(next), with};
    each_binding_step(from.reached, start, with, sink);
  }
}

// What a request reaches from the layer sources of a stored object, as reach
// gives what it reaches from the stored object itself: the layer sources,
// then the steps the request takes from them toward it.
constexpr search layer_sources(const thing &start, const context &with) {
  search found;
  add_layer_sources(found, start, with);
  return found;
}

// What the closure of layer sources passes a step to: every step a request
// takes but a wrap, to the search.
template <typename Search> struct unwrapped_sink {
  Search &found;
  const state &from;
  std::uint16_t index;
  const context &with;

  constexpr bool takes(step kind, layer, std::size_t) const {
    return kind != step::wrap;
  }
  constexpr bool takes_quals(std::size_t, std::uint8_t, std::size_t) const {
    return true;
  }

  constexpr void operator()(step kind, const thing &reached) {
    if (kind != step::wrap) {
      found.add({reached, stepped(from.made, kind, from.reached, reached, with),
                 index, kind});
    }
  }
};

// What a binding publishes, closed: its layer sources, and what a request
// reaches from them by every step but a wrap, of the layers the binding
// knows. None of it depends on the request, so the binding reaches it once,
// as the code compiles; a request then reaches from it only by a wrap, or by
// a step into a layer the binding does not know.
template <typename Search>
constexpr void close_layer_sources(Search &found, const context &with) {
  for (std::size_t next = 0; next < found.count; ++next) {
    const state from = found.states[next];
    unwrapped_sink<Search> sink{found, from, static_cast<std::uint16_t>(next),
                                with};
    each_request_step(from.reached, with, sink);
  }
}

constexpr search closed_layer_sources(const search &sources,
                                      const context &with) {
  search found = sources;
  close_layer_sources(found, with);
  return found;
}

// What a sink of the steps a request takes from a closed state passes on: a
// wrap, or a step into a layer the binding does not know, which those of the
// request may be; any other step reaches what the closure holds.
struct unclosed_sink {
  reached_sink &out;
  // The layers the binding knows: those of a lower index.
  std::size_t known;

  constexpr bool takes(step kind, layer outer, std::size_t depth) const {
    switch (kind) {
    case step::wrap:
      return out.takes(kind, outer, depth);
    case step::observe:
    case step::adopt:
    case step::convert:
    case step::unknown_bound:
      return outer >= known && out.takes(kind, outer, depth);
    default:
      return false;
    }
  }
  constexpr bool takes_quals(std::size_t, std::uint8_t, std::size_t) const {
    return false;
  }

  constexpr void operator()(step kind, const thing &reached) {
    out(kind, reached);
  }
};

// What a sink of the steps a request takes from what it made of the closure
// passes on: another wrap, a conversion of the outer layer it made to a more
// qualified element or of an array of a known bound to one of unknown bound,
// or a step into a layer the binding does not know. These the request makes
// itself, of the types it spells.
struct made_sink {
  reached_sink &out;
  std::size_t known;

  constexpr bool takes(step kind, layer outer, std::size_t depth) const {
    switch (kind) {
    case step::wrap:
    case step::qualify:
    case step::unknown_bound:
      return out.takes(kind, outer, depth);
    case step::observe:
    case step::adopt:
    case step::convert:
      return outer >= known && out.takes(kind, outer, depth);
    default:
      return false;
    }
  }
  constexpr bool takes_quals(std::size_t level, std::uint8_t quals,
                             std::size_t depth) const {
    return out.takes_quals(level, quals, depth);
  }

  constexpr void operator()(step kind, const thing &reached) {
    out(kind, reached);
  }
};

// What a request reaches from layer sources, by the steps it takes. The
// sources may be of a universe the request's extends: a binding's, of the
// layers it stores and of the handles a pointer may be taken over by, which
// the layers of the request follow.
// The steps a request takes, from every state a search holds and from every
// state they reach, added to it.
// The states below closed are closed layer sources, of a binding that knows
// the layers below known: a request steps from them only by a wrap or into a
// layer the binding does not know.
constexpr void extend_reach(search &found, std::size_t closed,
                            std::size_t known, const context &with,
                            const request &goal) {
  const bool foreign = with.layers->count > known;
  for (std::size_t next = 0; next < found.count; ++next) {
    const state &at = found.states[next];
    // A closed state steps only by a wrap, which makes a layer more, of a
    // thing of fewer than the request, or into a layer the binding does not
    // know.
    if ((next < closed && !foreign && at.reached.depth >= goal.depth) ||
        !may_satisfy(at.reached, goal, with)) {
      continue;
    }
    const state from = at;
    reached_sink sink{found, from, static_cast<std::uint16_t>(next), goal,
                      with};
    if (next >= closed) {
      made_sink made{sink, known};
      each_request_step(from.reached, with, made);
    } else {
      unclosed_sink unclosed{sink, known};
      each_request_step(from.reached, with, unclosed);
    }
  }
}

// What a request reaches from the closed layer sources of a binding that
// knows the layers below known.
constexpr search reach_closed(const search &closed, std::size_t known,
                              const context &with, const request &goal) {
  search found = closed;
  found.closed = closed.count;
  extend_reach(found, closed.count, known, with, goal);
  return found;
}

constexpr search reach_split(const thing &start, const context &with,
                             const request &goal) {
  return reach_closed(closed_layer_sources(layer_sources(start, with), with),
                      with.layers->count, with, goal);
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
constexpr thing stack_type(const request &goal) {
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
  return stack_can(stack_type(goal), with, copyable_layer{},
                   with.leaf.copyable);
}

// Whether a value of the stack of a request can be moved or copied.
constexpr bool stack_relocatable(const request &goal, const context &with) {
  return stack_can(stack_type(goal), with, relocatable_layer{},
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
    ++before.moves;
    return before;
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
  const search found = searched ? *searched : reach_split(start, with, goal);
  answer result =
      satisfy_closed(found, goal, taken, required_lifetime(rules.at, through),
                     with, handed_cost{returned, rules.runtime, &with});
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
