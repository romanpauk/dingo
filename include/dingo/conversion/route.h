//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

// The route engine of the next implementation: registration, lookup, and
// derivation as constexpr functions over shapes and the route forms. A route
// is a rule of a leaf or a form of a layer kind, and the engine knows a kind
// only by its descriptor, its properties and forms, so a kind of the user is
// derived as one of the library is. It interprets the rules the same way as
// spec/reference/routes.py, which is its oracle. Static containers evaluate it
// at compile time and runtime containers at registration, so both serve a
// request by the same derivation.

#include <dingo/conversion/key.h>
#include <dingo/conversion/layer_traits.h>
#include <dingo/conversion/vocabulary.gen.h>
#include <dingo/primitives/shape.h>

#include <cstddef>
#include <cstdint>

namespace dingo::core {

inline constexpr std::size_t max_interfaces = 8;
inline constexpr std::size_t max_rules_per_route = 8;
inline constexpr std::size_t max_terms = 96;
inline constexpr std::size_t max_kinds = 32;

constexpr const kind_descriptor *kind_of(const shape &value,
                                         std::size_t index = 0) {
  return value.nodes[index].kind;
}

// Whether a kind is of all of the layer_property bits `properties`; the leaf
// kind is of none.
constexpr bool has_properties(const kind_descriptor *kind,
                              std::uint16_t properties) {
  return kind->layer && (kind->properties & properties) == properties;
}

constexpr std::uint32_t form_bit(route_form form) {
  return std::uint32_t(1) << static_cast<unsigned>(form);
}

constexpr bool has_form(const kind_descriptor *kind, route_form form) {
  return (kind->forms & form_bit(form)) != 0;
}

// Whether a kind holds a number of elements: an extent, or one known at
// runtime.
constexpr bool holds_elements(const kind_descriptor *kind) {
  return has_properties(kind, layer_property::extent) ||
         has_properties(kind, layer_property::unsized);
}

// A step of a route: a rule of a leaf, of the leaf kind, or a form of a kind.
struct route_step {
  route_form form = route_form{};
  const kind_descriptor *kind = &leaf_kind;
};

// The kind a step derives from and the one it derives, or the leaf kind for
// an element.
constexpr const kind_descriptor *kind_at(const route_step &step,
                                         form_layer layer) {
  switch (layer) {
  case form_layer::kind:
    return step.kind;
  case form_layer::target:
    if ((step.kind->targeted & form_bit(step.form)) != 0) {
      return step.kind->targets[static_cast<std::size_t>(step.form)];
    }
    return &leaf_kind;
  default:
    return &leaf_kind;
  }
}

constexpr const kind_descriptor *from_kind(const route_step &step) {
  return kind_at(step, info(step.form).from_layer);
}

constexpr const kind_descriptor *to_kind(const route_step &step) {
  return kind_at(step, info(step.form).to_layer);
}

// A set of layer kinds, each with the kinds its forms lead to.
struct kind_set {
  const kind_descriptor *items[max_kinds] = {};
  std::size_t size = 0;

  constexpr void add(const kind_descriptor *kind) {
    if (!kind->layer) {
      return;
    }
    for (std::size_t index = 0; index < size; ++index) {
      if (same_kind(items[index], kind)) {
        return;
      }
    }
    if (size < max_kinds) {
      items[size++] = kind;
      for (std::size_t form = 0; form < max_kind_forms; ++form) {
        if ((kind->targeted & (std::uint32_t(1) << form)) != 0) {
          add(kind->targets[form]);
        }
      }
    }
  }
};

// The kinds of a stored shape and the ones their forms lead to.
constexpr kind_set kinds_of(const shape &stored) {
  kind_set kinds;
  for (std::size_t index = 0; index < stored.size; ++index) {
    kinds.add(stored.nodes[index].kind);
  }
  return kinds;
}

// A binding as the engine sees it: the stored shape, the scope, and the leaf
// shapes of the declared interfaces.
// Whether two keys are of one domain: keys without a value of the same kind,
// or keys with a value of the same type, whether fixed or supplied at
// runtime.
constexpr bool same_key_domain(const key_descriptor &left,
                               const key_descriptor &right) {
  if (has_value(left.kind) != has_value(right.kind)) {
    return false;
  }
  if (!has_value(left.kind) && left.kind != right.kind) {
    return false;
  }
  return left.type == right.type;
}

// A set of leaves, in the order they are added.
struct leaf_set {
  type_id items[shape_capacity] = {};
  std::size_t size = 0;

  constexpr bool contains(type_id leaf) const {
    // Most shapes name one leaf.
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

struct binding_descriptor {
  shape stored;
  // The leaves the stored shape names the binding by without declared
  // interfaces, named_leaves(stored).
  leaf_set leaves = {};
  key_descriptor key;
  // The keys of other domains the binding is also found by, each of a key
  // type of its own.
  key_descriptor aliases[max_key_aliases] = {};
  std::size_t alias_count = 0;
  // The descriptor of the scope of the binding.
  scope_info scope = {};
  shape interfaces[max_interfaces] = {};
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
  shape value;
  key_descriptor key;
};

// A derived term and how it was reached.
struct term {
  shape value;
  value_category category = value_category::lvalue;
  route_step rules[max_rules_per_route] = {};
  // For each rule, the argument of the source layer it reached, or for an
  // upcast, the index of the interface it converted to.
  std::uint8_t arguments[max_rules_per_route] = {};
  std::size_t rule_count = 0;
  bool copied = false;
  // The leaves the copy rule of the route copied.
  std::uint32_t copied_leaves = 0;
  bool consumed = false;
  // Whether the term is a sibling of the term `primary`, the term of its value
  // and category derived first: it takes the route of the primary up to the
  // step `branch`, there another alternative of a variadic layer, and reaches
  // the value on its own. A sibling is neither derived from nor selected; a
  // walk of the primary continues along it from the object of the step when
  // the object holds its alternative.
  bool sibling = false;
  std::uint8_t primary = 0;
  std::uint8_t branch = 0;
  // The layer the last rule started from: its kind, qualifiers, and extent,
  // which a request that copies or decays the term borrows.
  const kind_descriptor *source_kind = &leaf_kind;
  std::uint8_t source_qualifiers = 0;
  std::uint32_t source_extent = 0;
};

struct terms {
  term items[max_terms] = {};
  std::size_t size = 0;
  // A derivation produced more terms than fit; its result is incomplete.
  bool overflow = false;
};

// Storage forms

// Whether the layer at `index` is of a layer class, by the properties of its
// kind.
constexpr bool of_class(const kind_descriptor *kind, const layer_class &of) {
  if (!kind->layer) {
    return false;
  }
  return (kind->properties & of.has) == of.has &&
         (kind->properties & of.lacks) == 0;
}

constexpr bool of_class(const shape &value, std::size_t index,
                        const layer_class &of) {
  return of_class(kind_of(value, index), of);
}

// The most specific storage form of a stored shape, by the classes of its
// outer layers.
constexpr storage_form storage_form_of(const shape &stored) {
  storage_form best = storage_form::plain;
  int depth = 0;
  for (const storage_form_info &form : storage_forms) {
    if (!form.outer.any || !of_class(stored, 0, form.outer)) {
      continue;
    }
    if (!form.inner.any) {
      if (depth < 1) {
        best = form.form;
        depth = 1;
      }
    } else if (stored.nodes[0].arity == 1 && of_class(stored, 1, form.inner)) {
      best = form.form;
      depth = 2;
    }
  }
  return best;
}

constexpr std::uint8_t exposed_categories(const binding_descriptor &binding) {
  return binding.scope
      .exposures[static_cast<std::size_t>(storage_form_of(binding.stored))];
}

// Shape properties

constexpr bool constructible(const shape &value, std::size_t index = 0) {
  const kind_descriptor *kind = kind_of(value, index);
  if (!kind->layer) {
    return value.nodes[index].constructible;
  }
  return has_properties(kind, layer_property::constructible) &&
         constructible(value, index + 1);
}

constexpr std::uint32_t constructed_leaves(const shape &value,
                                           std::size_t index = 0) {
  const kind_descriptor *kind = kind_of(value, index);
  if (!kind->layer) {
    return 1;
  }
  if (has_properties(kind, layer_property::extent)) {
    return value.nodes[index].extent * constructed_leaves(value, index + 1);
  }
  return constructed_leaves(value, index + 1);
}

// The leaves a copy of the shape copies: those its layers hold in place.
constexpr std::uint32_t leaves_held_by_value(const shape &value,
                                             std::size_t index = 0) {
  const kind_descriptor *kind = kind_of(value, index);
  if (!kind->layer) {
    return 1;
  }
  if (!has_properties(kind, layer_property::in_place)) {
    return 0;
  }
  if (has_properties(kind, layer_property::extent)) {
    return value.nodes[index].extent * leaves_held_by_value(value, index + 1);
  }
  return leaves_held_by_value(value, index + 1);
}

// Whether a shape is an object that can be referred to and pointed to:
// anything but an address, a layer of a kind of the address form.
constexpr bool addressable(const shape &value) {
  return !has_form(kind_of(value), route_form::address);
}

constexpr bool holds_in_array(const shape &value) {
  for (std::size_t index = 0; index < value.size; ++index) {
    if (holds_elements(kind_of(value, index))) {
      return true;
    }
  }
  return false;
}

// The leaf at the end of the first-argument chain.
constexpr type_id innermost_leaf(const shape &value) {
  std::size_t index = 0;
  while (is_layer(kind_of(value, index))) {
    ++index;
  }
  return value.nodes[index].type;
}

namespace detail {
constexpr void add_named_leaves(const shape &value, std::size_t index,
                                leaf_set &leaves) {
  const kind_descriptor *kind = kind_of(value, index);
  if (!is_layer(kind)) {
    leaves.add(value.nodes[index].type);
  } else if (!has_properties(kind, layer_property::variadic)) {
    add_named_leaves(value, index + 1, leaves);
  } else {
    for (std::size_t argument = 0; argument < value.nodes[index].arity;
         ++argument) {
      add_named_leaves(value, argument_index(value, index, argument), leaves);
    }
  }
}
} // namespace detail

// The leaves a stored shape names its binding by without declared interfaces:
// the leaf its layers hold, and through a layer of a variadic kind, the leaf of
// each of its alternatives, whichever the object holds.
// spec: lookup_filters.binding_filter
constexpr leaf_set named_leaves(const shape &value) {
  leaf_set leaves;
  detail::add_named_leaves(value, 0, leaves);
  return leaves;
}

// Whether a layer that a route creates can be copied: a layer copied as a
// handle, or one that holds its element in place and is not immovable, when
// the element can be copied.
constexpr bool created_copyable(const kind_descriptor *kind,
                                const shape &argument) {
  if (has_properties(kind, layer_property::copyable)) {
    return true;
  }
  if (has_properties(kind, layer_property::in_place) &&
      !has_properties(kind, layer_property::immovable)) {
    return argument.nodes[0].copyable;
  }
  return false;
}

constexpr bool is_wrapper(const kind_descriptor *kind) {
  return has_properties(kind, layer_property::wrapper);
}

constexpr bool requirements_hold(const binding_descriptor &binding,
                                 const route_form_info &rule,
                                 const shape &bound) {
  auto requires_ = [&](requirement value) {
    return (rule.requires_ & static_cast<std::uint8_t>(value)) != 0;
  };
  if (requires_(requirement::copyable) && !bound.nodes[0].copyable) {
    return false;
  }
  // An array of the language: of an extent, and immovable.
  if (requires_(requirement::array) &&
      !has_properties(kind_of(bound),
                      layer_property::extent | layer_property::immovable)) {
    return false;
  }
  if (requires_(requirement::addressable) && !addressable(bound)) {
    return false;
  }
  if (requires_(requirement::leaf) && is_layer(kind_of(bound))) {
    return false;
  }
  if (rule.form == route_form::copy &&
      binding.scope.rule == exposure_rule::handle_copies &&
      !has_properties(kind_of(bound), layer_property::sharing)) {
    return false;
  }
  return true;
}

// Derivation

namespace detail {
// The term of a value and category that is no sibling, or max_terms.
constexpr std::size_t primary_of(const terms &found, const shape &value,
                                 value_category category) {
  for (std::size_t index = 0; index < found.size; ++index) {
    const term &item = found.items[index];
    if (!item.sibling && item.category == category && item.value == value) {
      return index;
    }
  }
  return max_terms;
}

// How a request completes a term: by copying, decaying, or neither.
constexpr int completion_class(const term &route) {
  if (route.rule_count == 0) {
    return 0;
  }
  const route_form last = route.rules[route.rule_count - 1].form;
  return last == route_form::copy ? 1 : last == route_form::decay ? 2 : 3;
}

// Whether two terms a request completes by copying or decaying borrow alike:
// the layers their last rules start from are of one kind, qualifiers, and
// extent, so a walk to them yields one type.
constexpr bool borrow_alike(const term &left, const term &right) {
  return same_kind(left.source_kind, right.source_kind) &&
         left.source_qualifiers == right.source_qualifiers &&
         left.source_extent == right.source_extent;
}

// Keeps `derived` as a sibling of the term `primary` when their routes part at
// a step into a variadic layer, by its alternatives, and they are completed
// alike; one per step and alternative.
constexpr void add_sibling(terms &found, std::size_t primary, term derived) {
  const term &first = found.items[primary];
  const std::size_t common = first.rule_count < derived.rule_count
                                 ? first.rule_count
                                 : derived.rule_count;
  for (std::size_t position = 0; position < common; ++position) {
    const route_step &taken = first.rules[position];
    const route_step &other = derived.rules[position];
    const bool same =
        taken.form == other.form && same_kind(taken.kind, other.kind);
    if (same && first.arguments[position] == derived.arguments[position]) {
      continue;
    }
    if (!same || !has_properties(from_kind(taken), layer_property::variadic) ||
        completion_class(first) != completion_class(derived) ||
        (completion_class(first) != 3 && !borrow_alike(first, derived))) {
      return;
    }
    for (std::size_t index = 0; index < found.size; ++index) {
      const term &item = found.items[index];
      if (item.sibling && item.primary == primary && item.branch == position &&
          item.arguments[position] == derived.arguments[position]) {
        return;
      }
    }
    if (found.size == max_terms) {
      found.overflow = true;
      return;
    }
    derived.sibling = true;
    derived.primary = static_cast<std::uint8_t>(primary);
    derived.branch = static_cast<std::uint8_t>(position);
    found.items[found.size++] = derived;
    return;
  }
}

// The term `step` derives from `from`, of the value `value`, with the pattern
// variable bound to `bound`.
constexpr term extend(const term &from, const route_step &step,
                      const shape &value, const shape &bound,
                      std::size_t argument) {
  const route_form_info &rule = info(step.form);
  term derived = from;
  derived.source_kind = from.value.nodes[0].kind;
  derived.source_qualifiers = from.value.nodes[0].qualifiers;
  derived.source_extent = from.value.nodes[0].extent;
  derived.value = value;
  derived.category = rule.to_category;
  derived.arguments[derived.rule_count] = static_cast<std::uint8_t>(argument);
  derived.rules[derived.rule_count++] = step;
  if (step.form == route_form::copy) {
    derived.copied = true;
    derived.copied_leaves = leaves_held_by_value(bound);
    // A copy is a new object, without the qualifiers of its source.
    derived.value.nodes[0].qualifiers = 0;
  }
  derived.consumed = from.consumed || rule.rule_access == access::consume;
  return derived;
}

constexpr void add(terms &found, const term &derived) {
  const std::size_t primary =
      primary_of(found, derived.value, derived.category);
  if (primary != max_terms) {
    add_sibling(found, primary, derived);
    return;
  }
  if (found.size == max_terms) {
    found.overflow = true;
    return;
  }
  found.items[found.size++] = derived;
}

// The value a step that is no upcast or conversion derives from `from` with
// the pattern variable bound to `bound`, or false when it is larger than
// `limit`.
constexpr bool step_value(const term &from, const route_step &step,
                          const shape &bound, std::size_t limit, shape &value) {
  const shape &source = from.value;
  const kind_descriptor *to = to_kind(step);
  const kind_descriptor *of = from_kind(step);
  value = bound;
  // The element of a layer that holds it in place, an optional or an
  // alternative, has the qualifiers of the layer.
  if (!is_layer(to) && has_properties(of, layer_property::in_place)) {
    value.nodes[0].qualifiers |= source.nodes[0].qualifiers;
  }
  if (is_layer(to)) {
    type_id family = nullptr;
    if (is_wrapper(to)) {
      bool keeps = is_layer(of) && source.nodes[0].family != nullptr;
      family = keeps ? source.nodes[0].family : any_family;
    }
    value = wrap(to, family, created_copyable(to, bound), bound);
  }
  return value.size <= limit;
}

// Applies `step` to `from` with the pattern variable bound to `bound`, the
// argument at `argument` of the source shape when the step starts from a
// layer.
constexpr void apply(const binding_descriptor &binding, terms &found,
                     const term &from, const route_step &step,
                     const shape &bound, std::size_t argument,
                     std::size_t limit) {
  const route_form_info &rule = info(step.form);
  if (!requirements_hold(binding, rule, bound)) {
    return;
  }
  const shape &source = from.value;
  const kind_descriptor *to = to_kind(step);
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
      shape interface = binding.interfaces[index];
      interface.nodes[0].qualifiers =
          rule.converts ? std::uint8_t(0) : bound.nodes[0].qualifiers;
      shape value = !is_layer(to)
                        ? interface
                        : wrap(to, source.nodes[0].family,
                               source.nodes[0].copyable, interface,
                               source.nodes[0].extent);
      add(found, extend(from, step, value, bound, index));
    }
    return;
  }
  shape value;
  if (step_value(from, step, bound, limit, value)) {
    add(found, extend(from, step, value, bound, argument));
  }
}

// Applies `step`, which starts from the layer `from` is, to each argument of
// it: only a layer of a variadic kind matches any of its arguments.
constexpr void apply_to_arguments(const binding_descriptor &binding,
                                  terms &found, const term &from,
                                  const route_step &step, std::size_t limit) {
  std::size_t arguments =
      has_properties(kind_of(from.value), layer_property::variadic)
          ? from.value.nodes[0].arity
          : 1;
  for (std::size_t argument = 0; argument < arguments; ++argument) {
    shape bound = subtree(from.value, argument_index(from.value, 0, argument));
    apply(binding, found, from, step, bound, argument, limit);
  }
}
} // namespace detail

// Whether a form creates a layer of its kind, from an element or from a layer
// of the kind it leads from: wrapping, adopting, or taking an address. A
// creating form applies at the request, which names the kind it creates.
constexpr bool creates(const route_form_info &rule) {
  return rule.of_kind && rule.to_layer == form_layer::kind &&
         rule.from_layer != form_layer::kind;
}

namespace detail {
// Applies every form that does not create a layer to the term at `next` of
// `found`, in the order of the algebra: a rule of a leaf to the term, and a
// form of the kind of its layer.
constexpr void derive_from(const binding_descriptor &binding, terms &found,
                           std::size_t next, std::uint8_t exposed,
                           std::size_t limit) {
  term from = found.items[next];
  if (from.rule_count == max_rules_per_route) {
    return;
  }
  const kind_descriptor *kind = kind_of(from.value);
  for (const route_form_info &rule : route_forms) {
    if (rule.from_category != from.category ||
        (rule.exposure != 0 && (exposed & rule.exposure) == 0)) {
      continue;
    }
    if (!rule.of_kind) {
      apply(binding, found, from, route_step{rule.form, &leaf_kind}, from.value,
            0, limit);
    } else if (rule.from_layer == form_layer::kind &&
               has_form(kind, rule.form)) {
      apply_to_arguments(binding, found, from, route_step{rule.form, kind},
                         limit);
    }
  }
}
} // namespace detail

// Routes derive shapes at most one layer larger than the stored shape, so a
// binding can publish every term it serves before any request is known.
constexpr std::size_t derivation_bound(const binding_descriptor &binding) {
  return binding.stored.size + 1;
}

// The terms a binding derives by the forms that do not create a layer.
constexpr terms derive(const binding_descriptor &binding) {
  const scope_info &scope = binding.scope;
  terms found;
  term start;
  start.value = binding.stored;
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
  return found;
}

// Service

// Whether a request of the qualifiers of `requested` may refer to the objects
// of `derived`, by the qualification conversions of C++: each object a
// reference or a pointer refers to is a level, whose qualifiers the request
// may add to but not remove, and adds to only below levels that are all
// const. A request by value copies its top-level object, whose qualifiers do
// not count. An array has the qualifiers of its elements, and the arguments of
// a wrapper are part of its type.
constexpr bool qualifies(const shape &derived, const shape &requested,
                         bool refers) {
  std::size_t index = 0;
  bool level = refers;
  bool above_const = true;
  for (;;) {
    const shape_node &d = derived.nodes[index];
    const shape_node &r = requested.nodes[index];
    const kind_descriptor *kind = d.kind;
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
    // An address, a layer of a kind of the address form, refers to a level.
    if (has_form(kind, route_form::address)) {
      ++index;
      level = true;
      continue;
    }
    if (is_layer(kind)) {
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

constexpr bool accepts(const request_descriptor &request, const term &candidate,
                       std::uint8_t exposed) {
  if (!shapes_match(candidate.value, request.value) ||
      !qualifies(candidate.value, request.value,
                 request.kind != request_kind::value)) {
    return false;
  }
  if (request.kind == request_kind::lvalue_reference) {
    return candidate.category == value_category::lvalue &&
           exposes(exposed, request_category::lvalue) &&
           addressable(candidate.value);
  }
  if (has_properties(kind_of(candidate.value), layer_property::immovable)) {
    return false;
  }
  if (candidate.category == value_category::rvalue) {
    return exposes(exposed, request_category::rvalue);
  }
  return request.kind == request_kind::value &&
         candidate.category == value_category::prvalue;
}

// Whether `left` is preferred to `right`: fewer rules, then no copy, then the
// forms that come first in the order of the algebra, which orders forms and
// not kinds; of others, the one derived first is.
constexpr bool preferred(const term &left, const term &right) {
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

inline constexpr std::size_t no_term = static_cast<std::size_t>(-1);
inline constexpr std::size_t max_created_layers = 2;

// The route a request takes: a term of the binding, or no_term, and the
// layers the request creates of its object, innermost first, each by a
// creating form of the kind of a layer of the request.
struct route_choice {
  std::size_t term = no_term;
  route_step created[max_created_layers] = {};
  std::size_t created_count = 0;
};

namespace detail {
// The term the creating form `step` derives from `from`, into `made`; false
// when it derives none.
constexpr bool create(const binding_descriptor &binding, const term &from,
                      const route_step &step, std::uint8_t exposed,
                      term &made) {
  const route_form_info &rule = info(step.form);
  if (from.rule_count == max_rules_per_route ||
      rule.from_category != from.category ||
      (rule.exposure != 0 && (exposed & rule.exposure) == 0)) {
    return false;
  }
  shape bound = from.value;
  if (rule.from_layer == form_layer::target) {
    // A layer of the kind the form leads from, whose element is the one of
    // the created layer.
    if (!same_kind(from_kind(step), kind_of(from.value)) ||
        from.value.nodes[0].arity != 1) {
      return false;
    }
    bound = subtree(from.value, 1);
  }
  if (!requirements_hold(binding, rule, bound)) {
    return false;
  }
  shape value;
  if (!step_value(from, step, bound, derivation_bound(binding), value)) {
    return false;
  }
  made = extend(from, step, value, bound, 0);
  return true;
}

// The kind of the layer a creating form of `step` makes its layer of: the
// kind it leads from, or that of the element of the request at `request`.
constexpr const kind_descriptor *created_inner(const route_step &step,
                                               const shape &request) {
  if (info(step.form).from_layer == form_layer::target) {
    return from_kind(step);
  }
  return request.size > 1 ? kind_of(request, 1) : &leaf_kind;
}
} // namespace detail

// The route that serves the request, of the terms `found` of `binding`: a
// term the request accepts, or one the request makes by creating up to
// max_created_layers layers, the preferred one; or none.
constexpr route_choice select_route(const binding_descriptor &binding,
                                    const terms &found,
                                    const request_descriptor &request,
                                    std::uint8_t exposed) {
  route_choice best;
  term best_route;
  auto consider = [&](const term &candidate, const route_choice &choice) {
    if (accepts(request, candidate, exposed) &&
        (best.term == no_term || preferred(candidate, best_route))) {
      best = choice;
      best_route = candidate;
    }
  };
  for (std::size_t index = 0; index < found.size; ++index) {
    if (!found.items[index].sibling) {
      consider(found.items[index], route_choice{index, {}, 0});
    }
  }
  const kind_descriptor *outer = kind_of(request.value);
  for (const route_form_info &rule : route_forms) {
    if (!creates(rule) || !has_form(outer, rule.form)) {
      continue;
    }
    const route_step step{rule.form, outer};
    term made;
    for (std::size_t index = 0; index < found.size; ++index) {
      if (!found.items[index].sibling &&
          detail::create(binding, found.items[index], step, exposed, made)) {
        consider(made, route_choice{index, {step}, 1});
      }
    }
    // The layer the outer one is made of is created too.
    const kind_descriptor *inner = detail::created_inner(step, request.value);
    for (const route_form_info &inner_rule : route_forms) {
      if (!creates(inner_rule) || !has_form(inner, inner_rule.form)) {
        continue;
      }
      const route_step inner_step{inner_rule.form, inner};
      term middle;
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
// interfaces, or the leaves its stored shape names without them.
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
  form_not_stored,
  nested_layers,
  no_default_factory,
  interface_in_array,
  no_interface_served
};

// Whether a binding serves one of its interfaces: by reference, or by value
// of a handle of any kind, a nullable layer that refers to its element. The
// handles of its kinds and the address of the language stand for them all: a
// handle that adopts its element adopts an address the binding serves. A const
// leaf serves its interfaces as const ones.
constexpr bool serves_an_interface(const binding_descriptor &binding,
                                   const terms &found, std::uint8_t exposed) {
  kind_set kinds = kinds_of(binding.stored);
  kinds.add(address_kind_v);
  constexpr layer_class handle{true, layer_property::nullable,
                               layer_property::in_place};
  for (std::size_t index = 0; index < binding.interface_count; ++index) {
    for (const std::uint8_t qualifiers : {std::uint8_t(0), const_qualified}) {
      shape interface = binding.interfaces[index];
      interface.nodes[0].qualifiers |= qualifiers;
      if (select_route(binding, found,
                       {request_kind::lvalue_reference, interface, binding.key},
                       exposed)
              .term != no_term) {
        return true;
      }
      for (std::size_t kind = 0; kind < kinds.size; ++kind) {
        const kind_descriptor *of = kinds.items[kind];
        if (!of_class(of, handle)) {
          continue;
        }
        const request_descriptor probe{
            request_kind::value,
            wrap(of, is_wrapper(of) ? any_family : nullptr,
                 created_copyable(of, interface), interface),
            binding.key};
        if (select_route(binding, found, probe, exposed).term != no_term) {
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
    return registration_status::form_not_stored;
  }
  if (!scope.nested_layers && is_layer(binding.stored.nodes[0].kind)) {
    std::size_t arguments = binding.stored.nodes[0].arity;
    for (std::size_t argument = 0; argument < arguments; ++argument) {
      if (is_layer(
              binding.stored.nodes[argument_index(binding.stored, 0, argument)]
                  .kind)) {
        return registration_status::nested_layers;
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
    terms found = derive(binding);
    std::uint8_t exposed = exposed_categories(binding);
    if (serves_an_interface(binding, found, exposed)) {
      return registration_status::registered;
    }
    return registration_status::no_interface_served;
  }
  return registration_status::registered;
}

} // namespace dingo::core
