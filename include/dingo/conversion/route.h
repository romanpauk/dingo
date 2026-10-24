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
#include <limits>

namespace dingo {

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

// The kind the form `form` of `kind` leads to, which it has one for: its place
// among the targets of the kind is the number of forms before it that have one.
constexpr const semantics_descriptor *
target_of(const semantics_descriptor *kind, operation form) {
  std::size_t rank = 0;
  for (unsigned bit = 0; bit < static_cast<unsigned>(form); ++bit) {
    if ((kind->targeted & (std::uint32_t(1) << bit)) != 0) {
      ++rank;
    }
  }
  return kind->targets[rank];
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
      return target_of(step.kind, step.form);
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

// A set of wrapper semantics, as a view of the array it is in.
struct semantics_set {
  const semantics_descriptor *const *items = nullptr;
  std::size_t size = 0;
};

// Where the wrapper semantics of a type tree are added, each with the kinds its
// forms lead to. When there is no room left it sets `full`, so the caller adds
// them again with more.
struct semantics_buffer {
  const semantics_descriptor **items = nullptr;
  std::size_t capacity = 0;
  std::size_t size = 0;
  bool full = false;

  constexpr void add(const semantics_descriptor *kind) {
    if (!kind->wrapper) {
      return;
    }
    for (std::size_t index = 0; index < size; ++index) {
      if (same_semantics(items[index], kind)) {
        return;
      }
    }
    if (size == capacity) {
      full = true;
      return;
    }
    items[size++] = kind;
    std::size_t rank = 0;
    for (unsigned form = 0; form < std::numeric_limits<std::uint32_t>::digits;
         ++form) {
      if ((kind->targeted & (std::uint32_t(1) << form)) != 0) {
        add(kind->targets[rank++]);
      }
    }
  }
};

// Adds the wrapper semantics of a stored type tree and of the address of the
// language, and the ones their forms lead to.
constexpr void add_semantics_of(type_tree_view stored,
                                semantics_buffer &kinds) {
  for (std::size_t index = 0; index < stored.size; ++index) {
    kinds.add(stored.nodes[index].kind);
  }
  kinds.add(address_semantics_v);
}

// Whether those semantics fit in room for `Room` of them.
template <std::size_t Room>
constexpr bool semantics_fit(type_tree_view stored) {
  const semantics_descriptor *storage[Room] = {};
  semantics_buffer kinds{storage, Room};
  add_semantics_of(stored, kinds);
  return !kinds.full;
}

// Gives `use` those semantics, built in room for `Room` of them, which they
// must fit in: semantics_fit<Room>(stored).
template <std::size_t Room, typename Use>
constexpr auto semantics_with(type_tree_view stored, Use &&use) {
  const semantics_descriptor *storage[Room] = {};
  semantics_buffer kinds{storage, Room};
  add_semantics_of(stored, kinds);
  return use(semantics_set{storage, kinds.size});
}

// A set of leaves, as a view of the array it is in.
struct leaf_set {
  const type_id *items = nullptr;
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
};

// Where the leaves are added, in the order they are added. A type tree names no
// more leaves than it has nodes, which is the room it needs.
struct leaf_buffer {
  type_id *items = nullptr;
  std::size_t capacity = 0;
  std::size_t size = 0;

  constexpr void add(type_id leaf) {
    if (!leaf_set{items, size}.contains(leaf)) {
      items[size++] = leaf;
    }
  }
};

// An array of exactly `Count` leaves, or of one when there are none.
template <std::size_t Count> struct leaf_array {
  type_id items[Count != 0 ? Count : 1] = {};

  constexpr leaf_set view() const { return {items, Count}; }
};

// An interface a binding declares, and whether its stored leaf declares a user
// conversion to it.
struct declared_interface {
  type_tree_view tree;
  bool converts = false;
};

// The interfaces of a binding, over an array of exactly as many.
struct declared_interfaces {
  const declared_interface *items = nullptr;
  std::size_t size = 0;

  constexpr const declared_interface &operator[](std::size_t index) const {
    return items[index];
  }
};

// The keys of other types of a binding, over an array of exactly as many.
struct key_descriptors {
  const key_descriptor *items = nullptr;
  std::size_t size = 0;

  constexpr const key_descriptor &operator[](std::size_t index) const {
    return items[index];
  }
};

// An array of exactly `Count` interfaces, or of one when there are none.
template <std::size_t Count> struct declared_interface_array {
  declared_interface items[Count != 0 ? Count : 1] = {};

  constexpr declared_interfaces view() const { return {items, Count}; }
};

// An array of exactly `Count` keys, or of one when there are none.
template <std::size_t Count> struct key_descriptor_array {
  key_descriptor items[Count != 0 ? Count : 1] = {};

  constexpr key_descriptors view() const { return {items, Count}; }
};

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
  key_descriptors aliases;
  // The descriptor of the scope of the binding.
  scope_info scope = {};
  declared_interfaces interfaces;
  // Whether the binding declares its factory, so its leaf need not be
  // constructible.
  bool declared_factory = false;
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

// The reachable type of a value and category that is none.
inline constexpr std::size_t no_reachable_type = static_cast<std::size_t>(-1);

// Where the type tree of a stored reachable type is: nodes of the array the
// reachable types of a binding share.
struct tree_span {
  std::uint32_t offset = 0;
  std::uint32_t size = 0;
};

// A derived reachable type and how it was reached: by the last rule of its
// route from the reachable type `parent`, which the route continues. The rules
// of a route are read along the reachable types it continues, by rule_at.
// `Value` is how its type tree is held: a basic_type_tree that has room for
// more nodes, while it is derived, or the span of an array of exactly as many.
template <typename Value> struct basic_reachable_type {
  Value value;
  value_category category = value_category::lvalue;
  // The last rule of the route, and the argument of the source wrapper it
  // reached, or for an upcast, the index of the interface it converted to.
  route_step step = {};
  std::uint32_t argument = 0;
  // The reachable type, among the ones the route is published with, that the
  // route continues, or no_reachable_type for the stored one.
  std::size_t parent = no_reachable_type;
  // The number of rules of the route.
  std::size_t rule_count = 0;
  // Whether the route reaches into a variadic wrapper.
  bool variadic = false;
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
  std::size_t primary = 0;
  std::size_t branch = 0;
  // The wrapper the last rule started from: its kind, qualifiers, and extent,
  // which a request that copies or decays the reachable type borrows.
  const semantics_descriptor *source_semantics = &leaf_semantics;
  std::uint8_t source_qualifiers = 0;
  std::uint32_t source_extent = 0;
};

// A reachable type while it is derived, and as a binding keeps it.
template <std::size_t Nodes>
using reachable_scratch = basic_reachable_type<basic_type_tree<Nodes>>;
struct reachable_type : basic_reachable_type<tree_span> {};

// The reachable types of a binding, as a view of an array that has exactly as
// many as the binding has, and of the array of exactly the nodes of their type
// trees.
struct reachable_types {
  const reachable_type *items = nullptr;
  std::size_t size = 0;
  const type_tree_node *nodes = nullptr;

  constexpr type_tree_view tree(const reachable_type &item) const {
    return {nodes + item.value.offset, item.value.size};
  }

  constexpr type_tree_view tree(std::size_t index) const {
    return tree(items[index]);
  }
};

namespace detail {
// The reachable type that holds the rule of `route` at `position`: the route
// itself for its last rule, and the ones it continues before it.
template <typename Item>
constexpr const Item &holder_of_rule(const Item *items, const Item &route,
                                     std::size_t position) {
  const Item *holder = &route;
  for (std::size_t hops = route.rule_count - 1 - position; hops > 0; --hops) {
    holder = &items[holder->parent];
  }
  return *holder;
}
} // namespace detail

// The rule of `route` at `position`, and its argument, where `items` are the
// reachable types the route is published with.
template <typename Item>
constexpr const route_step &rule_at(const Item *items, const Item &route,
                                    std::size_t position) {
  return detail::holder_of_rule(items, route, position).step;
}

template <typename Item>
constexpr std::size_t argument_at(const Item *items, const Item &route,
                                  std::size_t position) {
  return detail::holder_of_rule(items, route, position).argument;
}

// Where a derivation writes the reachable types it finds. When there is no room
// left it sets `full` and writes no more, so the caller derives again with
// more.
template <std::size_t Nodes> struct reachable_type_buffer {
  reachable_scratch<Nodes> *items = nullptr;
  std::size_t capacity = 0;
  std::size_t size = 0;
  bool full = false;
};

// An array of exactly `Count` reachable types and `Nodes` nodes of their type
// trees.
template <std::size_t Count, std::size_t Nodes> struct reachable_table {
  reachable_type items[Count] = {};
  type_tree_node nodes[Nodes] = {};

  constexpr reachable_types view() const { return {items, Count, nodes}; }
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
                                leaf_buffer &leaves) {
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
constexpr void named_leaves_into(type_tree_view value, leaf_buffer &leaves) {
  detail::add_named_leaves(value, 0, leaves);
}

// The number of leaves a stored type tree of `Nodes` nodes names its binding
// by, which is room enough for them.
template <std::size_t Nodes>
constexpr std::size_t named_leaf_count(type_tree_view value) {
  type_id storage[Nodes] = {};
  leaf_buffer leaves{storage, Nodes};
  named_leaves_into(value, leaves);
  return leaves.size;
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

template <std::size_t Nodes>
constexpr std::size_t primary_of(const reachable_type_buffer<Nodes> &found,
                                 type_tree_view value,
                                 value_category category) {
  for (std::size_t index = 0; index < found.size; ++index) {
    const reachable_scratch<Nodes> &item = found.items[index];
    if (!item.sibling && item.category == category && item.value == value) {
      return index;
    }
  }
  return no_primary;
}

// How a request completes a reachable type: by copying, decaying, or neither.
template <typename Item> constexpr int completion_class(const Item &route) {
  if (route.rule_count == 0) {
    return 0;
  }
  const operation last = route.step.form;
  return last == operation::copy ? 1 : last == operation::decay ? 2 : 3;
}

// Whether two reachable types a request completes by copying or decaying borrow
// alike: the wrappers their last rules start from are of one kind, qualifiers,
// and extent, so a walk to them yields one type.
template <typename Item>
constexpr bool borrow_alike(const Item &left, const Item &right) {
  return same_semantics(left.source_semantics, right.source_semantics) &&
         left.source_qualifiers == right.source_qualifiers &&
         left.source_extent == right.source_extent;
}

// Keeps `derived` as a sibling of the reachable type `primary` when their
// routes part at a step into a variadic wrapper, by its alternatives, and they
// are completed alike; one per step and alternative.
template <std::size_t Nodes>
constexpr void add_sibling(reachable_type_buffer<Nodes> &found,
                           std::size_t primary,
                           reachable_scratch<Nodes> derived) {
  const reachable_scratch<Nodes> &first = found.items[primary];
  const std::size_t common = first.rule_count < derived.rule_count
                                 ? first.rule_count
                                 : derived.rule_count;
  for (std::size_t position = 0; position < common; ++position) {
    const route_step &taken = rule_at(found.items, first, position);
    const route_step &other = rule_at(found.items, derived, position);
    const std::size_t taken_argument =
        argument_at(found.items, first, position);
    const std::size_t other_argument =
        argument_at(found.items, derived, position);
    const bool same =
        taken.form == other.form && same_semantics(taken.kind, other.kind);
    if (same && taken_argument == other_argument) {
      continue;
    }
    if (!same || !has_properties(from_semantics(taken), property::variadic) ||
        completion_class(first) != completion_class(derived) ||
        (completion_class(first) != 3 && !borrow_alike(first, derived))) {
      return;
    }
    for (std::size_t index = 0; index < found.size; ++index) {
      const reachable_scratch<Nodes> &item = found.items[index];
      if (item.sibling && item.primary == primary && item.branch == position &&
          argument_at(found.items, item, position) == other_argument) {
        return;
      }
    }
    if (found.size == found.capacity) {
      found.full = true;
      return;
    }
    derived.sibling = true;
    derived.primary = primary;
    derived.branch = position;
    found.items[found.size++] = derived;
    return;
  }
}

// The reachable type `step` derives from `from`, of the value `value`, with the
// pattern variable bound to `bound`.
template <std::size_t Nodes>
constexpr reachable_scratch<Nodes>
extend(const reachable_scratch<Nodes> &from, std::size_t parent,
       const route_step &step, type_tree_view value, type_tree_view bound,
       std::size_t argument) {
  const operation_info &rule = info(step.form);
  reachable_scratch<Nodes> derived = from;
  derived.source_semantics = from.value.nodes[0].kind;
  derived.source_qualifiers = from.value.nodes[0].qualifiers;
  derived.source_extent = from.value.nodes[0].extent;
  derived.value = copy_of<Nodes>(value);
  derived.category = rule.to_category;
  derived.step = step;
  derived.argument = static_cast<std::uint32_t>(argument);
  derived.parent = parent;
  ++derived.rule_count;
  derived.variadic =
      from.variadic || has_properties(from_semantics(step), property::variadic);
  if (step.form == operation::copy) {
    derived.copied = true;
    derived.copied_leaves = leaves_held_by_value(bound);
    // A copy is a new object, without the qualifiers of its source.
    derived.value.nodes[0].qualifiers = 0;
  }
  derived.consumed = from.consumed || rule.rule_access == access::consume;
  return derived;
}

template <std::size_t Nodes>
constexpr void add(reachable_type_buffer<Nodes> &found,
                   const reachable_scratch<Nodes> &derived) {
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
template <std::size_t Nodes>
constexpr bool step_value(const reachable_scratch<Nodes> &from,
                          const route_step &step, type_tree_view bound,
                          std::size_t limit, basic_type_tree<Nodes> &value) {
  const basic_type_tree<Nodes> &source = from.value;
  const semantics_descriptor *to = to_semantics(step);
  const semantics_descriptor *of = from_semantics(step);
  value = copy_of<Nodes>(bound);
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
    value = wrap<Nodes>(to, family, created_copyable(to, bound), bound);
  }
  return value.size <= limit;
}

// Applies `step` to `from` with the pattern variable bound to `bound`, the
// argument at `argument` of the source type tree when the step starts from a
// wrapper.
template <std::size_t Nodes>
constexpr void apply(const binding_descriptor &binding,
                     reachable_type_buffer<Nodes> &found,
                     const reachable_scratch<Nodes> &from, std::size_t parent,
                     const route_step &step, type_tree_view bound,
                     std::size_t argument, std::size_t limit) {
  const operation_info &rule = info(step.form);
  if (!requirements_hold(binding, rule, bound)) {
    return;
  }
  const basic_type_tree<Nodes> &source = from.value;
  const semantics_descriptor *to = to_semantics(step);
  if (rule.upcast || rule.converts) {
    // Only the stored leaf converts to the declared interfaces, and a user
    // conversion only to the ones it declares one to.
    if (bound.nodes[0].type != innermost_leaf(binding.stored)) {
      return;
    }
    for (std::size_t index = 0; index < binding.interfaces.size; ++index) {
      if (rule.converts && !binding.interfaces[index].converts) {
        continue;
      }
      // An upcast keeps the qualifiers of the leaf; a conversion makes a new
      // object.
      basic_type_tree<Nodes> interface =
          copy_of<Nodes>(binding.interfaces[index].tree);
      interface.nodes[0].qualifiers =
          rule.converts ? std::uint8_t(0) : bound.nodes[0].qualifiers;
      basic_type_tree<Nodes> value =
          !is_wrapper(to) ? interface
                          : wrap<Nodes>(to, source.nodes[0].family,
                                        source.nodes[0].copyable, interface,
                                        source.nodes[0].extent);
      add(found, extend(from, parent, step, value, bound, index));
    }
    return;
  }
  basic_type_tree<Nodes> value;
  if (step_value(from, step, bound, limit, value)) {
    add(found, extend(from, parent, step, value, bound, argument));
  }
}

// Applies `step`, which starts from the wrapper `from` is, to each argument of
// it: only a wrapper of a variadic kind matches any of its arguments.
template <std::size_t Nodes>
constexpr void apply_to_arguments(const binding_descriptor &binding,
                                  reachable_type_buffer<Nodes> &found,
                                  const reachable_scratch<Nodes> &from,
                                  std::size_t parent, const route_step &step,
                                  std::size_t limit) {
  std::size_t arguments =
      has_properties(semantics_of(from.value), property::variadic)
          ? from.value.nodes[0].arity
          : 1;
  for (std::size_t argument = 0; argument < arguments; ++argument) {
    basic_type_tree<Nodes> bound =
        subtree<Nodes>(from.value, argument_index(from.value, 0, argument));
    apply(binding, found, from, parent, step, bound, argument, limit);
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
template <std::size_t Nodes>
constexpr void derive_from(const binding_descriptor &binding,
                           reachable_type_buffer<Nodes> &found,
                           std::size_t next, std::uint8_t exposed,
                           std::size_t limit) {
  reachable_scratch<Nodes> from = found.items[next];
  const semantics_descriptor *kind = semantics_of(from.value);
  for (const operation_info &rule : operations) {
    if (rule.from_category != from.category ||
        (rule.exposure != 0 && (exposed & rule.exposure) == 0)) {
      continue;
    }
    if (!rule.of_semantics) {
      apply(binding, found, from, next, route_step{rule.form, &leaf_semantics},
            from.value, 0, limit);
    } else if (rule.from_position == type_position::wrapper &&
               has_operation(kind, rule.form)) {
      apply_to_arguments(binding, found, from, next,
                         route_step{rule.form, kind}, limit);
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

// The nodes the largest type tree a derivation of the binding builds has: the
// stored one or a declared interface, with the wrapper a rule applies to it,
// and the wrapper a rule that is then refused for being larger than the bound
// builds. A derivation sized by it never builds a type tree that does not fit.
constexpr std::size_t scratch_nodes(const binding_descriptor &binding) {
  std::size_t largest = binding.stored.size;
  for (std::size_t index = 0; index < binding.interfaces.size; ++index) {
    if (binding.interfaces[index].tree.size > largest) {
      largest = binding.interfaces[index].tree.size;
    }
  }
  return largest + 2;
}

// Derives the reachable types a binding has by the forms that do not create a
// wrapper into `found`, which stops taking them when it is full.
template <std::size_t Nodes>
constexpr void derive_into(const binding_descriptor &binding,
                           reachable_type_buffer<Nodes> &found) {
  const scope_info &scope = binding.scope;
  reachable_scratch<Nodes> start;
  start.value = copy_of<Nodes>(binding.stored);
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
  std::size_t wanted = binding.stored.size * (binding.interfaces.size + 1) * 2;
  std::size_t room = 8;
  while (room < wanted) {
    room *= 2;
  }
  return room;
}

// The reachable type `item` as a binding keeps it, with its type tree at
// `offset` of the nodes `nodes`.
template <std::size_t Nodes>
constexpr reachable_type stored_from(const reachable_scratch<Nodes> &item,
                                     type_tree_node *nodes,
                                     std::uint32_t offset) {
  reachable_type stored;
  stored.value = {offset, static_cast<std::uint32_t>(item.value.size)};
  for (std::size_t index = 0; index < item.value.size; ++index) {
    nodes[offset + index] = item.value.nodes[index];
  }
  stored.category = item.category;
  stored.step = item.step;
  stored.argument = item.argument;
  stored.parent = item.parent;
  stored.rule_count = item.rule_count;
  stored.variadic = item.variadic;
  stored.copied = item.copied;
  stored.copied_leaves = item.copied_leaves;
  stored.consumed = item.consumed;
  stored.sibling = item.sibling;
  stored.primary = item.primary;
  stored.branch = item.branch;
  stored.source_semantics = item.source_semantics;
  stored.source_qualifiers = item.source_qualifiers;
  stored.source_extent = item.source_extent;
  return stored;
}

// How many reachable types a derivation of a binding finds, how many nodes
// their type trees have, and whether they fit in the room it was given.
struct reachable_sizes {
  std::size_t items = 0;
  std::size_t nodes = 0;
  bool fits = true;
};

// Derives the reachable types of a binding in room for `Room` of them, whose
// type trees have room for `Nodes` nodes, which scratch_nodes(binding) gives.
template <std::size_t Room, std::size_t Nodes>
constexpr reachable_sizes derive_sizes(const binding_descriptor &binding) {
  reachable_scratch<Nodes> scratch[Room] = {};
  reachable_type_buffer<Nodes> found{scratch, Room};
  derive_into(binding, found);
  reachable_sizes sizes;
  sizes.fits = !found.full;
  sizes.items = found.size;
  for (std::size_t index = 0; index < found.size; ++index) {
    sizes.nodes += scratch[index].value.size;
  }
  return sizes;
}

// Derives the reachable types of a binding in room for `Room` of them, which
// they must fit in: derive_sizes<Room, Nodes>(binding).fits. It gives them to
// `use`, a callable of a `reachable_types`, and returns what it returns.
template <std::size_t Room, std::size_t Nodes, typename Use>
constexpr auto derive_with(const binding_descriptor &binding, Use &&use) {
  reachable_scratch<Nodes> scratch[Room] = {};
  reachable_type_buffer<Nodes> found{scratch, Room};
  derive_into(binding, found);
  reachable_type items[Room] = {};
  type_tree_node nodes[Room * Nodes] = {};
  std::uint32_t used = 0;
  for (std::size_t index = 0; index < found.size; ++index) {
    items[index] = stored_from(scratch[index], nodes, used);
    used += items[index].value.size;
  }
  return use(reachable_types{items, found.size, nodes});
}

namespace detail {
// Copies the reachable types into a table of exactly `Count` of them and
// `Nodes` nodes.
template <std::size_t Count, std::size_t Nodes> struct copy_reachable_types {
  constexpr reachable_table<Count, Nodes>
  operator()(const reachable_types &found) const {
    reachable_table<Count, Nodes> table;
    for (std::size_t index = 0; index < Count; ++index) {
      table.items[index] = found.items[index];
    }
    for (std::size_t index = 0; index < Nodes; ++index) {
      table.nodes[index] = found.nodes[index];
    }
    return table;
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

constexpr bool accepts(const request_descriptor &request, type_tree_view value,
                       value_category category, std::uint8_t exposed) {
  if (!type_trees_match(value, request.value) ||
      !qualifies(value, request.value, request.kind != request_kind::value)) {
    return false;
  }
  if (request.kind == request_kind::lvalue_reference) {
    return category == value_category::lvalue &&
           exposes(exposed, request_category::lvalue) && addressable(value);
  }
  if (has_properties(semantics_of(value), property::immovable)) {
    return false;
  }
  if (category == value_category::rvalue) {
    return exposes(exposed, request_category::rvalue);
  }
  return request.kind == request_kind::value &&
         category == value_category::prvalue;
}

template <typename Item>
constexpr bool accepts(const request_descriptor &request, const Item &candidate,
                       std::uint8_t exposed) {
  return accepts(request, type_tree_view(candidate.value), candidate.category,
                 exposed);
}

// The most steps a request creates wrappers by for each node of its type tree:
// the one that makes the wrapper of the node, and one for each form that makes
// a wrapper from a wrapper of another kind, since a chain of steps uses a form
// once for a node.
constexpr std::size_t creation_steps_per_node() {
  std::size_t steps = 1;
  for (const operation_info &rule : operations) {
    if (creates(rule) && rule.from_position == type_position::target) {
      ++steps;
    }
  }
  return steps;
}

// The most wrappers a request of `Nodes` nodes creates.
constexpr std::size_t creation_chain(std::size_t nodes) {
  return nodes * creation_steps_per_node();
}

// A route a request takes to serve itself: the reachable type of the binding
// it starts from, `base`, and the wrappers the request creates of it, which
// `reachable` is the result of.
template <std::size_t Nodes> struct route_candidate {
  reachable_scratch<Nodes> reachable;
  std::size_t base = no_reachable_type;
  route_step created[creation_chain(Nodes)] = {};
  std::size_t created_count = 0;
};

// The form of the rule of a candidate at `position`: one of the route of the
// reachable type it starts from, or of a wrapper it creates.
template <std::size_t Nodes>
constexpr operation form_at(const reachable_type *items,
                            const route_candidate<Nodes> &candidate,
                            std::size_t position) {
  const reachable_type &base = items[candidate.base];
  return position < base.rule_count
             ? rule_at(items, base, position).form
             : candidate.created[position - base.rule_count].form;
}

// Whether `left` is preferred to `right`: fewer rules, then no copy, then the
// forms that come first in the order of the rules, which orders forms and
// not kinds; of others, the one derived first is.
template <std::size_t Nodes>
constexpr bool preferred(const reachable_type *items,
                         const route_candidate<Nodes> &left,
                         const route_candidate<Nodes> &right) {
  if (left.reachable.rule_count != right.reachable.rule_count) {
    return left.reachable.rule_count < right.reachable.rule_count;
  }
  if (left.reachable.copied != right.reachable.copied) {
    return !left.reachable.copied;
  }
  for (std::size_t index = 0; index < left.reachable.rule_count; ++index) {
    const operation first = form_at(items, left, index);
    const operation second = form_at(items, right, index);
    if (first != second) {
      return first < second;
    }
  }
  return false;
}

// The route a request takes: a reachable type of the binding, or
// no_reachable_type, and the wrappers the request creates of its object,
// innermost first, each by a creating form of the kind of a wrapper of the
// request.
template <std::size_t Chain> struct route_choice {
  std::size_t reachable_type = no_reachable_type;
  route_step created[Chain != 0 ? Chain : 1] = {};
  std::size_t created_count = 0;
};

// The route a request of `Nodes` nodes takes.
template <std::size_t Nodes>
using route_choice_of = route_choice<creation_chain(Nodes)>;

namespace detail {
// The reachable type the creating form `step` derives from `from`, into `made`;
// false when it derives none.
template <std::size_t Nodes>
constexpr bool create(const binding_descriptor &binding,
                      const route_candidate<Nodes> &candidate,
                      const route_step &step, std::uint8_t exposed,
                      route_candidate<Nodes> &made) {
  const reachable_scratch<Nodes> &from = candidate.reachable;
  const operation_info &rule = info(step.form);
  if (rule.from_category != from.category ||
      (rule.exposure != 0 && (exposed & rule.exposure) == 0)) {
    return false;
  }
  basic_type_tree<Nodes> bound = from.value;
  if (rule.from_position == type_position::target) {
    // A wrapper of the kind the form leads from, whose element is the one of
    // the created wrapper.
    if (!same_semantics(from_semantics(step), semantics_of(from.value)) ||
        from.value.nodes[0].arity != 1) {
      return false;
    }
    bound = subtree<Nodes>(from.value, 1);
  }
  if (!requirements_hold(binding, rule, bound)) {
    return false;
  }
  // The wrapper the form creates makes the tree one node larger than the
  // element, and a tree larger than the request is not accepted.
  if (bound.size + 1 > Nodes) {
    return false;
  }
  basic_type_tree<Nodes> value;
  if (!step_value(from, step, bound, derivation_bound(binding), value)) {
    return false;
  }
  made = candidate;
  made.reachable = extend(from, from.parent, step, value, bound, 0);
  made.created[made.created_count++] = step;
  return true;
}

// The candidate that starts from the reachable type at `index` and creates no
// wrapper.
template <std::size_t Nodes>
constexpr route_candidate<Nodes> candidate_from(const reachable_types &found,
                                                std::size_t index) {
  const reachable_type &item = found.items[index];
  route_candidate<Nodes> candidate;
  candidate.base = index;
  candidate.reachable.value = copy_of<Nodes>(found.tree(item));
  candidate.reachable.category = item.category;
  candidate.reachable.step = item.step;
  candidate.reachable.argument = item.argument;
  candidate.reachable.parent = item.parent;
  candidate.reachable.rule_count = item.rule_count;
  candidate.reachable.variadic = item.variadic;
  candidate.reachable.copied = item.copied;
  candidate.reachable.copied_leaves = item.copied_leaves;
  candidate.reachable.consumed = item.consumed;
  candidate.reachable.sibling = item.sibling;
  candidate.reachable.primary = item.primary;
  candidate.reachable.branch = item.branch;
  candidate.reachable.source_semantics = item.source_semantics;
  candidate.reachable.source_qualifiers = item.source_qualifiers;
  candidate.reachable.source_extent = item.source_extent;
  return candidate;
}

// What a request creates next: the wrapper it makes the one above of, `outer`,
// and the node of the request that holds its element.
struct creation_goal {
  const semantics_descriptor *outer = &leaf_semantics;
  std::size_t element = 0;
};

// The goal under the creating form `step` of the wrapper of `goal`: the wrapper
// it leads from over the element of the goal, or the element of the goal.
constexpr creation_goal goal_under(const route_step &step,
                                   type_tree_view request,
                                   const creation_goal &goal) {
  if (info(step.form).from_position == type_position::target) {
    return {from_semantics(step), goal.element};
  }
  if (goal.element < request.size) {
    return {semantics_of(request, goal.element), goal.element + 1};
  }
  return {};
}

// What a search for the routes of a request that create wrappers is given: the
// binding, its reachable types, the request, and the categories its scope
// exposes.
struct creation_search {
  const binding_descriptor &binding;
  const reachable_types &found;
  type_tree_view request;
  std::uint8_t exposed;
};

// Offers `consider` the routes that make the request of the binding by the
// creating forms of `chain`, outermost first, applied to each reachable type of
// the binding.
template <std::size_t Nodes, typename Consider>
constexpr void offer_chain(const creation_search &search,
                           const route_step *chain, std::size_t length,
                           Consider &consider) {
  for (std::size_t index = 0; index < search.found.size; ++index) {
    // A reachable type larger than the request is neither accepted nor made
    // into one by creating a wrapper.
    if (search.found.items[index].sibling ||
        search.found.items[index].value.size > Nodes) {
      continue;
    }
    route_candidate<Nodes> made = candidate_from<Nodes>(search.found, index);
    bool created = true;
    for (std::size_t position = length; created && position > 0; --position) {
      const route_candidate<Nodes> from = made;
      created = create(search.binding, from, chain[position - 1],
                       search.exposed, made);
    }
    if (created) {
      consider(made);
    }
  }
}

// Offers `consider` the routes of `chain`, and of each chain that creates one
// wrapper more, as many as the request holds. A form is used once for a node of
// the request, which bounds the chain by creation_chain(Nodes) and ends a chain
// of forms that lead to each other.
template <std::size_t Nodes, typename Consider>
constexpr void create_by_chain(const creation_search &search, route_step *chain,
                               std::size_t length, const creation_goal &goal,
                               std::uint32_t used, Consider &consider) {
  if (length > 0) {
    offer_chain<Nodes>(search, chain, length, consider);
  }
  for (const operation_info &rule : operations) {
    if (!creates(rule) || !has_operation(goal.outer, rule.form) ||
        (used & operation_bit(rule.form)) != 0) {
      continue;
    }
    chain[length] = route_step{rule.form, goal.outer};
    const creation_goal next = goal_under(chain[length], search.request, goal);
    create_by_chain<Nodes>(
        search, chain, length + 1, next,
        next.element == goal.element ? used | operation_bit(rule.form) : 0,
        consider);
  }
}
} // namespace detail

// The route that serves the request, of the reachable types `found` of
// `binding`: a reachable type the request accepts, or one the request makes by
// creating wrappers, the preferred one; or none. The type trees it builds have
// room for `Nodes` nodes, the nodes of the request or more.
template <std::size_t Nodes>
constexpr route_choice_of<Nodes>
select_route(const binding_descriptor &binding, const reachable_types &found,
             const request_descriptor &request, std::uint8_t exposed) {
  route_choice_of<Nodes> best;
  route_candidate<Nodes> best_route;
  auto consider = [&](const route_candidate<Nodes> &candidate) {
    if (accepts(request, candidate.reachable, exposed) &&
        (best.reachable_type == no_reachable_type ||
         preferred(found.items, candidate, best_route))) {
      best.reachable_type = candidate.base;
      best.created_count = candidate.created_count;
      for (std::size_t index = 0; index < candidate.created_count; ++index) {
        best.created[index] = candidate.created[index];
      }
      best_route = candidate;
    }
  };
  for (std::size_t index = 0; index < found.size; ++index) {
    // A candidate is made of the reachable types the request accepts only.
    const reachable_type &item = found.items[index];
    if (!item.sibling && item.value.size <= Nodes &&
        accepts(request, found.tree(item), item.category, exposed)) {
      consider(detail::candidate_from<Nodes>(found, index));
    }
  }
  route_step chain[creation_chain(Nodes)] = {};
  const detail::creation_search search{binding, found, request.value, exposed};
  detail::create_by_chain<Nodes>(
      search, chain, 0, detail::creation_goal{semantics_of(request.value), 1},
      0, consider);
  return best;
}

// Calls `visit` with each type id a binding serves a request by: its declared
// interfaces, or the leaves its stored type tree names without them.
template <typename Visit>
constexpr void for_each_served(const binding_descriptor &binding,
                               Visit &&visit) {
  if (binding.interfaces.size == 0) {
    for (std::size_t index = 0; index < binding.leaves.size; ++index) {
      visit(binding.leaves.items[index]);
    }
    return;
  }
  for (std::size_t index = 0; index < binding.interfaces.size; ++index) {
    visit(binding.interfaces[index].tree.nodes[0].type);
  }
}

// Whether a binding serves the interface a request names: its declared
// interfaces, or the leaves its stored type tree names without them.
constexpr bool serves_interface(const binding_descriptor &binding,
                                type_id requested) {
  if (binding.interfaces.size == 0) {
    return binding.leaves.contains(requested);
  }
  for (std::size_t index = 0; index < binding.interfaces.size; ++index) {
    if (binding.interfaces[index].tree.nodes[0].type == requested) {
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
  for (std::size_t index = 0; index < binding.aliases.size; ++index) {
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
  for (std::size_t index = 0; index < right.aliases.size; ++index) {
    shared = shared || has_key_domain(left, right.aliases[index]);
  }
  if (!shared) {
    return false;
  }
  if (right.interfaces.size == 0) {
    for (std::size_t index = 0; index < right.leaves.size; ++index) {
      if (serves_interface(left, right.leaves.items[index])) {
        return true;
      }
    }
    return false;
  }
  for (std::size_t index = 0; index < right.interfaces.size; ++index) {
    if (serves_interface(left, right.interfaces[index].tree.nodes[0].type)) {
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
namespace detail {
// Whether the binding serves a handle of one of `kinds` to `interface`, by
// value.
template <std::size_t Nodes>
constexpr bool serves_by_handle(const binding_descriptor &binding,
                                const reachable_types &found,
                                std::uint8_t exposed,
                                const semantics_set &kinds,
                                const basic_type_tree<Nodes> &interface) {
  constexpr property_class handle{true, property::nullable, property::in_place};
  for (std::size_t kind = 0; kind < kinds.size; ++kind) {
    const semantics_descriptor *of = kinds.items[kind];
    if (!of_class(of, handle)) {
      continue;
    }
    const basic_type_tree<Nodes> wrapped =
        wrap<Nodes>(of, is_mapped(of) ? any_family : nullptr,
                    created_copyable(of, interface), interface);
    const request_descriptor probe{request_kind::value, wrapped, binding.key};
    if (select_route<Nodes>(binding, found, probe, exposed).reachable_type !=
        no_reachable_type) {
      return true;
    }
  }
  return false;
}
} // namespace detail

template <std::size_t Nodes, std::size_t Kinds>
constexpr bool serves_an_interface(const binding_descriptor &binding,
                                   const reachable_types &found,
                                   std::uint8_t exposed) {
  return semantics_with<Kinds>(binding.stored, [&](const semantics_set &kinds) {
    for (std::size_t index = 0; index < binding.interfaces.size; ++index) {
      for (const std::uint8_t qualifiers : {std::uint8_t(0), const_qualified}) {
        basic_type_tree<Nodes> interface =
            copy_of<Nodes>(binding.interfaces[index].tree);
        interface.nodes[0].qualifiers |= qualifiers;
        const request_descriptor reference{request_kind::lvalue_reference,
                                           interface, binding.key};
        if (select_route<Nodes>(binding, found, reference, exposed)
                    .reachable_type != no_reachable_type ||
            detail::serves_by_handle<Nodes>(binding, found, exposed, kinds,
                                            interface)) {
          return true;
        }
      }
    }
    return false;
  });
}

// Whether a binding registers, with the type trees of its derivation having
// room for `Nodes` nodes, which scratch_nodes(binding) gives, and the reachable
// types and the wrapper semantics in room for `Room` and `Kinds` of them, which
// they fit in.
template <std::size_t Nodes, std::size_t Room, std::size_t Kinds>
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
  if (binding.interfaces.size != 0) {
    if (holds_in_array(binding.stored)) {
      return registration_status::interface_in_array;
    }
    const std::uint8_t exposed = exposed_categories(binding);
    const bool serves =
        derive_with<Room, Nodes>(binding, [&](const reachable_types &found) {
          return serves_an_interface<Nodes, Kinds>(binding, found, exposed);
        });
    return serves ? registration_status::registered
                  : registration_status::no_interface_served;
  }
  return registration_status::registered;
}

} // namespace dingo
