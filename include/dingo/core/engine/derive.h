//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

// The route engine of the next implementation: registration, lookup, and
// derivation as constexpr functions over shapes and the generated rule table.
// It interprets the rules the same way as spec/reference/routes.py, which is
// its oracle. Static containers evaluate it at compile time and runtime
// containers at registration, so both serve a request by the same derivation.

#include <dingo/core/primitives/shape.h>
#include <dingo/core/projection/key.h>
#include <dingo/core/vocabulary/vocabulary.gen.h>

#include <cstddef>
#include <cstdint>

namespace dingo::core {

inline constexpr std::size_t max_interfaces = 8;
inline constexpr std::size_t max_rules_per_route = 8;
inline constexpr std::size_t max_terms = 96;

constexpr layer_kind kind_of(const shape &value, std::size_t index = 0) {
  return static_cast<layer_kind>(value.nodes[index].kind);
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

inline constexpr std::size_t max_key_aliases = 4;

struct binding_descriptor {
  shape stored;
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
  rule_id rules[max_rules_per_route] = {};
  // For each rule, the argument of the source layer it reached, or for an
  // upcast, the index of the interface it converted to.
  std::uint8_t arguments[max_rules_per_route] = {};
  std::size_t rule_count = 0;
  bool copied = false;
  // The leaves the copy rule of the route copied.
  std::uint32_t copied_leaves = 0;
  bool consumed = false;
};

struct terms {
  term items[max_terms] = {};
  std::size_t size = 0;
  // A derivation produced more terms than fit; its result is incomplete.
  bool overflow = false;
};

// Storage forms

constexpr storage_form storage_form_of(const shape &stored) {
  storage_form best = storage_form::plain;
  int depth = 0;
  for (const storage_form_info &form : storage_forms) {
    if (form.outer == layer_kind::leaf || kind_of(stored) != form.outer) {
      continue;
    }
    if (form.inner == layer_kind::leaf) {
      if (depth < 1) {
        best = form.form;
        depth = 1;
      }
    } else if (stored.nodes[0].arity == 1 && kind_of(stored, 1) == form.inner) {
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
  switch (kind_of(value, index)) {
  case layer_kind::leaf:
    return value.nodes[index].constructible;
  case layer_kind::ref:
  case layer_kind::array_unbounded:
    return false;
  default:
    return constructible(value, index + 1);
  }
}

constexpr std::uint32_t constructed_leaves(const shape &value,
                                           std::size_t index = 0) {
  switch (kind_of(value, index)) {
  case layer_kind::leaf:
    return 1;
  case layer_kind::array:
    return value.nodes[index].extent * constructed_leaves(value, index + 1);
  default:
    return constructed_leaves(value, index + 1);
  }
}

constexpr std::uint32_t leaves_held_by_value(const shape &value,
                                             std::size_t index = 0) {
  switch (kind_of(value, index)) {
  case layer_kind::leaf:
    return 1;
  case layer_kind::ptr:
  case layer_kind::share:
  case layer_kind::own:
  case layer_kind::ref:
    return 0;
  case layer_kind::array:
    return value.nodes[index].extent * leaves_held_by_value(value, index + 1);
  default:
    return leaves_held_by_value(value, index + 1);
  }
}

constexpr bool addressable(const shape &value) {
  return kind_of(value) != layer_kind::ptr;
}

constexpr bool holds_in_array(const shape &value) {
  for (std::size_t index = 0; index < value.size; ++index) {
    layer_kind kind = kind_of(value, index);
    if (kind == layer_kind::array || kind == layer_kind::array_unbounded) {
      return true;
    }
  }
  return false;
}

// The leaf at the end of the first-argument chain.
constexpr type_id innermost_leaf(const shape &value) {
  std::size_t index = 0;
  while (kind_of(value, index) != layer_kind::leaf) {
    ++index;
  }
  return value.nodes[index].type;
}

// Whether a layer that a route creates can be copied.
constexpr bool created_copyable(layer_kind kind, const shape &argument) {
  switch (kind) {
  case layer_kind::ptr:
  case layer_kind::share:
    return true;
  case layer_kind::opt:
    return argument.nodes[0].copyable;
  default:
    return false;
  }
}

constexpr bool is_wrapper_kind(layer_kind kind) {
  return kind == layer_kind::alt || kind == layer_kind::opt ||
         kind == layer_kind::own || kind == layer_kind::share;
}

constexpr bool requirements_hold(const binding_descriptor &binding,
                                 const route_rule &rule, const shape &bound) {
  auto requires_ = [&](requirement value) {
    return (rule.requires_ & static_cast<std::uint8_t>(value)) != 0;
  };
  if (requires_(requirement::copyable) && !bound.nodes[0].copyable) {
    return false;
  }
  if (requires_(requirement::array) && kind_of(bound) != layer_kind::array) {
    return false;
  }
  if (requires_(requirement::addressable) && !addressable(bound)) {
    return false;
  }
  if (requires_(requirement::leaf) && kind_of(bound) != layer_kind::leaf) {
    return false;
  }
  if (rule.id == rule_id::copy &&
      binding.scope.rule == exposure_rule::handle_copies &&
      !info(kind_of(bound)).sharing) {
    return false;
  }
  return true;
}

// Derivation

namespace detail {
constexpr bool seen(const terms &found, const shape &value,
                    value_category category) {
  for (std::size_t index = 0; index < found.size; ++index) {
    if (found.items[index].category == category &&
        found.items[index].value == value) {
      return true;
    }
  }
  return false;
}

constexpr void add(terms &found, const term &from, const route_rule &rule,
                   const shape &value, const shape &bound,
                   std::size_t argument) {
  if (seen(found, value, rule.to_category)) {
    return;
  }
  if (found.size == max_terms) {
    found.overflow = true;
    return;
  }
  term derived = from;
  derived.value = value;
  derived.category = rule.to_category;
  derived.arguments[derived.rule_count] = static_cast<std::uint8_t>(argument);
  derived.rules[derived.rule_count++] = rule.id;
  if (rule.id == rule_id::copy) {
    derived.copied = true;
    derived.copied_leaves = leaves_held_by_value(bound);
    // A copy is a new object, without the qualifiers of its source.
    derived.value.nodes[0].qualifiers = 0;
  }
  derived.consumed = from.consumed || rule.rule_access == access::consume;
  found.items[found.size++] = derived;
}

// Applies `rule` to `from` with the pattern variable bound to `bound`, the
// argument at `argument` of the source shape when the rule starts from a
// layer.
constexpr void apply(const binding_descriptor &binding, terms &found,
                     const term &from, const route_rule &rule,
                     const shape &bound, std::size_t argument,
                     std::size_t limit) {
  if (!requirements_hold(binding, rule, bound)) {
    return;
  }
  const shape &source = from.value;
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
      shape value =
          rule.to_layer == layer_kind::leaf
              ? interface
              : wrap(static_cast<std::uint8_t>(rule.to_layer),
                     source.nodes[0].family, source.nodes[0].copyable,
                     interface, source.nodes[0].extent);
      add(found, from, rule, value, bound, index);
    }
    return;
  }
  shape value = bound;
  // The element of a layer that holds it in place, an optional or an
  // alternative, has the qualifiers of the layer.
  if (rule.to_layer == layer_kind::leaf &&
      (rule.from_layer == layer_kind::opt ||
       rule.from_layer == layer_kind::alt)) {
    value.nodes[0].qualifiers |= source.nodes[0].qualifiers;
  }
  if (rule.to_layer != layer_kind::leaf) {
    type_id family = nullptr;
    if (is_wrapper_kind(rule.to_layer)) {
      bool keeps = rule.from_layer != layer_kind::leaf &&
                   source.nodes[0].family != nullptr;
      family = keeps ? source.nodes[0].family : any_family;
    }
    value = wrap(static_cast<std::uint8_t>(rule.to_layer), family,
                 created_copyable(rule.to_layer, bound), bound);
  }
  if (value.size > limit) {
    return;
  }
  add(found, from, rule, value, bound, argument);
}
} // namespace detail

namespace detail {
// Applies every rule to the term at `next` of `found`.
constexpr void derive_from(const binding_descriptor &binding, terms &found,
                           std::size_t next, std::uint8_t exposed,
                           std::size_t limit) {
  term from = found.items[next];
  if (from.rule_count == max_rules_per_route) {
    return;
  }
  for (const route_rule &rule : route_rules) {
    if (rule.from_category != from.category ||
        (rule.exposure != 0 && (exposed & rule.exposure) == 0)) {
      continue;
    }
    if (rule.from_layer == layer_kind::leaf) {
      apply(binding, found, from, rule, from.value, 0, limit);
      continue;
    }
    if (kind_of(from.value) != rule.from_layer) {
      continue;
    }
    // Only an alternative matches any of its arguments.
    std::size_t arguments =
        rule.from_layer == layer_kind::alt ? from.value.nodes[0].arity : 1;
    for (std::size_t argument = 0; argument < arguments; ++argument) {
      shape bound =
          subtree(from.value, argument_index(from.value, 0, argument));
      apply(binding, found, from, rule, bound, argument, limit);
    }
  }
}
} // namespace detail

constexpr terms derive(const binding_descriptor &binding) {
  const scope_info &scope = binding.scope;
  terms found;
  term start;
  start.value = binding.stored;
  start.category = scope.materializes;
  start.consumed = scope.materializes == value_category::rvalue;
  found.items[found.size++] = start;
  std::uint8_t exposed = exposed_categories(binding);
  std::size_t limit = binding.stored.size + 1;
  for (std::size_t next = 0; next < found.size; ++next) {
    detail::derive_from(binding, found, next, exposed, limit);
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
    const layer_kind kind = static_cast<layer_kind>(d.kind);
    if (kind == layer_kind::array || kind == layer_kind::array_unbounded) {
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
    if (kind == layer_kind::ptr) {
      ++index;
      level = true;
      continue;
    }
    if (kind != layer_kind::leaf) {
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
  layer_kind kind = kind_of(candidate.value);
  if (kind == layer_kind::array || kind == layer_kind::array_unbounded) {
    return false;
  }
  if (candidate.category == value_category::rvalue) {
    return exposes(exposed, request_category::rvalue);
  }
  return request.kind == request_kind::value &&
         candidate.category == value_category::prvalue;
}

// Whether `left` is preferred to `right`: fewer rules, then no copy, then the
// rules that come first in the spec.
constexpr bool preferred(const term &left, const term &right) {
  if (left.rule_count != right.rule_count) {
    return left.rule_count < right.rule_count;
  }
  if (left.copied != right.copied) {
    return !left.copied;
  }
  for (std::size_t index = 0; index < left.rule_count; ++index) {
    if (left.rules[index] != right.rules[index]) {
      return left.rules[index] < right.rules[index];
    }
  }
  return false;
}

inline constexpr std::size_t no_term = static_cast<std::size_t>(-1);

// The term of `found` that serves the request, or no_term.
constexpr std::size_t select_term(const terms &found,
                                  const request_descriptor &request,
                                  std::uint8_t exposed) {
  std::size_t best = no_term;
  for (std::size_t index = 0; index < found.size; ++index) {
    if (accepts(request, found.items[index], exposed) &&
        (best == no_term || preferred(found.items[index], found.items[best]))) {
      best = index;
    }
  }
  return best;
}

// Whether the request names the binding: its leaf is a declared interface, or
// the stored leaf when no interface is declared.
// Whether a binding serves the interface a request names: its declared
// interfaces, or its stored leaf without them.
constexpr bool serves_interface(const binding_descriptor &binding,
                                type_id requested) {
  if (binding.interface_count == 0) {
    return requested == innermost_leaf(binding.stored);
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
    return serves_interface(left, innermost_leaf(right.stored));
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

constexpr registration_status
register_status(const binding_descriptor &binding) {
  const scope_info &scope = binding.scope;
  if (exposed_categories(binding) == 0) {
    return registration_status::form_not_stored;
  }
  if (!scope.nested_layers && binding.stored.nodes[0].kind != 0) {
    std::size_t arguments = binding.stored.nodes[0].arity;
    for (std::size_t argument = 0; argument < arguments; ++argument) {
      if (binding.stored.nodes[argument_index(binding.stored, 0, argument)]
              .kind != 0) {
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
    for (std::size_t index = 0; index < binding.interface_count; ++index) {
      const shape &interface = binding.interfaces[index];
      request_descriptor probes[] = {
          {request_kind::lvalue_reference, interface, binding.key},
          {request_kind::value,
           wrap(static_cast<std::uint8_t>(layer_kind::ptr), nullptr, true,
                interface),
           binding.key},
          {request_kind::value,
           wrap(static_cast<std::uint8_t>(layer_kind::own), any_family, false,
                interface),
           binding.key},
          {request_kind::value,
           wrap(static_cast<std::uint8_t>(layer_kind::share), any_family, true,
                interface),
           binding.key},
      };
      for (const request_descriptor &probe : probes) {
        if (select_term(found, probe, exposed) != no_term) {
          return registration_status::registered;
        }
      }
    }
    return registration_status::no_interface_served;
  }
  return registration_status::registered;
}

} // namespace dingo::core
