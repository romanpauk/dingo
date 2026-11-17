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
  bool wraps_value = false;
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
  // The layers an owned value of it transfers to, as a mask of their indices.
  std::uint32_t transfers = 0;
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
  std::uint32_t converts_unknown_bound = 0;
  // Whether a handle of it takes over a raw pointer to an array of a known
  // bound, which it deletes as an array. It is last, as the layers of C++
  // itself list the properties before it.
  bool adopts_bounded = false;
};

// Whether a type can be a layer of these properties, as spec/semantics.py
// defines a layer. A layer that owns nothing, or shares what it owns, holds
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
         !(kind.owns == owner::none && (kind.wraps_value || kind.adopts)) &&
         (!kind.adopts || kind.shares_element) &&
         (!kind.adopts_bounded || kind.adopts) &&
         (!kind.rebinds || kind.shares_element) &&
         (!kind.qualifies || kind.shares_element) &&
         (!kind.converts_unknown_bound || kind.shares_element) &&
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
  bool factory_made = false;
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

constexpr bool owned(semantics::origin origin) {
  return origin != origin::borrowed;
}

// The element of a stack of layers.
enum class element : std::uint8_t { leaf, abstract };

// The known gaps of the library, which the expectations apply on top of the
// rules.
enum class limitation : std::uint8_t {
  made_holder_handover = 1 << 0,
  converted_copy_pointer = 1 << 1,
  qualified_below_element = 1 << 2,
};

// The properties of its registration a derivation uses, and the layers it
// may make.
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

  constexpr const layer_kind &kind(layer index) const {
    return layers->kinds[index];
  }
};

// The most layers an object has: a request has at most three wrappers around
// its interface, which may be a layer of its own, and an adopted pointer of
// an array becomes a handle of the array.
inline constexpr std::size_t max_layers = 6;

// An object a derivation reaches, as spec/semantics.py describes a Object.
struct object {
  layer layers[max_layers] = {};
  std::uint8_t depth = 0;
  semantics::element element = semantics::element::leaf;
  // How long each layer and the element live, outermost first.
  lifetime lifetimes[max_layers + 1] = {};
  semantics::origin origin = semantics::origin::borrowed;
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

constexpr bool operator==(const object &a, const object &b) {
  if (a.depth != b.depth || a.element != b.element || a.origin != b.origin ||
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

// An object with its outer layers replaced: the layers of front, then those of
// the object from below on; the lifetimes given for front, then those of the
// object from below on.
constexpr object restacked(const object &source, const layer *front,
                           std::size_t count, std::size_t below,
                           const lifetime *lives) {
  object made = source;
  std::size_t depth = 0;
  for (std::size_t index = 0; index < count; ++index) {
    made.lifetimes[depth] = lives[index];
    made.quals[depth] = 0;
    made.layers[depth++] = front[index];
  }
  for (std::size_t index = below; index <= source.depth; ++index) {
    made.lifetimes[depth] = source.lifetimes[index];
    made.quals[depth] = source.quals[index];
    if (index < source.depth) {
      made.layers[depth] = source.layers[index];
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

// An object with one more layer around it, which lives as the given lifetime.
constexpr object wrapped(const semantics::object &object, layer outer,
                         lifetime life) {
  return restacked(object, &outer, 1, 0, &life);
}

// How many of the layers from one of them on a copy or a move of a stack
// makes anew: those down to the first that shares its element; all of them
// and the element when none does.
constexpr std::size_t copied_layers(const semantics::object &object,
                                    const semantics::context &context,
                                    std::size_t first = 0) {
  for (std::size_t depth = 1; first + depth <= object.depth; ++depth) {
    if (context.kind(object.layers[first + depth - 1]).shares_element) {
      return depth;
    }
  }
  return object.depth - first + 1;
}

constexpr bool makes_leaf_anew(const semantics::object &object,
                               const semantics::context &context,
                               std::size_t first = 0) {
  return copied_layers(object, context, first) > object.depth - first;
}

// Whether a copy or a move of an object would slice the leaf: the interface
// without a layer is the base the leaf is.
constexpr bool slices(const semantics::object &object) {
  return object.element == element::abstract && object.depth == 0;
}

// The qualifiers of the element of the outer layer of an object: as the layer
// declares them, and those of the layer too when it holds the element in
// place.
constexpr std::uint8_t held_quals(const semantics::object &object,
                                  const layer_kind &kind) {
  return static_cast<std::uint8_t>(object.quals[1] |
                                   (kind.shares_element ? 0 : object.quals[0]));
}

constexpr bool copies_element(const semantics::object &object,
                              const semantics::context &context) {
  if (context.scope.publishes_early && object.instance &&
      object.origin != origin::copied) {
    return false;
  }
  return context.leaf.copyable;
}

constexpr bool can_copy(const semantics::object &object,
                        const semantics::context &context) {
  if (slices(object)) {
    return false;
  }
  const std::size_t depth = copied_layers(object, context);
  for (std::size_t index = 0; index < depth && index < object.depth; ++index) {
    if (!context.kind(object.layers[index]).copyable) {
      return false;
    }
  }
  // What a copy makes anew is not volatile, but a leaf that copies a
  // volatile one.
  for (std::size_t index = 0; index < depth && index <= object.depth; ++index) {
    if ((object.quals[index] & volatile_qual) &&
        !(index == object.depth && context.leaf.copies_volatile)) {
      return false;
    }
  }
  return depth <= object.depth || copies_element(object, context);
}

// A const or a volatile object is not moved from.
constexpr bool can_move(const semantics::object &object,
                        const semantics::context &context) {
  const std::size_t depth = copied_layers(object, context);
  for (std::size_t index = 0; index < depth && index < object.depth; ++index) {
    if (!context.kind(object.layers[index]).relocatable) {
      return false;
    }
  }
  for (std::size_t index = 0; index < depth && index <= object.depth; ++index) {
    if (object.quals[index]) {
      return false;
    }
  }
  return depth <= object.depth || context.leaf.movable;
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
  maybe_element,
  maybe_element_copy,
  transfer,
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
  object reached;
};

// The most steps from one object.
inline constexpr std::size_t max_transitions = 5 * max_kinds + 16;

struct transitions_list {
  transition items[max_transitions] = {};
  std::size_t count = 0;

  constexpr void add(step kind, const object &reached) {
    items[count++] = {kind, reached};
  }

  // It takes a step of every kind, of every outer layer and qualifier.
  constexpr bool takes(step, layer, std::size_t) const { return true; }
  constexpr bool takes_quals(std::size_t, std::uint8_t, std::size_t) const {
    return true;
  }

  constexpr void operator()(step kind, const object &reached) {
    add(kind, reached);
  }
};

// The deepest stack a derivation makes.
inline constexpr std::size_t derivation_depth = 4;

// The object of a copy: a new object the request owns. What a layer that
// shares its element holds is not copied.
constexpr object copied(const semantics::object &object,
                        const semantics::context &context) {
  const std::size_t depth = copied_layers(object, context);
  semantics::object made = object;
  for (std::size_t index = 0; index < depth && index <= object.depth; ++index) {
    made.lifetimes[index] = lifetime::request;
  }
  made.origin = origin::copied;
  made.instance = object.instance && depth <= object.depth;
  made.quals[0] = 0;
  return made;
}

// Whether the outer layer of an object gives its element: not a raw pointer
// the request owns or an array that decays, which take other steps; nor a
// layer the derivation wrapped a value in, whose element is what it was made
// of, as it was; nor one it converted to a more qualified element, whose
// element is that of what it was converted of.
constexpr bool gives_element(const semantics::object &object,
                             const layer_kind &kind) {
  return kind.gives_element && !kind.decays &&
         !(kind.adopted && object.owns_pointee) && !object.full &&
         !object.qualified;
}

// The element of the outer layer of an object that gives it.
constexpr object element_object(const semantics::object &object,
                                const semantics::context &context) {
  const layer outer = object.layers[0];
  const layer_kind &kind = context.kind(outer);
  semantics::object inner = restacked(object, nullptr, 0, 1, nullptr);
  inner.origin = kind.owns == owner::sole ? object.origin : origin::borrowed;
  inner.contained = object.contained != no_layer ? object.contained
                    : object.lifetimes[1] == lifetime::request ? outer
                                                               : no_layer;
  inner.unconstructed =
      context.scope.publishes_early && object.instance && object.depth > 1;
  inner.absent = object.absent || kind.may_be_empty;
  inner.quals[0] = held_quals(object, kind);
  inner.owns_pointee = false;
  inner.made_layer = false;
  return inner;
}

// What is derived of the element of a layer that may be empty without
// failing when it is: a pointer to the element, but into a copy a binding
// converts where the library limits it, and the optional the layer holds.
template <typename Sink>
constexpr void empty_safe_steps(const semantics::object &object,
                                const semantics::object &inner,
                                const semantics::context &context, Sink &out) {
  const layer below = inner.depth ? inner.layers[0] : no_layer;
  semantics::object pointer;
  if (below != no_layer && context.kind(below).decays) {
    pointer = inner;
    pointer.layers[0] = array_pointer_layer;
    pointer.lifetimes[0] = object.lifetimes[0];
    pointer.quals[1] = held_quals(inner, context.kind(below));
    pointer.quals[0] = 0;
  } else {
    pointer = wrapped(inner, pointer_layer, object.lifetimes[0]);
  }
  pointer.origin = origin::borrowed;
  pointer.absent = object.absent;
  if (object.origin != origin::copied ||
      !context.limited(limitation::converted_copy_pointer)) {
    out(step::element_pointer, pointer);
  }
  if (below != no_layer && context.kind(below).has_empty_value &&
      !inner.unconstructed) {
    if (owned(inner.origin)) {
      semantics::object same = inner;
      same.absent = object.absent;
      out(step::maybe_element, same);
    } else if (can_copy(inner, context)) {
      semantics::object copy = copied(inner, context);
      copy.absent = object.absent;
      out(step::maybe_element_copy, copy);
    }
  }
}

// Whether the outer layer of an object is made as the layer a request needs, of
// the qualifiers of its element too: one the request made of a value or of a
// pointer. A handle of the interface is one the binding made of the leaf and
// upcast, which a request converts.
constexpr bool made_whole(const semantics::object &object) {
  return object.made_layer &&
         !(object.element == element::abstract && object.depth == 1);
}

// The objects a pointer or a handle converts to of a more qualified element,
// as a pointer to the element converts, from the level of the object on: each
// level of qualifiers that may gain more, the element always, a level below
// it only where every level above it is a raw pointer made const and the
// library does not limit it.
template <typename Sink>
constexpr void qualify_levels(const semantics::object &object,
                              const semantics::context &context,
                              semantics::object &made, std::size_t level,
                              bool may_gain, bool gained, Sink &out) {
  if (level > object.depth || level > max_layers) {
    if (gained) {
      out(step::qualify, made);
    }
    return;
  }
  const std::uint8_t got = object.quals[level];
  for (std::uint8_t wanted = 0; wanted < 4; ++wanted) {
    if ((got & ~wanted) || (wanted != got && !may_gain) ||
        !out.takes_quals(level, wanted, object.depth)) {
      continue;
    }
    const bool below = level < object.depth &&
                       context.kind(object.layers[level]).adopted &&
                       !context.kind(object.layers[level]).adopted_array &&
                       (wanted & const_qual) &&
                       !context.limited(limitation::qualified_below_element);
    made.quals[level] = wanted;
    qualify_levels(object, context, made, level + 1, may_gain && below,
                   gained || wanted != got, out);
  }
  made.quals[level] = got;
}

// A pointer or a handle converted to one of a more qualified element: a new
// object, which holds the same element, moved out of one the request owns or
// copied of another. A pointer the request owns is handed over or taken over
// instead, never converted.
template <typename Sink>
constexpr void qualify_steps(const semantics::object &object,
                             const semantics::context &context, Sink &out) {
  if (!object.depth || !context.kind(object.layers[0]).qualifies ||
      object.qualified || object.owns_pointee ||
      !out.takes(step::qualify, object.layers[0], object.depth)) {
    return;
  }
  const bool moved = owned(object.origin) && can_move(object, context);
  if (!moved && !can_copy(object, context)) {
    return;
  }
  semantics::object made = object;
  // A layer made at once is the same object, of the qualifiers it needs.
  if (!made_whole(object)) {
    made.lifetimes[0] = lifetime::request;
  }
  made.origin = moved ? object.origin : origin::copied;
  made.quals[0] = 0;
  made.qualified = true;
  qualify_levels(object, context, made, 1, true, false, out);
}

// A pointer or a handle of an array of a known bound converted to one of an
// array of unknown bound, which holds the same array: a new object, moved out
// of one the request owns or copied of another. One converted to a more
// qualified element is not converted again, and a pointer the request owns is
// handed over or taken over instead.
template <typename Sink>
constexpr void unknown_bound_steps(const semantics::object &object,
                                   const semantics::context &context,
                                   Sink &out) {
  if (object.depth < 2 || !context.kind(object.layers[1]).bounded ||
      object.qualified || object.owns_pointee) {
    return;
  }
  const std::uint32_t targets =
      context.kind(object.layers[0]).converts_unknown_bound;
  if (!targets) {
    return;
  }
  const bool moved = owned(object.origin) && can_move(object, context);
  if (!moved && !can_copy(object, context)) {
    return;
  }
  for (std::size_t index = 0; index < context.layers->count; ++index) {
    if ((targets & (std::uint32_t{1} << index)) &&
        out.takes(step::unknown_bound, static_cast<layer>(index),
                  object.depth)) {
      semantics::object made = object;
      made.layers[0] = static_cast<layer>(index);
      made.layers[1] = array_layer;
      made.lifetimes[0] = lifetime::request;
      made.origin = moved ? object.origin : origin::copied;
      made.quals[0] = 0;
      made.made_layer = false;
      out(step::unknown_bound, made);
    }
  }
}

// The steps the library takes from an object: those the semantics allow
// that no limitation blocks. They are passed to a sink, out(kind, object), one
// by one: a constant expression returns no list of them, which a compiler
// would keep. A step but a conversion to a more qualified element makes an
// object not converted to one. A runtime resolution takes them only as it
// derives a request type, once.
template <typename Sink>
DINGO_COLD constexpr void each_step(const semantics::object &given,
                                    const semantics::context &context,
                                    Sink &out) {
  if (given.unconstructed) {
    return;
  }
  semantics::object object = given;
  object.qualified = false;
  const lifetime request = lifetime::request;
  if (object.depth) {
    const layer outer = object.layers[0];
    const layer_kind &kind = context.kind(outer);
    if (kind.adopted && !kind.adopted_array && !object.owns_pointee) {
      // A handle that does not own its element, made of a raw pointer to it
      // that owns nothing either: a new object, which points to what the
      // pointer does.
      for (std::size_t index = 0; index < context.layers->count; ++index) {
        if (context.layers->kinds[index].observes &&
            out.takes(step::observe, static_cast<layer>(index), object.depth)) {
          const layer target = static_cast<layer>(index);
          semantics::object observer =
              restacked(object, &target, 1, 1, &request);
          observer.origin = origin::made;
          observer.made_layer = true;
          out(step::observe, observer);
        }
      }
    }
    if (kind.decays) {
      if (out.takes(step::decay, array_pointer_layer, object.depth)) {
        semantics::object decayed =
            restacked(object, &array_pointer_layer, 1, 1, &request);
        decayed.quals[1] = held_quals(object, kind);
        decayed.owns_pointee = object.origin == origin::made;
        decayed.made_layer = false;
        out(step::decay, decayed);
      }
      if (object.origin == origin::made &&
          out.takes(step::hand_over, pointer_layer, object.depth + 1u)) {
        semantics::object handed = wrapped(object, pointer_layer, request);
        handed.owns_pointee = true;
        handed.made_layer = false;
        out(step::hand_over, handed);
      }
    } else if (kind.adopted && object.owns_pointee) {
      for (std::size_t index = 0; index < context.layers->count; ++index) {
        // A handle that does not delete an array of a known bound as an
        // array takes over no pointer to one.
        const bool bounded = !kind.adopted_array && object.depth > 1 &&
                             context.kind(object.layers[1]).bounded;
        if (!context.layers->kinds[index].adopts ||
            (bounded && !context.layers->kinds[index].adopts_bounded) ||
            !out.takes(step::adopt, static_cast<layer>(index),
                       object.depth + (kind.adopted_array ? 1u : 0u))) {
          continue;
        }
        const layer front[] = {static_cast<layer>(index), array_layer};
        const lifetime lives[] = {request, request};
        semantics::object adopted =
            restacked(object, front, kind.adopted_array ? 2 : 1, 1, lives);
        adopted.origin = origin::made;
        adopted.owns_pointee = false;
        adopted.made_layer = true;
        out(step::adopt, adopted);
      }
    }
    if (gives_element(given, kind) &&
        out.takes(step::element, outer, object.depth)) {
      const semantics::object inner = element_object(object, context);
      out(step::element, inner);
      if (kind.may_be_empty && !kind.adopted) {
        empty_safe_steps(object, inner, context, out);
      }
    }
    // Not a layer the derivation made, which it makes as the other at once,
    // nor one it converted to a more qualified element.
    if (owned(object.origin) && !object.quals[0] && !object.made_layer &&
        !given.qualified) {
      for (std::size_t index = 0; index < context.layers->count; ++index) {
        if ((kind.transfers & (std::uint32_t{1} << index)) &&
            out.takes(step::transfer, static_cast<layer>(index),
                      object.depth)) {
          const layer target = static_cast<layer>(index);
          out(step::transfer, restacked(object, &target, 1, 1, &request));
        }
      }
    }
  }
  if (object.element == element::leaf && context.abstract &&
      out.takes(step::upcast, no_layer, object.depth)) {
    const layer_kind *outer =
        object.depth ? &context.kind(object.layers[0]) : nullptr;
    semantics::object upcast = object;
    upcast.element = element::abstract;
    if (outer != nullptr && !outer->gives_element) {
      // The pointer to the first element of an array is a pointer to that
      // element as the interface, not to an array of the interface: the
      // request does not own what it points to.
      if (object.depth == 1 && outer->shares_element) {
        upcast.layers[0] = pointer_layer;
        upcast.origin = origin::borrowed;
        upcast.owns_pointee = false;
        out(step::upcast, upcast);
      }
    } else if (outer == nullptr) {
      // The leaf as the base it is: the same object.
      out(step::upcast, upcast);
    } else if (object.depth == 1 && outer->shares_element && outer->copyable &&
               !(object.quals[0] & volatile_qual)) {
      // A new pointer or handle of the interface.
      upcast.quals[0] = 0;
      out(step::upcast, upcast);
    } else if (object.depth == 1 && outer->shares_element &&
               owned(object.origin) && !object.quals[0]) {
      upcast.lifetimes[0] = request;
      out(step::upcast, upcast);
    } else if (object.depth > 1) {
      const std::size_t depth = copied_layers(object, context);
      bool copyable = true;
      for (std::size_t index = 0; index < object.depth; ++index) {
        copyable = copyable && context.kind(object.layers[index]).copyable;
      }
      for (std::size_t index = 0; index < depth; ++index) {
        copyable = copyable && !(object.quals[index] & volatile_qual);
      }
      if (depth == object.depth && copyable) {
        upcast.origin = origin::copied;
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
  if (!given.qualified &&
      out.takes(step::copy, object.depth ? object.layers[0] : no_layer,
                object.depth) &&
      can_copy(object, context)) {
    out(step::copy, copied(object, context));
  }
  qualify_steps(given, context, out);
  unknown_bound_steps(given, context, out);
  const bool shares = object.origin == origin::copied && object.instance;
  // A raw pointer that owns what it points to is not wrapped: it is handed
  // over, or taken over by a handle.
  if ((object.origin == origin::made || shares) && can_move(object, context) &&
      object.depth < derivation_depth && !object.owns_pointee) {
    const std::size_t depth = copied_layers(object, context);
    semantics::object inner = object;
    lifetime outer = request;
    if (shares && object.lifetimes[depth] == lifetime::stable) {
      for (std::size_t index = 0; index < depth; ++index) {
        inner.lifetimes[index] = lifetime::stable;
      }
      outer = lifetime::stable;
    }
    for (std::size_t index = 0; index < context.layers->count; ++index) {
      const layer_kind &kind = context.layers->kinds[index];
      if (!kind.wraps_value || (kind.holds_element_only && object.depth) ||
          (slices(object) && !kind.shares_element) ||
          !out.takes(step::wrap, static_cast<layer>(index),
                     object.depth + 1u)) {
        continue;
      }
      semantics::object made = wrapped(inner, static_cast<layer>(index), outer);
      made.full = static_cast<std::uint8_t>(object.full + 1);
      made.owns_pointee = false;
      made.made_layer = true;
      out(step::wrap, made);
    }
  }
}

// The steps the library takes from an object, as a list. It is a template,
// so that only a file that lists them instantiates the steps into a list.
template <typename List = transitions_list>
constexpr List steps(const semantics::object &object,
                     const semantics::context &context) {
  List out;
  each_step(object, context, out);
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

// Whether a cost of an object is no better than another of it: of the same
// copies and moves of the leaf, and of no fewer objects. A derivation from
// it costs no less, so a search that drops it ends.
constexpr bool dominates(cost a, cost b) {
  return same_leaf(a, b) && a.kept <= b.kept && a.made <= b.made;
}

// What a step does to the leaf.
enum class effect : std::uint8_t { none, copy, move };

constexpr effect step_effect(step kind, const semantics::object &object,
                             const semantics::context &context) {
  if (kind == step::maybe_element_copy) {
    return makes_leaf_anew(object, context, 1) ? effect::copy : effect::none;
  }
  if ((kind == step::copy || kind == step::wrap) &&
      makes_leaf_anew(object, context)) {
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

// The cost after a step from an object to what it reaches: of the leaf, and of
// the object it makes, kept when it lives as long as the container. Every
// step but the element, the upcast of the leaf itself, and the qualifiers of
// a layer made at once, makes one.
constexpr cost stepped(cost before, step kind, const semantics::object &object,
                       const semantics::object &reached,
                       const semantics::context &context) {
  cost made = affected(before, step_effect(kind, object, context));
  const bool makes = kind != step::element && kind != step::maybe_element &&
                     !(kind == step::upcast && object.depth == 0) &&
                     !(kind == step::qualify && made_whole(object));
  if (makes) {
    if (reached.lifetimes[0] == lifetime::stable) {
      ++made.kept;
    } else {
      ++made.made;
    }
  }
  return made;
}

// What a request names: the layers around its element, of which the
// outer shape_depth are of the shape of the request and the rest of the
// interface it is of.
struct request {
  layer layers[max_layers] = {};
  std::uint8_t depth = 0;
  semantics::element element = semantics::element::leaf;
  std::uint8_t shape_depth = 0;
  // The qualifiers of the object the request is of and of each layer below,
  // outermost first.
  std::uint8_t quals[max_layers + 1] = {};
};

// Whether the layers of an object may still become the layers of a request:
// those below its outermost are those of the request below the same depth,
// and its outermost is the request's, or converts to it. A wrap or an
// observer is made only as the layer of the request at its depth: neither
// is converted, and taking its element gives what it was made of, so any
// other leads nowhere.
// The search it directs reaches what satisfies a request at the same cost as
// the search of every object; the test of the semantics checks it for every
// request.
constexpr bool may_become(const semantics::object &object, const request &goal,
                          const semantics::context &context) {
  if (object.depth > goal.depth) {
    return false;
  }
  const std::size_t offset = goal.depth - object.depth;
  for (std::size_t index = 1; index < object.depth; ++index) {
    if (object.layers[index] != goal.layers[offset + index]) {
      return false;
    }
  }
  return object.depth == 0 || object.layers[0] == goal.layers[offset] ||
         (context.kind(object.layers[0]).transfers &
          (std::uint32_t{1} << goal.layers[offset])) != 0;
}

// Whether a request may still reach its goal from an object by its steps. Each
// adds an outer layer or converts the outermost, and the conversion to an
// array of unknown bound converts the one below it too, so an object is of no
// use when it has more layers than the request, but for a raw pointer to
// what the request is, which a request of a pointer takes, or an array that
// decays to one; or when a layer below its outermost two is not the
// request's at the same depth.
constexpr bool may_satisfy(const semantics::object &object, const request &goal,
                           const semantics::context &context) {
  if (object.depth > goal.depth + 1 ||
      (object.depth == goal.depth + 1 &&
       !context.kind(object.layers[0]).adopted &&
       !context.kind(object.layers[0]).decays)) {
    return false;
  }
  // The layer of the request below a layer of the object at an index past the
  // outermost: the object has at most one layer more.
  const std::size_t offset = goal.depth + 1 - object.depth;
  for (std::size_t index = 1; index < object.depth; ++index) {
    const layer wanted = goal.layers[offset + index - 1];
    if (object.layers[index] != wanted &&
        !(index == 1 && context.kind(object.layers[1]).bounded &&
          wanted == array_layer)) {
      return false;
    }
  }
  return true;
}

inline constexpr std::uint16_t no_state = 0xFFFF;

// An object a derivation reaches, with the copies and the moves of the leaf of
// one derivation that reaches it, and the state and the step it reached the
// object from first.
struct state {
  object reached;
  cost made;
  std::uint16_t parent = no_state;
  semantics::step step = step::decay;
};

// The states a search holds. The code compiles a search of the layer sources
// of every binding and of every request a static container derives, which a
// compiler may evaluate element by element: the searches of the semantics
// domains reach at most 83 states. One the code compiles that reaches more
// does not compile; a runtime resolution that reaches more fails.
inline constexpr std::size_t max_states = 256;

// The states a stored object reaches on the way to a request.
// A hash of an object and the copies and the moves of its leaf, which a state
// is found by.
constexpr std::uint32_t state_hash(const semantics::state &state) {
  const object &t = state.reached;
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
  mix(static_cast<unsigned>(t.element));
  mix(static_cast<unsigned>(t.origin));
  mix(t.instance);
  mix(t.contained);
  mix(t.unconstructed);
  mix(t.absent);
  mix(t.full);
  mix(t.owns_pointee);
  mix(t.made_layer);
  mix(t.qualified);
  mix(state.made.copies);
  mix(state.made.moves);
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

  // Adds a state, unless one of its object and of no greater cost is found
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

// Whether an object converted to a more qualified element may still become the
// request: its layers may, or a raw pointer may become the observer of the
// request at its depth, which takes the qualifiers of what the pointer points
// to; and below its outermost its qualifiers are those of the request at the
// same depth, which no later step changes.
constexpr bool quals_satisfy(const semantics::object &object,
                             const request &goal,
                             const semantics::context &context) {
  if (object.depth == 0 || object.depth > goal.depth) {
    return false;
  }
  const std::size_t offset = goal.depth - object.depth;
  const layer_kind &outer = context.kind(object.layers[0]);
  const bool observed = outer.adopted && !outer.adopted_array &&
                        context.kind(goal.layers[offset]).observes;
  if (!may_become(object, goal, context) && !observed) {
    return false;
  }
  for (std::size_t index = 1; index < object.depth; ++index) {
    if (object.layers[index] != goal.layers[offset + index]) {
      return false;
    }
  }
  for (std::size_t level = 1; level <= object.depth; ++level) {
    if (object.quals[level] != goal.quals[offset + level]) {
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
  const state &source;
  std::uint16_t index;
  const request &goal;
  const semantics::context &context;

  // A wrap or an observer is taken only as the layer of the request at its
  // depth, which is checked before the step is made.
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

  constexpr void operator()(step kind, const object &reached) {
    if ((kind == step::wrap || kind == step::observe) &&
        !(may_become(reached, goal, context) &&
          reached.layers[0] == goal.layers[goal.depth - reached.depth])) {
      return;
    }
    if (kind == step::qualify && !quals_satisfy(reached, goal, context)) {
      return;
    }
    found.add({reached,
               stepped(source.made, kind, source.reached, reached, context),
               index, kind});
  }
};

// What a closure passes a step to: every step, to the search.
template <typename Search> struct any_sink {
  Search &found;
  const state &source;
  std::uint16_t index;
  const semantics::context &context;

  // It takes a step of every kind, of every outer layer and qualifier.
  constexpr bool takes(step, layer, std::size_t) const { return true; }
  constexpr bool takes_quals(std::size_t, std::uint8_t, std::size_t) const {
    return true;
  }

  constexpr void operator()(step kind, const object &reached) {
    found.add({reached,
               stepped(source.made, kind, source.reached, reached, context),
               index, kind});
  }
};

// What a request reaches from a stored object by every step, as the
// specification searches it. It is a template, so that only a file that
// searches so instantiates the steps into its sink.
template <typename Sink = reached_sink>
constexpr search reach(const object &start, const semantics::context &context,
                       const request &goal) {
  search found;
  found.add({start, {}, no_state, step::decay});
  for (std::size_t next = 0; next < found.count; ++next) {
    const state source = found.states[next];
    Sink sink{found, source, static_cast<std::uint16_t>(next), goal, context};
    each_step(source.reached, context, sink);
  }
  return found;
}

// How a request takes the layers it names.
enum class category : std::uint8_t {
  value,
  const_value,
  lvalue_reference,
  const_lvalue_reference,
  rvalue_reference,
  pointer,
  const_pointer,
};

constexpr bool refers(semantics::category category) {
  return category == category::lvalue_reference ||
         category == category::const_lvalue_reference ||
         category == category::pointer || category == category::const_pointer;
}

constexpr bool points(semantics::category category) {
  return category == category::pointer || category == category::const_pointer;
}

constexpr bool has_layers(const semantics::object &object, const request &goal,
                          std::size_t first = 0) {
  if (object.depth != goal.depth + first) {
    return false;
  }
  for (std::size_t index = 0; index < goal.depth; ++index) {
    if (object.layers[first + index] != goal.layers[index]) {
      return false;
    }
  }
  return true;
}

// Whether an object is of the shape of a request: its layers, and below the
// object it is the qualifiers of each.
constexpr bool has_shape(const semantics::object &object, const request &goal,
                         std::size_t first = 0) {
  if (!has_layers(object, goal, first)) {
    return false;
  }
  for (std::size_t index = 1; index <= goal.depth; ++index) {
    if (object.quals[first + index] != goal.quals[index]) {
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

// Whether a pointer to the object of an object at a level, of the layers below,
// converts to a pointer of a request, as C++ converts one: the object may be
// of more qualifiers, of the mask referable, and a level below it may be of
// more when every level above it, from the object down, is a raw pointer the
// request makes const.
constexpr bool converts_quals(const semantics::object &object,
                              std::size_t first, const request &goal,
                              std::uint8_t referable,
                              const semantics::context &context) {
  if (object.quals[first] & ~referable) {
    return false;
  }
  bool through = true;
  for (std::size_t level = 1; first + level <= object.depth; ++level) {
    const std::uint8_t above = level == 1 ? referable : goal.quals[level - 1];
    const layer_kind &kind = context.kind(object.layers[first + level - 1]);
    through =
        through && kind.adopted && !kind.adopted_array && (above & const_qual);
    const std::uint8_t got = object.quals[first + level];
    const std::uint8_t wanted = goal.quals[level];
    if (got != wanted && ((got & ~wanted) || !through)) {
      return false;
    }
  }
  return true;
}

// Whether an object is a pointer value a pointer request takes: one whose
// pointee lives long enough; or one whose pointee the request owns, which
// hands that over, only where nothing is referred to.
constexpr bool pointer_satisfies(const semantics::object &object,
                                 const request &goal, lifetime needed,
                                 const semantics::context &context,
                                 std::uint8_t referable, bool referred) {
  if (!object.depth || !context.kind(object.layers[0]).adopted ||
      !has_layers(object, goal, 1) || !object.instance ||
      !converts_quals(object, 1, goal, referable, context)) {
    return false;
  }
  if (object.owns_pointee) {
    return !referred && !(context.limited(limitation::made_holder_handover) &&
                          object.contained != no_layer);
  }
  return static_cast<int>(object.lifetimes[1]) >= static_cast<int>(needed);
}

// How a request has the object that satisfies it: a reference or a pointer
// refers to it, a pointer request takes it as the pointer value it is, an
// rvalue reference takes what is made for the request, or a value is moved or
// copied of it.
enum class access : std::uint8_t { refer, point, take, move, copy };

// The fewest copies and moves of the leaf of what satisfies a request, as the
// specification's satisfying gives it, or none; the state of the search
// that satisfies it at that cost, the first one found, and its access.
struct answer {
  bool found = false;
  cost fewest;
  bool overflow = false;
  std::uint16_t state = no_state;
  semantics::access access = semantics::access::refer;
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
// specification's fewest ranks it.
constexpr void consider(answer &result, cost made, std::size_t index,
                        semantics::access access, bool absent) {
  if (!result.found || cheaper(made, absent, result.fewest, result.absent)) {
    result.found = true;
    result.fewest = made;
    result.state = static_cast<std::uint16_t>(index);
    result.access = access;
    result.absent = absent;
  }
}

// What satisfying a request does to the leaf, of the copies and the moves
// that reach what satisfies it and the effect of satisfying it of that.
struct affected_cost {
  constexpr cost operator()(cost made, semantics::effect effect,
                            const object &) const {
    return affected(made, effect);
  }
};

template <typename Finish>
DINGO_COLD constexpr answer
satisfy_states(const state *states, std::size_t count, bool overflow,
               const request &goal, category taken, lifetime needed,
               const semantics::context &context, Finish finish) {
  answer result;
  result.overflow = overflow;
  if (refers(taken)) {
    const std::uint8_t referable = referable_quals(goal, taken);
    for (std::size_t index = 0; index < count; ++index) {
      const state &each = states[index];
      const semantics::object &object = each.reached;
      // A reference adds qualifiers only to what it refers to; a pointer
      // converts as C++ converts one.
      const bool qualified =
          points(taken)
              ? has_layers(object, goal) &&
                    converts_quals(object, 0, goal, referable, context)
              : has_shape(object, goal) && !(object.quals[0] & ~referable);
      if (object.element == goal.element && qualified && object.instance &&
          static_cast<int>(object.life()) >= static_cast<int>(needed)) {
        consider(result, finish(each.made, effect::none, object), index,
                 access::refer, object.absent);
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
      if (each.reached.element == goal.element &&
          pointer_satisfies(each.reached, goal, needed, context, referable,
                            referred)) {
        consider(result, finish(each.made, effect::none, each.reached), index,
                 access::point, each.reached.absent);
      }
    }
    return result;
  }
  for (std::size_t index = 0; index < count; ++index) {
    const state &each = states[index];
    const semantics::object &object = each.reached;
    if (object.element != goal.element || !has_shape(object, goal)) {
      continue;
    }
    if (taken == category::rvalue_reference) {
      if (object.origin == origin::made && can_move(object, context)) {
        consider(result, finish(each.made, effect::none, object), index,
                 access::take, object.absent);
      }
      continue;
    }
    if (slices(object) || object.unconstructed) {
      continue;
    }
    const bool anew = makes_leaf_anew(object, context);
    if (owned(object.origin) && can_move(object, context)) {
      consider(result,
               finish(each.made, anew ? effect::move : effect::none, object),
               index, access::move, object.absent);
    }
    if (can_copy(object, context)) {
      consider(result,
               finish(each.made, anew ? effect::copy : effect::none, object),
               index, access::copy, object.absent);
    }
  }
  return result;
}

// What satisfies a request of what a search reached: of the closed layer
// sources it reached from alone, when they satisfy it, as no derivation through
// a wrap is cheaper than one of them; of every state otherwise.
template <typename Finish>
constexpr answer satisfy_closed(const search &found, const request &goal,
                                category taken, lifetime needed,
                                const semantics::context &context,
                                Finish finish) {
  if (found.closed != 0) {
    const answer closed =
        satisfy_states(found.states, found.closed, found.overflow, goal, taken,
                       needed, context, finish);
    if (closed.found) {
      return closed;
    }
  }
  return satisfy_states(found.states, found.count, found.overflow, goal, taken,
                        needed, context, finish);
}

constexpr answer satisfy(const search &found, const request &goal,
                         category taken, lifetime needed,
                         const semantics::context &context) {
  return satisfy_closed(found, goal, taken, needed, context, affected_cost{});
}

inline constexpr std::size_t max_steps = 16;

// The steps from a stored object to what satisfies a request, and the objects
// they reach, the first the stored object; and how the last satisfies it.
struct derivation {
  bool found = false;
  step steps[max_steps] = {};
  object objects[max_steps + 1] = {};
  // The state of the search each object is.
  std::uint16_t states[max_steps + 1] = {};
  std::size_t count = 0;
  semantics::access access = semantics::access::refer;
  cost fewest;
  // Whether the search held fewer states than it reached: the derivation is not
  // to be trusted.
  bool overflow = false;
};

constexpr derivation answer_derivation(const search &found,
                                       const answer &result);

// The derivation of the steps to a state of a search, which satisfies nothing.
constexpr derivation state_derivation(const search &found,
                                      std::uint16_t state) {
  answer reached;
  reached.found = true;
  reached.state = state;
  return answer_derivation(found, reached);
}

constexpr derivation answer_derivation(const search &found,
                                       const answer &result) {
  derivation made;
  made.overflow = result.overflow;
  if (!result.found) {
    return made;
  }
  std::size_t path[max_steps + 1] = {};
  std::size_t length = 0;
  for (std::uint16_t index = result.state; index != no_state;
       index = found.states[index].parent) {
    if (length == max_steps + 1) {
      // A derivation longer than a derivation holds is none.
      return made;
    }
    path[length++] = index;
  }
  made.found = true;
  made.count = length - 1;
  for (std::size_t index = 0; index < length; ++index) {
    const state &each = found.states[path[length - 1 - index]];
    made.objects[index] = each.reached;
    made.states[index] = static_cast<std::uint16_t>(path[length - 1 - index]);
    if (index) {
      made.steps[index - 1] = each.step;
    }
  }
  made.access = result.access;
  made.fewest = result.fewest;
  return made;
}

// Who takes a step, as the specification splits them. A binding has the
// type it stores, a request only the interface it names: the binding takes
// the steps that need the type it stores, which go into what it stores, and
// every step from what holds the leaf, when the request names the
// interface, which a request cannot name. What they reach are its layer
// sources, which it publishes, of no request. A request takes every other
// step from them, each of which adds a layer or converts one.
constexpr bool binding_kind(step kind) {
  return kind == step::element || kind == step::element_pointer ||
         kind == step::maybe_element || kind == step::maybe_element_copy ||
         kind == step::upcast;
}

constexpr bool names_leaf(const semantics::object &object,
                          const semantics::context &context) {
  return context.abstract && object.element == element::leaf;
}

// Whether the layers of an object are the stored layers.
constexpr bool holds_stored(const semantics::object &object,
                            const semantics::object &stored) {
  if (!stored.depth || object.depth != stored.depth) {
    return false;
  }
  for (std::size_t index = 0; index < stored.depth; ++index) {
    if (object.layers[index] != stored.layers[index]) {
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
  const object &source;
  const object &stored;
  const semantics::context &context;
  Sink &out;

  // It takes the steps it passes on, and a handle a pointer is taken over
  // by that owns it alone, whose element it releases, of those what it
  // passes them to takes.
  constexpr bool takes(step kind, layer outer, std::size_t depth) const {
    return (passes(kind, outer) ||
            (kind == step::adopt && context.kind(outer).owns == owner::sole)) &&
           out.takes(kind, outer, depth);
  }
  constexpr bool takes_quals(std::size_t, std::uint8_t, std::size_t) const {
    return false;
  }

  constexpr bool passes(step kind, layer outer) const {
    return binding_kind(kind) ||
           (kind != step::qualify && names_leaf(source, context) &&
            (kind != step::wrap ||
             (source.depth == 0 && context.kind(outer).rebinds) ||
             holds_stored(source, stored)));
  }

  constexpr void operator()(step kind, const object &reached) {
    if (passes(kind, reached.layers[0])) {
      out(kind, reached);
    }
    if (kind == step::adopt && !reached.unconstructed &&
        context.kind(reached.layers[0]).owns == owner::sole &&
        gives_element(reached, context.kind(reached.layers[0]))) {
      out(step::release, element_object(reached, context));
    }
  }
};

template <typename Sink>
constexpr void each_binding_step(const semantics::object &object,
                                 const semantics::object &stored,
                                 const semantics::context &context, Sink &out) {
  binding_steps_sink<Sink> filtered{object, stored, context, out};
  each_step(object, context, filtered);
}

// The steps a request takes from an object: every step a binding does not
// take, which its sink takes none of, from what does not hold the leaf a
// request of the interface cannot name. It takes none from the bare
// interface, as only the binding names the leaf it is: no copy or move of it,
// and no wrap of it.
template <typename Sink>
constexpr void each_request_step(const semantics::object &object,
                                 const semantics::context &context, Sink &out) {
  if (!names_leaf(object, context) && !slices(object)) {
    each_step(object, context, out);
  }
}

// What a binding publishes of its stored object: the objects its steps reach,
// each with the copies and the moves of the leaf that reach it. They are of
// no request.
template <typename Search>
constexpr void add_layer_sources(Search &found, const object &start,
                                 const semantics::context &context) {
  found.add({start, {}, no_state, step::decay});
  for (std::size_t next = 0; next < found.count; ++next) {
    const state source = found.states[next];
    any_sink<Search> sink{found, source, static_cast<std::uint16_t>(next),
                          context};
    each_binding_step(source.reached, start, context, sink);
  }
}

// What a request reaches from the layer sources of a stored object, as reach
// gives what it reaches from the stored object itself: the layer sources,
// then the steps the request takes from them toward it.
constexpr search layer_sources(const object &start,
                               const semantics::context &context) {
  search found;
  add_layer_sources(found, start, context);
  return found;
}

// What the closure of layer sources passes a step to: every step a request
// takes but a wrap and a conversion to a more qualified element, to the
// search.
template <typename Search> struct unwrapped_sink {
  Search &found;
  const state &source;
  std::uint16_t index;
  const semantics::context &context;

  constexpr bool takes(step kind, layer, std::size_t) const {
    return kind != step::wrap && kind != step::qualify && !binding_kind(kind);
  }
  constexpr bool takes_quals(std::size_t, std::uint8_t, std::size_t) const {
    return false;
  }

  constexpr void operator()(step kind, const object &reached) {
    found.add({reached,
               stepped(source.made, kind, source.reached, reached, context),
               index, kind});
  }
};

// What a binding publishes, closed: its layer sources, and what a request
// reaches from them by every step but a wrap, of the layers whose types the
// binding has. None of it depends on the request, so the binding reaches it
// once, as the code compiles; a request then reaches from it only by a wrap, or
// by a step into a layer whose type the binding does not have.
template <typename Search>
constexpr void close_layer_sources(Search &found,
                                   const semantics::context &context) {
  for (std::size_t next = 0; next < found.count; ++next) {
    const state source = found.states[next];
    unwrapped_sink<Search> sink{found, source, static_cast<std::uint16_t>(next),
                                context};
    each_request_step(source.reached, context, sink);
  }
}

constexpr search closed_layer_sources(const search &sources,
                                      const semantics::context &context) {
  search found = sources;
  close_layer_sources(found, context);
  return found;
}

// Where a state of a search extending a closure stands: in the closure; made
// by the request of what it holds by a wrap, or by a step into a layer whose
// type the binding does not have; or open, reached by a conversion to a more
// qualified element, which the closure leaves to the request, and by any step
// after it but a wrap, as the closure would hold it.
enum class extension : std::uint8_t { closed, made, open };

// What a sink of the steps a request takes from a state of a search passes
// on to the search, as reached_sink does: no step a binding takes, and of the
// others, from a closed state: a wrap, a conversion to a more qualified
// element, or a step into a layer whose type the binding does not have, which
// those of the request may be; any other step reaches what the closure holds.
// From what the request made of the closure: another wrap, a conversion of the
// outer layer it made to a more qualified element or of an array of a known
// bound to one of unknown bound, or a step into a layer whose type the binding
// does not have, which the request makes itself, of the types it spells. From
// an open state: every step. One sink of all, so that the steps of an object
// are compiled once for a runtime resolution.
struct extending_sink : reached_sink {
  // The layers whose types the binding has: those of a lower index.
  std::size_t known;
  extension standing;

  constexpr bool takes(step kind, layer outer, std::size_t depth) const {
    if (binding_kind(kind)) {
      return false;
    }
    if (standing != extension::open) {
      switch (kind) {
      case step::wrap:
      case step::qualify:
        break;
      case step::unknown_bound:
        if (standing != extension::made && outer < known) {
          return false;
        }
        break;
      case step::observe:
      case step::adopt:
      case step::transfer:
        if (outer < known) {
          return false;
        }
        break;
      default:
        return false;
      }
    }
    return reached_sink::takes(kind, outer, depth);
  }
};

// What a request reaches from layer sources, by the steps it takes. The
// sources may be of a universe the request's extends: a binding's, of the
// layers it stores and of the handles a pointer may be taken over by, which
// the layers of the request follow.
// The steps a request takes, from every state a search holds and from every
// state they reach, added to it.
// The states below closed are closed layer sources, of a binding that has the
// types of the layers below known: a request steps from them only by a wrap or
// into a layer whose type the binding does not have.
DINGO_COLD constexpr void extend_reach(search &found, std::size_t closed,
                                       std::size_t known,
                                       const semantics::context &context,
                                       const request &goal) {
  const bool foreign = context.layers->count > known;
  for (std::size_t next = 0; next < found.count; ++next) {
    const state &current = found.states[next];
    // A closed state steps only by a wrap, which makes a layer more, of a
    // object of fewer than the request, by a conversion to a more qualified
    // element, or into a layer whose type the binding does not have.
    if ((next < closed && !foreign && current.reached.depth > goal.depth &&
         !(current.reached.depth &&
           context.kind(current.reached.layers[0]).qualifies)) ||
        !may_satisfy(current.reached, goal, context)) {
      continue;
    }
    // A state the request reached of the closure is open when a conversion to
    // a more qualified element reached it, or a step but a wrap of an open
    // one.
    extension standing = extension::closed;
    if (next >= closed) {
      standing = extension::made;
      for (std::size_t each = next; each >= closed;
           each = found.states[each].parent) {
        if (found.states[each].step == step::wrap) {
          break;
        }
        if (found.states[each].step == step::qualify) {
          standing = extension::open;
          break;
        }
      }
    }
    const state source = current;
    extending_sink extending{
        {found, source, static_cast<std::uint16_t>(next), goal, context},
        known,
        standing};
    each_request_step(source.reached, context, extending);
  }
}

// What a request reaches from the closed layer sources of a binding that
// has the types of the layers below known.
constexpr search reach_closed(const search &closed, std::size_t known,
                              const semantics::context &context,
                              const request &goal) {
  search found = closed;
  found.closed = closed.count;
  extend_reach(found, closed.count, known, context, goal);
  return found;
}

constexpr search reach_split(const object &start,
                             const semantics::context &context,
                             const request &goal) {
  return reach_closed(
      closed_layer_sources(layer_sources(start, context), context),
      context.layers->count, context, goal);
}
// Where a request stands: itself, the dependency of an unregistered class,
// or the dependency of a class a shared or a cyclical scope keeps.
enum class position : std::uint8_t { direct, consumer, shared, cyclical };

// The entry point a request is made through.
enum class entry : std::uint8_t { resolve, invoke, construct };

// What a request is, beside what it names: where it stands, the entry
// point it is made through, whether a key or an annotation selects it, and
// whether the runtime container resolves it.
struct request_rules {
  semantics::position position;
  entry through;
  bool selected;
  bool runtime;
};

// Whether a request in a category holds the shape by value: a resolved
// rvalue reference is returned by value.
constexpr bool returns_value(semantics::category category) {
  return category == category::value || category == category::const_value ||
         category == category::rvalue_reference;
}

// How long an object a request refers to has to live: a reference that is
// passed away, returned by a resolve or a construct or held by a class one of
// them returns, as long as the container; one a callable takes, or a class
// a shared or a cyclical scope keeps, while the request does.
constexpr lifetime required_lifetime(semantics::position position,
                                     entry through) {
  if (position == position::shared || position == position::cyclical ||
      through == entry::invoke) {
    return lifetime::request;
  }
  return lifetime::stable;
}

// The entry a request is resolved through: construct<T>() of a reference, of
// an rvalue reference, or of a request a key or an annotation selects,
// resolves it.
constexpr entry request_entry(entry through, semantics::position position,
                              bool selected, semantics::category category) {
  if (position == position::direct && through == entry::construct &&
      (selected || !returns_value(category) ||
       category == category::rvalue_reference)) {
    return entry::resolve;
  }
  return through;
}

// The layers of a request as an object, to query what its stack can do.
constexpr object stack_type(const request &goal) {
  semantics::object object;
  for (std::size_t index = 0; index < goal.depth; ++index) {
    object.layers[index] = goal.layers[index];
  }
  object.depth = goal.depth;
  object.element = goal.element;
  return object;
}

// Whether what a copy or a move of a stack makes anew can be: each layer
// down to the first that shares its element, and the element when none does.
template <typename Can>
constexpr bool stack_can(const semantics::object &object,
                         const semantics::context &context, Can layer_can,
                         bool element_can) {
  const std::size_t depth = copied_layers(object, context);
  for (std::size_t index = 0; index < depth && index < object.depth; ++index) {
    if (!layer_can(context.kind(object.layers[index]))) {
      return false;
    }
  }
  return depth <= object.depth || element_can;
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
constexpr bool copyable_shape(const request &goal,
                              const semantics::context &context) {
  return stack_can(stack_type(goal), context, copyable_layer{},
                   context.leaf.copyable);
}

// Whether a value of the stack of a request can be moved or copied.
constexpr bool stack_relocatable(const request &goal,
                                 const semantics::context &context) {
  return stack_can(stack_type(goal), context, relocatable_layer{},
                   context.leaf.movable) ||
         copyable_shape(goal, context);
}

// Whether a request a key or an annotation selects cannot be held by value
// in its selector, which it is passed in: a const value is copied into it,
// another one moved or copied. A direct resolve returns the value instead.
constexpr bool selector_rejects(const request &goal,
                                semantics::category category,
                                semantics::position position, entry through,
                                bool selected,
                                const semantics::context &context) {
  if (!selected ||
      (category != category::value && category != category::const_value) ||
      (position == position::direct && through == entry::resolve)) {
    return false;
  }
  if (category == category::const_value) {
    return !copyable_shape(goal, context);
  }
  return !stack_relocatable(goal, context);
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
  const semantics::context *context;

  constexpr cost operator()(cost made, semantics::effect effect,
                            const object &reached) const {
    return handed_over(
        made,
        returned && makes_leaf_anew(reached, *context) ? effect::move : effect,
        runtime);
  }
};

// The fewest copies and moves of the leaf a request resolves with, as
// spec/semantics.py's fewest gives it, and the derivation of it; none when it
// does not resolve. construct<T>() of a value it cannot resolve makes T of the
// leaf the container resolves by value: a new handle, optional or array of
// it, the rest of a bounded array value-initialized. The value is moved into
// it once it is resolved.
// The search of the request may be given, made once for several rules.
constexpr answer resolve(const object &start, const semantics::context &context,
                         const request &goal, category taken,
                         const request_rules &rules,
                         const search *searched = nullptr) {
  const entry through =
      request_entry(rules.through, rules.position, rules.selected, taken);
  if (selector_rejects(goal, taken, rules.position, through, rules.selected,
                       context)) {
    return {};
  }
  const bool returned =
      rules.position == position::direct &&
      (through == entry::resolve || through == entry::construct) &&
      taken == category::rvalue_reference;
  const search found = searched ? *searched : reach_split(start, context, goal);
  answer result = satisfy_closed(
      found, goal, taken, required_lifetime(rules.position, through), context,
      handed_cost{returned, rules.runtime, &context});
  if (result.found || found.overflow) {
    return result;
  }
  bool bounded = false;
  for (std::size_t index = 0; index < goal.shape_depth; ++index) {
    bounded = bounded || context.kind(goal.layers[index]).bounded;
  }
  if (rules.position == position::direct && through == entry::construct &&
      goal.element == element::leaf && goal.shape_depth != 0 &&
      (context.leaf.default_constructible || !bounded)) {
    request leaf;
    answer value = resolve(
        start, context, leaf, category::value,
        {position::direct, entry::resolve, rules.selected, rules.runtime});
    if (value.found) {
      value.fewest.moves = static_cast<std::uint8_t>(value.fewest.moves + 1);
    }
    return value;
  }
  return result;
}

} // namespace dingo::semantics
