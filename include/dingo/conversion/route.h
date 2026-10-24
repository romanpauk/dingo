//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

// The routes of the next implementation: registration, lookup, and
// derivation as constexpr functions over type trees and the operations. A route
// is a rule of a leaf or a form of a wrapper semantics, and the functions know
// a kind only by its descriptor, its properties and forms, so a kind of the
// user is derived as one of the library is. It interprets the rules the same
// way as spec/reference/routes.py, which is its oracle. Static containers
// evaluate it at compile time and runtime containers at registration, so both
// serve a request by the same derivation.

#include <dingo/lookup/key.h>
#include <dingo/lookup/key_kinds.gen.h>
#include <dingo/storage/scopes.gen.h>
#include <dingo/storage/storage_kinds.gen.h>
#include <dingo/type/type_tree.h>
#include <dingo/type/wrapper_traits.h>

#include <cstddef>
#include <cstdint>
#include <cstdlib>

namespace dingo {

inline constexpr std::size_t max_interfaces = 8;
inline constexpr std::size_t max_rules_per_route = 8;
inline constexpr std::size_t max_semantics = 32;

constexpr const semantics_descriptor *semantics_of(type_tree_view value,
                                                   std::size_t index = 0) {
  return value.nodes[index].kind;
}

// Whether a kind is of all of the property bits `properties`; the leaf
// kind is of none.
constexpr bool has_properties(const semantics_descriptor *kind,
                              std::uint16_t properties) {
  return kind->wrapper && (kind->properties & properties) == properties;
}

constexpr std::uint32_t operation_bit(operation form) {
  return std::uint32_t(1) << static_cast<unsigned>(form);
}

constexpr bool has_operation(const semantics_descriptor *kind, operation form) {
  return (kind->forms & operation_bit(form)) != 0;
}

// Whether a kind holds a number of elements: an extent, or one known at
// runtime.
constexpr bool holds_elements(const semantics_descriptor *kind) {
  return has_properties(kind, property::extent) ||
         has_properties(kind, property::unsized);
}

// A step of a route: a rule of a leaf, of the leaf kind, or a form of a kind.
struct route_step {
  operation form = operation{};
  const semantics_descriptor *kind = &leaf_semantics;
};

// The kind a step derives from and the one it derives, or the leaf kind for
// an element.
constexpr const semantics_descriptor *semantics_at(const route_step &step,
                                                   type_position wrapper) {
  switch (wrapper) {
  case type_position::wrapper:
    return step.kind;
  case type_position::target:
    if ((step.kind->targeted & operation_bit(step.form)) != 0) {
      return step.kind->targets[static_cast<std::size_t>(step.form)];
    }
    return &leaf_semantics;
  default:
    return &leaf_semantics;
  }
}

constexpr const semantics_descriptor *from_semantics(const route_step &step) {
  return semantics_at(step, info(step.form).from_position);
}

constexpr const semantics_descriptor *to_semantics(const route_step &step) {
  return semantics_at(step, info(step.form).to_position);
}

// A set of wrapper semantics, each with the kinds its forms lead to.
struct semantics_set {
  const semantics_descriptor *items[max_semantics] = {};
  std::size_t size = 0;

  constexpr void add(const semantics_descriptor *kind) {
    if (!kind->wrapper) {
      return;
    }
    for (std::size_t index = 0; index < size; ++index) {
      if (same_semantics(items[index], kind)) {
        return;
      }
    }
    if (size < max_semantics) {
      items[size++] = kind;
      for (std::size_t form = 0; form < max_semantics_operations; ++form) {
        if ((kind->targeted & (std::uint32_t(1) << form)) != 0) {
          add(kind->targets[form]);
        }
      }
    }
  }
};

// The kinds of a stored type tree and the ones their forms lead to.
constexpr semantics_set semantics_in(type_tree_view stored) {
  semantics_set kinds;
  for (std::size_t index = 0; index < stored.size; ++index) {
    kinds.add(stored.nodes[index].kind);
  }
  return kinds;
}

// A set of leaves, in the order they are added.
struct leaf_set {
  type_id items[type_tree_capacity] = {};
  std::size_t size = 0;

  constexpr bool contains(type_id leaf) const {
    // Most type trees name one leaf.
    if (size == 1) {
      return items[0] == leaf;
    }
    for (std::size_t index = 0; index < size; ++index) {
      if (items[index] == leaf) {
        return true;
      }
    }
    return false;
  }

  constexpr void add(type_id leaf) {
    if (!contains(leaf)) {
      items[size++] = leaf;
    }
  }
};

inline constexpr std::size_t max_key_aliases = 4;

// A binding as the routes see it: the stored type tree, the scope, and the
// leaf type trees of the declared interfaces.
struct binding_descriptor {
  type_tree_view stored;
  // The leaves the stored type tree names the binding by without declared
  // interfaces, named_leaves(stored).
  leaf_set leaves = {};
  key_descriptor key;
  // The keys of other domains the binding is also found by, each of a key
  // type of its own.
  key_descriptor aliases[max_key_aliases] = {};
  std::size_t alias_count = 0;
  // The descriptor of the scope of the binding.
  scope_info scope = {};
  type_tree_view interfaces[max_interfaces] = {};
  std::size_t interface_count = 0;
  // Whether the binding declares its factory, so its leaf need not be
  // constructible.
  bool declared_factory = false;
  // The interfaces, by bit, the stored leaf declares a user conversion to.
  std::uint32_t converted = 0;
};

enum class request_kind : std::uint8_t {
  lvalue_reference,
  rvalue_reference,
  value
};

struct request_descriptor {
  request_kind kind = request_kind::value;
  type_tree_view value;
  key_descriptor key;
};

// A derived reachable type and how it was reached.
struct reachable_type {
  type_tree value;
  value_category category = value_category::lvalue;
  route_step rules[max_rules_per_route] = {};
  // For each rule, the argument of the source wrapper it reached, or for an
  // upcast, the index of the interface it converted to.
  std::uint8_t arguments[max_rules_per_route] = {};
  std::size_t rule_count = 0;
  bool copied = false;
  // The leaves the copy rule of the route copied.
  std::uint32_t copied_leaves = 0;
  bool consumed = false;
  // Whether the reachable type is a sibling of the reachable type `primary`,
  // the reachable type of its value and category derived first: it takes the
  // route of the primary up to the step `branch`, there another alternative of
  // a variadic wrapper, and reaches the value on its own. A sibling is neither
  // derived from nor selected; a walk of the primary continues along it from
  // the object of the step when the object holds its alternative.
  bool sibling = false;
  std::uint8_t primary = 0;
  std::uint8_t branch = 0;
  // The wrapper the last rule started from: its kind, qualifiers, and extent,
  // which a request that copies or decays the reachable type borrows.
  const semantics_descriptor *source_semantics = &leaf_semantics;
  std::uint8_t source_qualifiers = 0;
  std::uint32_t source_extent = 0;
};

// The reachable types of a binding, as a view of an array that has exactly as
// many as the binding has.
struct reachable_types {
  const reachable_type *items = nullptr;
  std::size_t size = 0;
};

// Where a derivation writes the reachable types it finds. When there is no room
// left it sets `full` and writes no more, so the caller derives again with
// more.
struct reachable_type_buffer {
  reachable_type *items = nullptr;
  std::size_t capacity = 0;
  std::size_t size = 0;
  bool full = false;
};

// An array of exactly `Count` reachable types.
template <std::size_t Count> struct reachable_type_array {
  reachable_type items[Count] = {};

  constexpr reachable_types view() const { return {items, Count}; }
};

// Storage kinds

// Whether the wrapper at `index` is of a wrapper class, by the properties of
// its kind.
constexpr bool of_class(const semantics_descriptor *kind,
                        const property_class &of) {
  if (!kind->wrapper) {
    return false;
  }
  return (kind->properties & of.has) == of.has &&
         (kind->properties & of.lacks) == 0;
}

constexpr bool of_class(type_tree_view value, std::size_t index,
                        const property_class &of) {
  return of_class(semantics_of(value, index), of);
}

// The most specific storage kind of a stored type tree, by the classes of its
// outer wrappers.
constexpr storage_kind storage_kind_of(type_tree_view stored) {
  storage_kind best = storage_kind::plain;
  int depth = 0;
  for (const storage_kind_info &entry : storage_kinds) {
    if (!entry.outer.any || !of_class(stored, 0, entry.outer)) {
      continue;
    }
    if (!entry.inner.any) {
      if (depth < 1) {
        best = entry.kind;
        depth = 1;
      }
    } else if (stored.nodes[0].arity == 1 && of_class(stored, 1, entry.inner)) {
      best = entry.kind;
      depth = 2;
    }
  }
  return best;
}

constexpr std::uint8_t exposed_categories(const binding_descriptor &binding) {
  return binding.scope
      .exposures[static_cast<std::size_t>(storage_kind_of(binding.stored))];
}

// Type tree properties

constexpr bool constructible(type_tree_view value, std::size_t index = 0) {
  const semantics_descriptor *kind = semantics_of(value, index);
  if (!kind->wrapper) {
    return value.nodes[index].constructible;
  }
  return has_properties(kind, property::constructible) &&
         constructible(value, index + 1);
}

constexpr std::uint32_t constructed_leaves(type_tree_view value,
                                           std::size_t index = 0) {
  const semantics_descriptor *kind = semantics_of(value, index);
  if (!kind->wrapper) {
    return 1;
  }
  if (has_properties(kind, property::extent)) {
    return value.nodes[index].extent * constructed_leaves(value, index + 1);
  }
  return constructed_leaves(value, index + 1);
}

// The leaves a copy of the type tree copies: those its wrappers hold in place.
constexpr std::uint32_t leaves_held_by_value(type_tree_view value,
                                             std::size_t index = 0) {
  const semantics_descriptor *kind = semantics_of(value, index);
  if (!kind->wrapper) {
    return 1;
  }
  if (!has_properties(kind, property::in_place)) {
    return 0;
  }
  if (has_properties(kind, property::extent)) {
    return value.nodes[index].extent * leaves_held_by_value(value, index + 1);
  }
  return leaves_held_by_value(value, index + 1);
}

// Whether a type tree is an object that can be referred to and pointed to:
// anything but an address, a wrapper of a kind of the address form.
constexpr bool addressable(type_tree_view value) {
  return !has_operation(semantics_of(value), operation::address);
}

constexpr bool holds_in_array(type_tree_view value) {
  for (std::size_t index = 0; index < value.size; ++index) {
    if (holds_elements(semantics_of(value, index))) {
      return true;
    }
  }
  return false;
}

// The leaf at the end of the first-argument chain.
constexpr type_id innermost_leaf(type_tree_view value) {
  std::size_t index = 0;
  while (is_wrapper(semantics_of(value, index))) {
    ++index;
  }
  return value.nodes[index].type;
}

namespace detail {
constexpr void add_named_leaves(type_tree_view value, std::size_t index,
                                leaf_set &leaves) {
  const semantics_descriptor *kind = semantics_of(value, index);
  if (!is_wrapper(kind)) {
    leaves.add(value.nodes[index].type);
  } else if (!has_properties(kind, property::variadic)) {
    add_named_leaves(value, index + 1, leaves);
  } else {
    for (std::size_t argument = 0; argument < value.nodes[index].arity;
         ++argument) {
      add_named_leaves(value, argument_index(value, index, argument), leaves);
    }
  }
}
} // namespace detail

// The leaves a stored type tree names its binding by without declared
// interfaces: the leaf its wrappers hold, and through a wrapper of a variadic
// kind, the leaf of each of its alternatives, whichever the object holds. spec:
// lookup_filters.binding_filter
constexpr leaf_set named_leaves(type_tree_view value) {
  leaf_set leaves;
  detail::add_named_leaves(value, 0, leaves);
  return leaves;
}

// Whether a wrapper that a route creates can be copied: a wrapper copied as a
// handle, or one that holds its element in place and is not immovable, when
// the element can be copied.
constexpr bool created_copyable(const semantics_descriptor *kind,
                                type_tree_view argument) {
  if (has_properties(kind, property::copyable)) {
    return true;
  }
  if (has_properties(kind, property::in_place) &&
      !has_properties(kind, property::immovable)) {
    return argument.nodes[0].copyable;
  }
  return false;
}

constexpr bool is_mapped(const semantics_descriptor *kind) {
  return has_properties(kind, property::mapped);
}

constexpr bool requirements_hold(const binding_descriptor &binding,
                                 const operation_info &rule,
                                 type_tree_view bound) {
  auto requires_ = [&](requirement value) {
    return (rule.requires_ & static_cast<std::uint8_t>(value)) != 0;
  };
  if (requires_(requirement::copyable) && !bound.nodes[0].copyable) {
    return false;
  }
  // An array of the language: of an extent, and immovable.
  if (requires_(requirement::array) &&
      !has_properties(semantics_of(bound),
                      property::extent | property::immovable)) {
    return false;
  }
  if (requires_(requirement::addressable) && !addressable(bound)) {
    return false;
  }
  if (requires_(requirement::leaf) && is_wrapper(semantics_of(bound))) {
    return false;
  }
  if (rule.form == operation::copy &&
      binding.scope.rule == exposure_rule::handle_copies &&
      !has_properties(semantics_of(bound), property::sharing)) {
    return false;
  }
  return true;
}

// Derivation

namespace detail {
// The reachable type of a value and category that is no sibling, or
// no_primary.
inline constexpr std::size_t no_primary = static_cast<std::size_t>(-1);

constexpr std::size_t primary_of(const reachable_type_buffer &found,
                                 type_tree_view value,
                                 value_category category) {
  for (std::size_t index = 0; index < found.size; ++index) {
    const reachable_type &item = found.items[index];
    if (!item.sibling && item.category == category && item.value == value) {
      return index;
    }
  }
  return no_primary;
}

// How a request completes a reachable type: by copying, decaying, or neither.
constexpr int completion_class(const reachable_type &route) {
  if (route.rule_count == 0) {
    return 0;
  }
  const operation last = route.rules[route.rule_count - 1].form;
  return last == operation::copy ? 1 : last == operation::decay ? 2 : 3;
}

// Whether two reachable types a request completes by copying or decaying borrow
// alike: the wrappers their last rules start from are of one kind, qualifiers,
// and extent, so a walk to them yields one type.
constexpr bool borrow_alike(const reachable_type &left,
                            const reachable_type &right) {
  return same_semantics(left.source_semantics, right.source_semantics) &&
         left.source_qualifiers == right.source_qualifiers &&
         left.source_extent == right.source_extent;
}

// Keeps `derived` as a sibling of the reachable type `primary` when their
// routes part at a step into a variadic wrapper, by its alternatives, and they
// are completed alike; one per step and alternative.
constexpr void add_sibling(reachable_type_buffer &found, std::size_t primary,
                           reachable_type derived) {
  const reachable_type &first = found.items[primary];
  const std::size_t common = first.rule_count < derived.rule_count
                                 ? first.rule_count
                                 : derived.rule_count;
  for (std::size_t position = 0; position < common; ++position) {
    const route_step &taken = first.rules[position];
    const route_step &other = derived.rules[position];
    const bool same =
        taken.form == other.form && same_semantics(taken.kind, other.kind);
    if (same && first.arguments[position] == derived.arguments[position]) {
      continue;
    }
    if (!same || !has_properties(from_semantics(taken), property::variadic) ||
        completion_class(first) != completion_class(derived) ||
        (completion_class(first) != 3 && !borrow_alike(first, derived))) {
      return;
    }
    for (std::size_t index = 0; index < found.size; ++index) {
      const reachable_type &item = found.items[index];
      if (item.sibling && item.primary == primary && item.branch == position &&
          item.arguments[position] == derived.arguments[position]) {
        return;
      }
    }
    if (found.size == found.capacity) {
      found.full = true;
      return;
    }
    derived.sibling = true;
    derived.primary = static_cast<std::uint8_t>(primary);
    derived.branch = static_cast<std::uint8_t>(position);
    found.items[found.size++] = derived;
    return;
  }
}

// The reachable type `step` derives from `from`, of the value `value`, with the
// pattern variable bound to `bound`.
constexpr reachable_type extend(const reachable_type &from,
                                const route_step &step, type_tree_view value,
                                type_tree_view bound, std::size_t argument) {
  const operation_info &rule = info(step.form);
  reachable_type derived = from;
  derived.source_semantics = from.value.nodes[0].kind;
  derived.source_qualifiers = from.value.nodes[0].qualifiers;
  derived.source_extent = from.value.nodes[0].extent;
  derived.value = copy_of(value);
  derived.category = rule.to_category;
  derived.arguments[derived.rule_count] = static_cast<std::uint8_t>(argument);
  derived.rules[derived.rule_count++] = step;
  if (step.form == operation::copy) {
    derived.copied = true;
    derived.copied_leaves = leaves_held_by_value(bound);
    // A copy is a new object, without the qualifiers of its source.
    derived.value.nodes[0].qualifiers = 0;
  }
  derived.consumed = from.consumed || rule.rule_access == access::consume;
  return derived;
}

constexpr void add(reachable_type_buffer &found,
                   const reachable_type &derived) {
  const std::size_t primary =
      primary_of(found, derived.value, derived.category);
  if (primary != no_primary) {
    add_sibling(found, primary, derived);
    return;
  }
  if (found.size == found.capacity) {
    found.full = true;
    return;
  }
  found.items[found.size++] = derived;
}

// The value a step that is no upcast or conversion derives from `from` with
// the pattern variable bound to `bound`, or false when it is larger than
// `limit`.
constexpr bool step_value(const reachable_type &from, const route_step &step,
                          type_tree_view bound, std::size_t limit,
                          type_tree &value) {
  const type_tree &source = from.value;
  const semantics_descriptor *to = to_semantics(step);
  const semantics_descriptor *of = from_semantics(step);
  value = copy_of(bound);
  // The element of a wrapper that holds it in place, an optional or an
  // alternative, has the qualifiers of the wrapper.
  if (!is_wrapper(to) && has_properties(of, property::in_place)) {
    value.nodes[0].qualifiers |= source.nodes[0].qualifiers;
  }
  if (is_wrapper(to)) {
    type_id family = nullptr;
    if (is_mapped(to)) {
      bool keeps = is_wrapper(of) && source.nodes[0].family != nullptr;
      family = keeps ? source.nodes[0].family : any_family;
    }
    value = wrap(to, family, created_copyable(to, bound), bound);
  }
  return value.size <= limit;
}

// Applies `step` to `from` with the pattern variable bound to `bound`, the
// argument at `argument` of the source type tree when the step starts from a
// wrapper.
constexpr void apply(const binding_descriptor &binding,
                     reachable_type_buffer &found, const reachable_type &from,
                     const route_step &step, type_tree_view bound,
                     std::size_t argument, std::size_t limit) {
  const operation_info &rule = info(step.form);
  if (!requirements_hold(binding, rule, bound)) {
    return;
  }
  const type_tree &source = from.value;
  const semantics_descriptor *to = to_semantics(step);
  if (rule.upcast || rule.converts) {
    // Only the stored leaf converts to the declared interfaces, and a user
    // conversion only to the ones it declares one to.
    if (bound.nodes[0].type != innermost_leaf(binding.stored)) {
      return;
    }
    for (std::size_t index = 0; index < binding.interface_count; ++index) {
      if (rule.converts && (binding.converted & (1u << index)) == 0) {
        continue;
      }
      // An upcast keeps the qualifiers of the leaf; a conversion makes a new
      // object.
      type_tree interface = copy_of(binding.interfaces[index]);
      interface.nodes[0].qualifiers =
          rule.converts ? std::uint8_t(0) : bound.nodes[0].qualifiers;
      type_tree value = !is_wrapper(to)
                        ? interface
                        : wrap(to, source.nodes[0].family,
                               source.nodes[0].copyable, interface,
                               source.nodes[0].extent);
      add(found, extend(from, step, value, bound, index));
    }
    return;
  }
  type_tree value;
  if (step_value(from, step, bound, limit, value)) {
    add(found, extend(from, step, value, bound, argument));
  }
}

// Applies `step`, which starts from the wrapper `from` is, to each argument of
// it: only a wrapper of a variadic kind matches any of its arguments.
constexpr void apply_to_arguments(const binding_descriptor &binding,
                                  reachable_type_buffer &found,
                                  const reachable_type &from,
                                  const route_step &step, std::size_t limit) {
  std::size_t arguments =
      has_properties(semantics_of(from.value), property::variadic)
          ? from.value.nodes[0].arity
          : 1;
  for (std::size_t argument = 0; argument < arguments; ++argument) {
    type_tree bound =
        subtree(from.value, argument_index(from.value, 0, argument));
    apply(binding, found, from, step, bound, argument, limit);
  }
}
} // namespace detail

// Whether a form creates a wrapper of its kind, from an element or from a
// wrapper of the kind it leads from: wrapping, adopting, or taking an address.
// A creating form applies at the request, which names the kind it creates.
constexpr bool creates(const operation_info &rule) {
  return rule.of_semantics && rule.to_position == type_position::wrapper &&
         rule.from_position != type_position::wrapper;
}

namespace detail {
// Applies every form that does not create a wrapper to the reachable type at
// `next` of `found`, in the order of the rules: a rule of a leaf to the
// reachable type, and a form of the kind of its wrapper.
constexpr void derive_from(const binding_descriptor &binding,
                           reachable_type_buffer &found, std::size_t next,
                           std::uint8_t exposed, std::size_t limit) {
  reachable_type from = found.items[next];
  if (from.rule_count == max_rules_per_route) {
    return;
  }
  const semantics_descriptor *kind = semantics_of(from.value);
  for (const operation_info &rule : operations) {
    if (rule.from_category != from.category ||
        (rule.exposure != 0 && (exposed & rule.exposure) == 0)) {
      continue;
    }
    if (!rule.of_semantics) {
      apply(binding, found, from, route_step{rule.form, &leaf_semantics},
            from.value, 0, limit);
    } else if (rule.from_position == type_position::wrapper &&
               has_operation(kind, rule.form)) {
      apply_to_arguments(binding, found, from, route_step{rule.form, kind},
                         limit);
    }
  }
}
} // namespace detail

// Routes derive type trees at most one wrapper larger than the stored type
// tree, so a binding can publish every reachable type it serves before any
// request is known.
constexpr std::size_t derivation_bound(const binding_descriptor &binding) {
  return binding.stored.size + 1;
}

// Derives the reachable types a binding has by the forms that do not create a
// wrapper into `found`, which stops taking them when it is full.
constexpr void derive_into(const binding_descriptor &binding,
                           reachable_type_buffer &found) {
  const scope_info &scope = binding.scope;
  reachable_type start;
  start.value = copy_of(binding.stored);
  start.category = scope.materializes;
  start.consumed = scope.materializes == value_category::rvalue;
  found.items[found.size++] = start;
  std::uint8_t exposed = exposed_categories(binding);
  std::size_t limit = derivation_bound(binding);
  for (std::size_t next = 0; next < found.size; ++next) {
    if (!found.items[next].sibling) {
      detail::derive_from(binding, found, next, exposed, limit);
    }
  }
}

// The room a derivation of the binding starts with: a few reachable types for
// each node of the stored type tree and each interface, as a power of two.
constexpr std::size_t initial_room(const binding_descriptor &binding) {
  std::size_t wanted = binding.stored.size * (binding.interface_count + 1) * 2;
  std::size_t room = 8;
  while (room < wanted) {
    room *= 2;
  }
  return room;
}

inline constexpr std::size_t largest_derivation_room = 1024;

[[noreturn]] inline void
derivation_has_more_reachable_types_than_the_largest_room() {
  std::abort();
}

// Derives the reachable types of a binding into room for `Room` of them, and
// into twice the room again while they do not fit, and gives them to `use`, a
// callable of a `reachable_types`. The room is only the scratch of the
// derivation: what `use` returns does not depend on it.
template <std::size_t Room, typename Use>
constexpr auto derive_then(const binding_descriptor &binding, Use &&use) {
  reachable_type_array<Room> storage;
  reachable_type_buffer found{storage.items, Room};
  derive_into(binding, found);
  if (!found.full) {
    return use(reachable_types{storage.items, found.size});
  }
  if constexpr (Room < largest_derivation_room) {
    return derive_then<Room * 2>(binding, static_cast<Use &&>(use));
  } else {
    // A binding of more than that many reachable types is not derived. The call
    // is not a constant expression, so a derivation at compile time stops here
    // and names this function.
    derivation_has_more_reachable_types_than_the_largest_room();
    return use(reachable_types{storage.items, found.size});
  }
}

// The reachable types a binding has, written into an array of exactly as many.
template <std::size_t Count>
constexpr reachable_type_array<Count>
derive_array(const binding_descriptor &binding) {
  reachable_type_array<Count> array;
  reachable_type_buffer found{array.items, Count};
  derive_into(binding, found);
  return array;
}

namespace detail {
struct count_reachable_types {
  constexpr std::size_t operator()(const reachable_types &found) const {
    return found.size;
  }
};
} // namespace detail

// Service

// Whether a request of the qualifiers of `requested` may refer to the objects
// of `derived`, by the qualification conversions of C++: each object a
// reference or a pointer refers to is a level, whose qualifiers the request
// may add to but not remove, and adds to only below levels that are all
// const. A request by value copies its top-level object, whose qualifiers do
// not count. An array has the qualifiers of its elements, and the arguments of
// a wrapper are part of its type.
constexpr bool qualifies(type_tree_view derived, type_tree_view requested,
                         bool refers) {
  std::size_t index = 0;
  bool level = refers;
  bool above_const = true;
  for (;;) {
    const type_tree_node &d = derived.nodes[index];
    const type_tree_node &r = requested.nodes[index];
    const semantics_descriptor *kind = d.kind;
    if (holds_elements(kind)) {
      ++index;
      continue;
    }
    if (level) {
      if ((r.qualifiers & d.qualifiers) != d.qualifiers) {
        return false;
      }
      if (r.qualifiers != d.qualifiers && !above_const) {
        return false;
      }
      above_const = above_const && (r.qualifiers & const_qualified) != 0;
    }
    // An address, a wrapper of a kind of the address form, refers to a level.
    if (has_operation(kind, operation::address)) {
      ++index;
      level = true;
      continue;
    }
    if (is_wrapper(kind)) {
      const std::size_t end = index + subtree_size(derived, index);
      for (std::size_t inner = index + 1; inner < end; ++inner) {
        if (derived.nodes[inner].qualifiers !=
            requested.nodes[inner].qualifiers) {
          return false;
        }
      }
    }
    return true;
  }
}

constexpr bool accepts(const request_descriptor &request,
                       const reachable_type &candidate, std::uint8_t exposed) {
  if (!type_trees_match(candidate.value, request.value) ||
      !qualifies(candidate.value, request.value,
                 request.kind != request_kind::value)) {
    return false;
  }
  if (request.kind == request_kind::lvalue_reference) {
    return candidate.category == value_category::lvalue &&
           exposes(exposed, request_category::lvalue) &&
           addressable(candidate.value);
  }
  if (has_properties(semantics_of(candidate.value), property::immovable)) {
    return false;
  }
  if (candidate.category == value_category::rvalue) {
    return exposes(exposed, request_category::rvalue);
  }
  return request.kind == request_kind::value &&
         candidate.category == value_category::prvalue;
}

// Whether `left` is preferred to `right`: fewer rules, then no copy, then the
// forms that come first in the order of the rules, which orders forms and
// not kinds; of others, the one derived first is.
constexpr bool preferred(const reachable_type &left,
                         const reachable_type &right) {
  if (left.rule_count != right.rule_count) {
    return left.rule_count < right.rule_count;
  }
  if (left.copied != right.copied) {
    return !left.copied;
  }
  for (std::size_t index = 0; index < left.rule_count; ++index) {
    if (left.rules[index].form != right.rules[index].form) {
      return left.rules[index].form < right.rules[index].form;
    }
  }
  return false;
}

inline constexpr std::size_t no_reachable_type = static_cast<std::size_t>(-1);
inline constexpr std::size_t max_created_wrappers = 2;

// The route a request takes: a reachable type of the binding, or
// no_reachable_type, and the wrappers the request creates of its object,
// innermost first, each by a creating form of the kind of a wrapper of the
// request.
struct route_choice {
  std::size_t reachable_type = no_reachable_type;
  route_step created[max_created_wrappers] = {};
  std::size_t created_count = 0;
};

namespace detail {
// The reachable type the creating form `step` derives from `from`, into `made`;
// false when it derives none.
constexpr bool create(const binding_descriptor &binding,
                      const reachable_type &from, const route_step &step,
                      std::uint8_t exposed, reachable_type &made) {
  const operation_info &rule = info(step.form);
  if (from.rule_count == max_rules_per_route ||
      rule.from_category != from.category ||
      (rule.exposure != 0 && (exposed & rule.exposure) == 0)) {
    return false;
  }
  type_tree bound = from.value;
  if (rule.from_position == type_position::target) {
    // A wrapper of the kind the form leads from, whose element is the one of
    // the created wrapper.
    if (!same_semantics(from_semantics(step), semantics_of(from.value)) ||
        from.value.nodes[0].arity != 1) {
      return false;
    }
    bound = subtree(from.value, 1);
  }
  if (!requirements_hold(binding, rule, bound)) {
    return false;
  }
  type_tree value;
  if (!step_value(from, step, bound, derivation_bound(binding), value)) {
    return false;
  }
  made = extend(from, step, value, bound, 0);
  return true;
}

// The kind of the wrapper a creating form of `step` makes its wrapper of: the
// kind it leads from, or that of the element of the request at `request`.
constexpr const semantics_descriptor *created_inner(const route_step &step,
                                                    type_tree_view request) {
  if (info(step.form).from_position == type_position::target) {
    return from_semantics(step);
  }
  return request.size > 1 ? semantics_of(request, 1) : &leaf_semantics;
}
} // namespace detail

// The route that serves the request, of the reachable types `found` of
// `binding`: a reachable type the request accepts, or one the request makes by
// creating up to max_created_wrappers wrappers, the preferred one; or none.
constexpr route_choice select_route(const binding_descriptor &binding,
                                    const reachable_types &found,
                                    const request_descriptor &request,
                                    std::uint8_t exposed) {
  route_choice best;
  reachable_type best_route;
  auto consider = [&](const reachable_type &candidate,
                      const route_choice &choice) {
    if (accepts(request, candidate, exposed) &&
        (best.reachable_type == no_reachable_type ||
         preferred(candidate, best_route))) {
      best = choice;
      best_route = candidate;
    }
  };
  for (std::size_t index = 0; index < found.size; ++index) {
    if (!found.items[index].sibling) {
      consider(found.items[index], route_choice{index, {}, 0});
    }
  }
  const semantics_descriptor *outer = semantics_of(request.value);
  for (const operation_info &rule : operations) {
    if (!creates(rule) || !has_operation(outer, rule.form)) {
      continue;
    }
    const route_step step{rule.form, outer};
    reachable_type made;
    for (std::size_t index = 0; index < found.size; ++index) {
      if (!found.items[index].sibling &&
          detail::create(binding, found.items[index], step, exposed, made)) {
        consider(made, route_choice{index, {step}, 1});
      }
    }
    // The wrapper the outer one is made of is created too.
    const semantics_descriptor *inner =
        detail::created_inner(step, request.value);
    for (const operation_info &inner_rule : operations) {
      if (!creates(inner_rule) || !has_operation(inner, inner_rule.form)) {
        continue;
      }
      const route_step inner_step{inner_rule.form, inner};
      reachable_type middle;
      for (std::size_t index = 0; index < found.size; ++index) {
        if (!found.items[index].sibling &&
            detail::create(binding, found.items[index], inner_step, exposed,
                           middle) &&
            detail::create(binding, middle, step, exposed, made)) {
          consider(made, route_choice{index, {inner_step, step}, 2});
        }
      }
    }
  }
  return best;
}

// Whether a binding serves the interface a request names: its declared
// interfaces, or the leaves its stored type tree names without them.
constexpr bool serves_interface(const binding_descriptor &binding,
                                type_id requested) {
  if (binding.interface_count == 0) {
    return binding.leaves.contains(requested);
  }
  for (std::size_t index = 0; index < binding.interface_count; ++index) {
    if (binding.interfaces[index].nodes[0].type == requested) {
      return true;
    }
  }
  return false;
}

// Whether a request names the binding, up to the value of their keys, which
// the backend compares: the binding serves its interface with a key of the
// same domain. spec: lookup_filters.binding_filter
constexpr bool has_key_domain(const binding_descriptor &binding,
                              const key_descriptor &key) {
  if (same_key_domain(binding.key, key)) {
    return true;
  }
  for (std::size_t index = 0; index < binding.alias_count; ++index) {
    if (same_key_domain(binding.aliases[index], key)) {
      return true;
    }
  }
  return false;
}

constexpr bool finds(const binding_descriptor &binding,
                     const request_descriptor &request) {
  return serves_interface(binding, innermost_leaf(request.value)) &&
         has_key_domain(binding, request.key);
}

// Whether two bindings serve a common interface with keys of one domain, so
// that equal key values make them candidates of the same requests.
constexpr bool share_domain(const binding_descriptor &left,
                            const binding_descriptor &right) {
  bool shared = has_key_domain(left, right.key);
  for (std::size_t index = 0; index < right.alias_count; ++index) {
    shared = shared || has_key_domain(left, right.aliases[index]);
  }
  if (!shared) {
    return false;
  }
  if (right.interface_count == 0) {
    for (std::size_t index = 0; index < right.leaves.size; ++index) {
      if (serves_interface(left, right.leaves.items[index])) {
        return true;
      }
    }
    return false;
  }
  for (std::size_t index = 0; index < right.interface_count; ++index) {
    if (serves_interface(left, right.interfaces[index].nodes[0].type)) {
      return true;
    }
  }
  return false;
}

// Registration

enum class registration_status : std::uint8_t {
  registered,
  kind_not_stored,
  nested_wrappers,
  no_default_factory,
  interface_in_array,
  no_interface_served
};

// Whether a binding serves one of its interfaces: by reference, or by value
// of a handle of any kind, a nullable wrapper that refers to its element. The
// handles of its kinds and the address of the language stand for them all: a
// handle that adopts its element adopts an address the binding serves. A const
// leaf serves its interfaces as const ones.
constexpr bool serves_an_interface(const binding_descriptor &binding,
                                   const reachable_types &found,
                                   std::uint8_t exposed) {
  semantics_set kinds = semantics_in(binding.stored);
  kinds.add(address_semantics_v);
  constexpr property_class handle{true, property::nullable, property::in_place};
  for (std::size_t index = 0; index < binding.interface_count; ++index) {
    for (const std::uint8_t qualifiers : {std::uint8_t(0), const_qualified}) {
      type_tree interface = copy_of(binding.interfaces[index]);
      interface.nodes[0].qualifiers |= qualifiers;
      if (select_route(binding, found,
                       {request_kind::lvalue_reference, interface, binding.key},
                       exposed)
              .reachable_type != no_reachable_type) {
        return true;
      }
      for (std::size_t kind = 0; kind < kinds.size; ++kind) {
        const semantics_descriptor *of = kinds.items[kind];
        if (!of_class(of, handle)) {
          continue;
        }
        const type_tree wrapped =
            wrap(of, is_mapped(of) ? any_family : nullptr,
                 created_copyable(of, interface), interface);
        const request_descriptor probe{request_kind::value, wrapped,
                                       binding.key};
        if (select_route(binding, found, probe, exposed).reachable_type !=
            no_reachable_type) {
          return true;
        }
      }
    }
  }
  return false;
}

constexpr registration_status
register_status(const binding_descriptor &binding) {
  const scope_info &scope = binding.scope;
  if (exposed_categories(binding) == 0) {
    return registration_status::kind_not_stored;
  }
  if (!scope.nested_wrappers && is_wrapper(binding.stored.nodes[0].kind)) {
    std::size_t arguments = binding.stored.nodes[0].arity;
    for (std::size_t argument = 0; argument < arguments; ++argument) {
      if (is_wrapper(
              binding.stored.nodes[argument_index(binding.stored, 0, argument)]
                  .kind)) {
        return registration_status::nested_wrappers;
      }
    }
  }
  if (!scope.needs_instance && !binding.declared_factory &&
      !constructible(binding.stored)) {
    return registration_status::no_default_factory;
  }
  if (binding.interface_count != 0) {
    if (holds_in_array(binding.stored)) {
      return registration_status::interface_in_array;
    }
    const std::uint8_t exposed = exposed_categories(binding);
    const bool serves =
        derive_then<8>(binding, [&](const reachable_types &found) {
          return serves_an_interface(binding, found, exposed);
        });
    return serves ? registration_status::registered
                  : registration_status::no_interface_served;
  }
  return registration_status::registered;
}

} // namespace dingo
