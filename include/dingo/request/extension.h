//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/base/config.h>
#include <dingo/request/binding_semantics.h>

#include <cstddef>
#include <cstdint>
#include <type_traits>

// The extension of a request from a layer source of a binding, derived as the
// code compiles of the request type alone. A request the closure of the layer
// sources of a binding does not satisfy is reached from a state of the closure
// of a type the request spells: the request below some of its outer layers, of
// the qualifiers it spells below its outer layer or of none, or the pointer a
// handle of it adopts or observes. What the request reaches from the state
// depends on that type and on the form of the state alone: an extension is
// derived of the request type for each, and the closure of a binding of any
// type gives the states it extends.

namespace dingo::detail::execution {

// The form of a layer source, as an extension depends on it: whether it is
// borrowed, made or a copy, whether it is an instance, whether its outer
// layer was made of what it holds, whether it owns what its pointer points to,
// and which of its levels live as long as the container.
constexpr std::uint16_t source_form(const semantics::object &object) {
  unsigned code = static_cast<unsigned>(object.origin) |
                  (object.instance ? 4u : 0u) | (object.made_layer ? 8u : 0u) |
                  (object.owns_pointee ? 16u : 0u);
  for (std::size_t index = 0; index <= object.depth; ++index) {
    if (object.lifetimes[index] == semantics::lifetime::stable) {
      code |= 32u << index;
    }
  }
  return static_cast<std::uint16_t>(code);
}

// Whether a state of a closure is a layer source an extension starts from: an
// object of no qualifiers of its own, constructed and not converted to a more
// qualified element.
constexpr bool extensible(const semantics::object &object) {
  return !object.unconstructed && !object.qualified && object.quals[0] == 0;
}

// Whether a state of a closure is of a type: of its layers, its element and
// its qualifiers below its own.
constexpr bool same_source_type(const semantics::object &state,
                                const semantics::object &type) {
  if (state.depth != type.depth || state.element != type.element) {
    return false;
  }
  for (std::size_t index = 0; index < type.depth; ++index) {
    if (state.layers[index] != type.layers[index]) {
      return false;
    }
  }
  for (std::size_t index = 1; index <= type.depth; ++index) {
    if (state.quals[index] != type.quals[index]) {
      return false;
    }
  }
  return true;
}

// The layer source of a type and a form: of the layers, the element and the
// qualifiers below its own of the type, and of the form; the rest of an object
// is the same for every one, as no extension depends on it.
constexpr semantics::object source_object(const semantics::object &type,
                                          std::uint16_t code) {
  semantics::object made;
  made.depth = type.depth;
  made.element = type.element;
  for (std::size_t index = 0; index < type.depth; ++index) {
    made.layers[index] = type.layers[index];
  }
  for (std::size_t index = 1; index <= type.depth; ++index) {
    made.quals[index] = type.quals[index];
  }
  made.origin = static_cast<semantics::origin>(code & 3u);
  made.instance = (code & 4u) != 0;
  made.made_layer = (code & 8u) != 0;
  made.owns_pointee = (code & 16u) != 0;
  for (std::size_t index = 0; index <= type.depth; ++index) {
    made.lifetimes[index] = (code & (32u << index))
                                ? semantics::lifetime::stable
                                : semantics::lifetime::request;
  }
  return made;
}

// The kinds of nested source of a request, a type it spells: the request
// below its outer layers, of its own qualifiers below the outer one or of
// none; and the pointer a handle of the request adopts or observes.
enum class nested_kind : std::uint8_t { spelled, unqualified, pointer };

// The nested source of a kind of a request below some of its outer layers,
// if it has one.
struct nested_source {
  bool exists = false;
  semantics::object type;
};

constexpr nested_source request_nested_source(const semantics::request &goal,
                                              const semantics::context &context,
                                              std::size_t offset,
                                              nested_kind kind) {
  nested_source made;
  semantics::object &type = made.type;
  type.element = goal.element;
  if (kind != nested_kind::pointer) {
    type.depth = static_cast<std::uint8_t>(goal.depth - offset);
    bool qualified = false;
    for (std::size_t index = 0; index < type.depth; ++index) {
      type.layers[index] = goal.layers[offset + index];
    }
    for (std::size_t index = 1; index <= type.depth; ++index) {
      type.quals[index] =
          kind == nested_kind::unqualified ? 0 : goal.quals[offset + index];
      qualified = qualified || goal.quals[offset + index] != 0;
    }
    made.exists = kind == nested_kind::spelled || qualified;
    return made;
  }
  if (offset >= goal.depth) {
    return made;
  }
  const semantics::layer_kind &outer = context.kind(goal.layers[offset]);
  if (!outer.adopts && !outer.observes) {
    return made;
  }
  // A handle of an array is made of the pointer to its first element.
  const bool array = offset + 1 < goal.depth &&
                     goal.layers[offset + 1] == semantics::array_layer;
  const std::size_t below = offset + (array ? 2 : 1);
  type.depth = static_cast<std::uint8_t>(goal.depth - below + 1);
  type.layers[0] =
      array ? semantics::array_pointer_layer : semantics::pointer_layer;
  for (std::size_t index = 1; index < type.depth; ++index) {
    type.layers[index] = goal.layers[below + index - 1];
  }
  for (std::size_t index = 1; index <= type.depth; ++index) {
    type.quals[index] = goal.quals[below + index - 1];
  }
  made.exists = true;
  return made;
}

// The forms a layer source of a type may have: borrowed, an instance; made,
// of its outer layer made of what it holds or not; or a copy, an instance or
// not. A level never outlives one it holds, and what is made or copied lives
// as long as the request. A pointer is owned when it was made, by a handle
// that adopts it, or borrowed, by one that observes it.
struct form_list {
  std::uint16_t forms[64] = {};
  std::size_t count = 0;
};

constexpr form_list source_forms(const semantics::object &type, bool pointer,
                                 bool adopts, bool observes) {
  form_list made;
  for (unsigned origin = 0; origin < 3; ++origin) {
    for (std::size_t request_levels = origin ? 1 : 0;
         request_levels <= type.depth + 1u; ++request_levels) {
      for (unsigned flag = 0; flag < (origin ? 2u : 1u); ++flag) {
        for (unsigned owns = 0; owns < (pointer ? 2u : 1u); ++owns) {
          if (pointer && (owns ? !adopts || origin != 1 : !observes)) {
            continue;
          }
          unsigned code = origin | (origin == 2 ? (flag ? 4u : 0u) : 4u) |
                          (origin == 1 && flag ? 8u : 0u) | (owns ? 16u : 0u);
          for (std::size_t index = request_levels; index <= type.depth;
               ++index) {
            code |= 32u << index;
          }
          made.forms[made.count++] = static_cast<std::uint16_t>(code);
        }
      }
    }
  }
  return made;
}

// What a request reaches of one layer source, by its steps into no layer but
// those of C++ itself and its own: the derivation of what satisfies it, for
// as long as it needs, and whether that may be absent.
struct extension {
  semantics::derivation derivation;
  bool absent = false;
};

// The states an extension reaches at most: of one object, by the steps of
// the layers of a request, of which it reaches few.
inline constexpr std::size_t extension_states = 48;

// What satisfies a request of one layer source, of one search, for both
// lifetimes: as long as the container, and as long as the request.
struct extension_answers {
  semantics::answer stable;
  semantics::answer transient;
};

// Extends a search of one layer source, which it holds alone, by every step
// of a request but those into the layers of C++ itself, which the closure of
// the binding takes. The search is filled in place: a constant expression
// that returned it would be kept.
template <typename Search>
constexpr void extend_source(Search &found, const semantics::object &start,
                             const semantics::context &context,
                             const semantics::request &goal) {
  found.add({start, {}, semantics::no_state, semantics::step::decay});
  found.closed = 1;
  semantics::extend_reach(found, 1, 4, context, goal);
}

template <typename Search>
constexpr extension
satisfied(const Search &found, const semantics::context &context,
          const semantics::request &goal, semantics::category taken,
          semantics::lifetime needed) {
  const semantics::answer result =
      semantics::satisfy(found, goal, taken, needed, context);
  extension made{semantics::answer_derivation(found, result), result.absent};
  made.derivation.overflow = found.overflow;
  return made;
}

DINGO_COLD constexpr extension extend(const semantics::object &start,
                                      const semantics::context &context,
                                      const semantics::request &goal,
                                      semantics::category taken,
                                      semantics::lifetime needed) {
  semantics::basic_search<extension_states> found;
  extend_source(found, start, context, goal);
  return satisfied(found, context, goal, taken, needed);
}

DINGO_COLD constexpr extension_answers
answer_extension(const semantics::object &start,
                 const semantics::context &context,
                 const semantics::request &goal, semantics::category taken) {
  semantics::basic_search<extension_states> found;
  extend_source(found, start, context, goal);
  extension_answers made{
      semantics::satisfy(found, goal, taken, semantics::lifetime::stable,
                         context),
      semantics::satisfy(found, goal, taken, semantics::lifetime::request,
                         context)};
  made.stable.overflow = made.transient.overflow = found.overflow;
  return made;
}

// A nested source of a request: below how many of its outer layers, of which
// kind, and the type.
struct nested_entry {
  std::uint8_t offset = 0;
  nested_kind kind = nested_kind::spelled;
  semantics::object type;
};

// What satisfies a request of a nested source of a form, for both lifetimes.
// Of the type the request spells, a step at least: what satisfies it of the
// layer source alone is the closure's, in the context of the binding. Of a
// type of fewer qualifiers, the layer source may satisfy it as C++ converts a
// pointer to more of them. Of the semantics alone, so that requests of the
// same layers derive it once.
constexpr extension_answers nested_answers(const nested_entry &group,
                                           std::uint16_t code,
                                           const semantics::context &context,
                                           const semantics::request &goal,
                                           semantics::category taken) {
  extension_answers made =
      answer_extension(source_object(group.type, code), context, goal, taken);
  for (semantics::answer *each : {&made.stable, &made.transient}) {
    if (group.kind == nested_kind::spelled && each->found && each->state == 0) {
      *each = semantics::answer{};
    }
  }
  return made;
}

// The answers and the derivations of the extension of a nested source of a
// form, for both lifetimes, of one search: as long as the container, and as
// long as the request.
struct extension_derivations {
  semantics::answer answers[2];
  semantics::derivation derivations[2];
  bool overflow = false;
};

DINGO_COLD constexpr extension_derivations
nested_extension(const nested_entry &group, std::uint16_t code,
                 const semantics::context &context,
                 const semantics::request &goal, semantics::category taken) {
  semantics::basic_search<extension_states> found;
  extend_source(found, source_object(group.type, code), context, goal);
  extension_derivations made;
  made.overflow = found.overflow;
  for (std::size_t lifetime = 0; lifetime < 2; ++lifetime) {
    semantics::answer reached = semantics::satisfy(
        found, goal, taken,
        lifetime ? semantics::lifetime::request : semantics::lifetime::stable,
        context);
    if (group.kind == nested_kind::spelled && reached.found &&
        reached.state == 0) {
      reached = semantics::answer{};
    }
    made.answers[lifetime] = reached;
    if (reached.found) {
      made.derivations[lifetime] = semantics::answer_derivation(found, reached);
    }
  }
  return made;
}

// The extension of a request type of the abstract interface of the leaf
// (Abstract) or of the leaf itself: its context, what it is of, and its
// nested sources.
template <typename Request, bool Abstract> struct request_extension {
  using category = requested_category<Request>;
  using shape = typename category::shape;
  using element_type = innermost_t<shape, void>;
  using stack = stack_t<shape, element_type>;
  using universe_type = layer_universe<typename stack::layers>;
  using identities = typename universe_type::identities;

  static constexpr bool modeled =
      category::modeled && stack::element != stack_element::other;

  // The traits of the leaf the request type gives: none of the leaf of an
  // interface, nor of one incomplete where the request is compiled.
  static constexpr semantics::leaf_traits leaf() {
    if constexpr (Abstract || !is_complete_v<element_type>) {
      return {false, false};
    } else {
      return leaf_traits_v<element_type>();
    }
  }

  static constexpr semantics::context context() {
    return {leaf(),
            {false, false, false},
            Abstract,
            applied_limitations,
            &universe_type::value};
  }

  static constexpr semantics::request goal() {
    semantics::request made;
    place_layers<universe_type>(made.layers, typename stack::layers{});
    made.depth =
        static_cast<std::uint8_t>(type_list_size_v<typename stack::layers>);
    made.element =
        Abstract ? semantics::element::abstract : semantics::element::leaf;
    stack_quals<shape, element_type>::fill(made.quals);
    return made;
  }

  static constexpr std::size_t max_nested = 3 * (semantics::max_layers + 1);
  struct nested_list {
    nested_entry entries[max_nested] = {};
    std::size_t count = 0;
  };
  static constexpr nested_list make_nested() {
    nested_list made;
    if constexpr (modeled) {
      const semantics::context own = context();
      const semantics::request whole = goal();
      for (std::size_t offset = 0; offset <= whole.depth; ++offset) {
        for (const nested_kind kind :
             {nested_kind::spelled, nested_kind::unqualified,
              nested_kind::pointer}) {
          // The type the request spells is extended by no step: one keeps
          // its layers, and its qualifiers are those the request names.
          if (offset == 0 && kind == nested_kind::spelled) {
            continue;
          }
          const nested_source type =
              request_nested_source(whole, own, offset, kind);
          if (type.exists) {
            made.entries[made.count++] = {static_cast<std::uint8_t>(offset),
                                          kind, type.type};
          }
        }
      }
    }
    return made;
  }
  static constexpr nested_list nested = make_nested();

  // The forms of a nested source of the request.
  static constexpr form_list forms(const nested_entry &group) {
    const semantics::request whole = goal();
    if (group.kind != nested_kind::pointer) {
      return source_forms(group.type, false, false, false);
    }
    const semantics::context own = context();
    return source_forms(group.type, true,
                        own.kind(whole.layers[group.offset]).adopts,
                        own.kind(whole.layers[group.offset]).observes);
  }

  // What satisfies the request of a nested source of a form, and its
  // extension, for both lifetimes.
  static constexpr extension_answers answers(const nested_entry &group,
                                             std::uint16_t code) {
    return nested_answers(group, code, context(), goal(), category::value);
  }
  static constexpr extension_derivations extended(const nested_entry &group,
                                                  std::uint16_t code) {
    return nested_extension(group, code, context(), goal(), category::value);
  }

  // The derivation of the extension of a nested source of a form,
  // for a lifetime.
  static constexpr extension derived(const semantics::object &type,
                                     std::uint16_t code,
                                     semantics::lifetime needed) {
    return extend(source_object(type, code), context(), goal(), category::value,
                  needed);
  }
};

} // namespace dingo::detail::execution
