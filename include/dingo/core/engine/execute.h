//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

// A binding of the next implementation and the execution of its routes. A
// binding type derives the terms it serves at compile time, and walk applies
// the rules of one term to the stored object, one C++ operation per rule as
// the rule table describes it.
//
// The binding applies the rules of a term up to its boundary, and the request
// completes it: a last copy, address-of, or decay of a borrowed object depends
// only on the requested type, and a layer a rule creates from a leaf or a
// pointer may be of any family, whose wrapper type only the request names. A
// runtime container erases the binding at the boundary, and a static
// container completes the term in the same way.

#include <dingo/core/base/mechanics.h>
#include <dingo/core/engine/derive.h>
#include <dingo/core/primitives/retained.h>
#include <dingo/core/primitives/shape.h>
#include <dingo/core/projection/annotated.h>
#include <dingo/core/projection/construct.h>
#include <dingo/core/projection/conversion.h>
#include <dingo/core/projection/layer_traits.h>
#include <dingo/core/vocabulary/compile_diagnostics.gen.h>
#include <dingo/core/vocabulary/scope_tags.gen.h>
#include <dingo/core/vocabulary/vocabulary.gen.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <type_traits>
#include <utility>

namespace dingo::core {

// The shapes of the declared interfaces of a binding of the stored leaf
// `Leaf`, and the ones the leaf declares a user conversion to.
template <typename Leaf, typename List> struct interface_shapes;

template <typename Leaf, typename... Interfaces>
struct interface_shapes<Leaf, type_list<Interfaces...>> {
  static_assert(require<sizeof...(Interfaces) <= max_interfaces,
                        diag::interface_limit, Leaf>());

  static constexpr binding_descriptor apply(binding_descriptor binding) {
    std::size_t index = 0;
    ((binding.interfaces[index++] = interface_shape_v<Interfaces>), ...);
    binding.interface_count = sizeof...(Interfaces);
    index = 0;
    ((binding.converted |=
      (declares_conversion<interface_object_t<Interfaces>, Leaf>::value
           ? 1u << index
           : 0u),
      ++index),
     ...);
    return binding;
  }
};

// Whether a term derives an object by a cached rule.
constexpr bool keeps_objects(const terms &published) {
  for (std::size_t index = 0; index < published.size; ++index) {
    const term &route = published.items[index];
    for (std::size_t step = 0; step < route.rule_count; ++step) {
      if (route_rules[static_cast<std::size_t>(route.rules[step])].cached) {
        return true;
      }
    }
  }
  return false;
}

// The context of a binding without cached rules.
struct no_kept_objects {
  void clear() noexcept {}
};

// A binding of a scope, a stored type, the interfaces it declares, the factory
// that makes its object, detected_factory unless one is declared, its key, and
// the static bindings it declares locally, a type_list of binding types. The
// scope is its tag type, whose scope_traits hold its descriptor.
template <typename Scope, typename Stored, typename Interfaces,
          typename Factory = detected_factory, typename Key = no_key,
          typename Locals = type_list<>, typename KeyAliases = type_list<>>
struct binding_type {
  using stored_type = Stored;
  using interfaces = Interfaces;
  using factory_type = Factory;
  using key_type = Key;
  // The runtime keys of other key types the binding is also found by.
  using key_aliases = KeyAliases;
  using locals = Locals;
  using scope_tag = Scope;
  static constexpr const scope_info &scope = scope_of_v<Scope>;
  // Whether a request consumes an object the binding constructs for it.
  static constexpr bool consumes = scope.consumes;
  // The stored object the slot yields to a walk: the object of a borrowing
  // scope, or of an array, which a consuming scope allocates so its ownership
  // passes to the request, and a new object otherwise.
  using yielded_type = std::conditional_t<consumes && !std::is_array_v<Stored>,
                                          Stored, Stored &>;

  static constexpr binding_descriptor make_descriptor() {
    binding_descriptor binding;
    binding.stored = shape_v<Stored>;
    binding.scope = scope;
    binding.declared_factory = !std::is_same_v<Factory, detected_factory>;
    binding.key = Key::descriptor;
    set_aliases(binding, KeyAliases());
    return interface_shapes<leaf_of_t<Stored>, Interfaces>::apply(binding);
  }

  template <typename... Aliases>
  static constexpr void set_aliases(binding_descriptor &binding,
                                    type_list<Aliases...> /*aliases*/) {
    static_assert(require<sizeof...(Aliases) <= max_key_aliases,
                          diag::key_alias_limit, Stored>());
    ((binding.aliases[binding.alias_count++] = Aliases::descriptor), ...);
  }

  static constexpr binding_descriptor descriptor = make_descriptor();
  static constexpr registration_status status = register_status(descriptor);
  static constexpr terms published = derive(descriptor);
  static constexpr std::uint8_t exposed = exposed_categories(descriptor);
  // Whether a term keeps an object it derives, in the context of the walk.
  static constexpr bool keeps = keeps_objects(published);
  using context_type = std::conditional_t<keeps, kept_objects, no_kept_objects>;
};

namespace detail {
template <typename Binding, typename Interfaces> struct first_interface {
  using type = leaf_of_t<typename Binding::stored_type>;
};

template <typename Binding, typename First, typename... Rest>
struct first_interface<Binding, type_list<First, Rest...>> {
  using type = First;
};
} // namespace detail

// The interface a binding serves first, which diagnostics name: its first
// declared interface, or its stored leaf.
template <typename Binding>
using first_interface_t =
    typename detail::first_interface<Binding,
                                     typename Binding::interfaces>::type;

// Whether the scope of `Binding` stores it, reporting why not when the
// program is compiled.
template <typename Binding> constexpr bool registers() {
  using stored = typename Binding::stored_type;
  constexpr registration_status status = Binding::status;
  static_assert(require<status != registration_status::form_not_stored,
                        diag::scope_form_not_stored, stored>());
  static_assert(require<status != registration_status::nested_layers,
                        diag::scope_nested_layers, stored>());
  static_assert(require<status != registration_status::no_default_factory,
                        diag::no_default_factory, stored>());
  static_assert(require<status != registration_status::interface_in_array,
                        diag::interface_in_array, stored>());
  static_assert(require<status != registration_status::no_interface_served,
                        diag::no_interface_served, stored>());
  return status == registration_status::registered;
}

// The result of walking a route whose operation the next implementation does
// not implement yet.
struct unsupported_route {};

// Whether a rule creates a wrapper layer from a leaf or a language layer: the
// layer may be of any family, so the request names its wrapper type.
constexpr bool creates_wrapper(rule_id id) {
  const route_rule &rule = route_rules[static_cast<std::size_t>(id)];
  return is_wrapper_kind(rule.to_layer) && !is_wrapper_kind(rule.from_layer);
}

// How the request completes a term the binding walked to its boundary: it
// refers to the borrowed object, copies it, takes its address or decays it;
// or the binding constructs the boundary object and the request makes the
// requested object from it.
enum class completion : std::uint8_t { refer, copy, address, construct };

inline constexpr std::size_t max_created_layers = 2;

struct completion_plan {
  completion kind = completion::construct;
  std::size_t binding_steps = 0;
  // The creating rules the request applies, innermost first.
  rule_id creating[max_created_layers] = {};
  std::size_t created = 0;
  // Whether the binding converts the pointer it hands over to the interface
  // `upcast_argument`, for an upcast of a handle the request creates.
  bool upcast = false;
  std::size_t upcast_argument = 0;
  // Whether the rules after the boundary are ones the request applies.
  bool supported = true;
};

constexpr completion_plan plan_completion(const term &route) {
  completion_plan plan;
  if (route.category == value_category::lvalue) {
    plan.kind = completion::refer;
    plan.binding_steps = route.rule_count;
    return plan;
  }
  if (route.rule_count > 0) {
    switch (route.rules[route.rule_count - 1]) {
    case rule_id::copy:
      plan.kind = completion::copy;
      plan.binding_steps = route.rule_count - 1;
      return plan;
    case rule_id::address_of:
    case rule_id::decay_array:
    case rule_id::decay_unbounded_array:
      plan.kind = completion::address;
      plan.binding_steps = route.rule_count - 1;
      return plan;
    default:
      break;
    }
  }
  std::size_t step = 0;
  while (step < route.rule_count && !creates_wrapper(route.rules[step])) {
    ++step;
  }
  plan.binding_steps = step;
  for (; step < route.rule_count; ++step) {
    const rule_id id = route.rules[step];
    if (route_rules[static_cast<std::size_t>(id)].upcast && plan.created > 0 &&
        plan.creating[plan.created - 1] != rule_id::wrap_optional) {
      plan.upcast = true;
      plan.upcast_argument = route.arguments[step];
    } else if (creates_wrapper(id) && plan.created < max_created_layers) {
      plan.creating[plan.created++] = id;
    } else {
      plan.supported = false;
    }
  }
  return plan;
}

template <typename Binding, std::size_t Term>
inline constexpr completion_plan plan_v =
    plan_completion(Binding::published.items[Term]);

namespace detail {
template <typename Type>
using remove_cvref_t = std::remove_cv_t<std::remove_reference_t<Type>>;

template <typename Binding, std::size_t Term, std::size_t Steps,
          std::size_t Step, typename Context, typename Value>
decltype(auto) walk_step(Context &context, Value &&value);

// Continues the walk with the result of a step that yields a value: the last
// step returns it directly, so it is constructed in place by the caller.
template <typename Binding, std::size_t Term, std::size_t Steps,
          std::size_t Step, typename Context, typename Make>
decltype(auto) continue_with(Context &context, Make &&make) {
  if constexpr (Step + 1 == Steps) {
    return make();
  } else {
    return walk_step<Binding, Term, Steps, Step + 1>(context, make());
  }
}
} // namespace detail

// Applies the first `Steps` rules of term `Term` of `Binding` to `value`, the
// stored object. A route that ends in an lvalue yields a reference, any other
// a value. `context` keeps the objects the cached rules derive.
template <typename Binding, std::size_t Term,
          std::size_t Steps = Binding::published.items[Term].rule_count,
          typename Context, typename Value>
decltype(auto) walk(Context &context, Value &&value) {
  if constexpr (Steps == 0) {
    return static_cast<Value &&>(value);
  } else {
    return detail::walk_step<Binding, Term, Steps, 0>(
        context, std::forward<Value>(value));
  }
}

namespace detail {
template <typename Traits, typename Value, typename = void>
inline constexpr bool has_share_v = false;

template <typename Traits, typename Value>
inline constexpr bool has_share_v<
    Traits, Value,
    std::void_t<decltype(Traits::share(std::declval<Value &&>()))>> = true;

// Step `Step` of the walk of the first `Steps` rules of term `Term`.
template <typename Binding, std::size_t Term, std::size_t Steps,
          std::size_t Step>
struct step {
  static constexpr const route_rule &rule =
      route_rules[static_cast<std::size_t>(
          Binding::published.items[Term].rules[Step])];
  // The argument of the source layer the rule reaches, or for an upcast, the
  // index of the interface it converts to.
  static constexpr std::size_t argument =
      Binding::published.items[Term].arguments[Step];

  // Passes on an lvalue the step reaches; the last step returns it.
  template <typename Context, typename Reached>
  static decltype(auto) reach(Context &context, Reached &&reached) {
    if constexpr (Step + 1 == Steps) {
      return static_cast<Reached &&>(reached);
    } else {
      return walk_step<Binding, Term, Steps, Step + 1>(
          context, static_cast<Reached &&>(reached));
    }
  }

  template <typename Context, typename Make>
  static decltype(auto) yield(Context &context, Make &&make) {
    return continue_with<Binding, Term, Steps, Step>(context,
                                                     std::forward<Make>(make));
  }
};

template <typename Step, typename Binding>
using interface_t = interface_object_t<
    type_list_at_t<Step::argument, typename Binding::interfaces>>;

// A borrowed handle converts to the handle of the interface, which the binding
// keeps in the context, so the request can refer to it.
template <typename Binding, typename Step, typename Context, typename Value>
decltype(auto) keep_step(Context &context, Value &value) {
  using target = typename layer_traits<remove_cvref_t<Value>>::template rebind<
      interface_t<Step, Binding>>;
  if constexpr (converts<target, remove_cvref_t<Value>, const Value &>()) {
    return Step::reach(
        context,
        context.template get_or_emplace<target>(type_address_v<target>, [&] {
          return convert_to<target>(std::as_const(value));
        }));
  } else {
    return unsupported_route{};
  }
}

// A leaf, a pointer, or a consumed or copied handle converts to one of an
// interface.
template <typename Binding, typename Step, typename Context, typename Value>
decltype(auto) upcast_step(Context &context, Value &&value) {
  using interface = interface_t<Step, Binding>;
  if constexpr (Step::rule.to_layer == layer_kind::leaf ||
                Step::rule.to_layer == layer_kind::ptr) {
    using leaf = std::remove_pointer_t<remove_cvref_t<Value>>;
    static_assert(require<std::is_convertible_v<leaf *, interface *>,
                          diag::interface_not_derived,
                          typename Binding::stored_type, interface>());
  }
  if constexpr (Step::rule.to_layer == layer_kind::leaf) {
    return Step::reach(context, static_cast<interface &>(value));
  } else if constexpr (Step::rule.to_layer == layer_kind::ptr) {
    return Step::yield(context,
                       [&] { return static_cast<interface *>(value); });
  } else {
    using target = typename layer_traits<
        remove_cvref_t<Value>>::template rebind<interface>;
    if constexpr (converts<target, remove_cvref_t<Value>, Value &&>()) {
      return Step::yield(context, [&] {
        return convert_to<target>(std::forward<Value>(value));
      });
    } else {
      return unsupported_route{};
    }
  }
}

// The leaf makes an object of an interface by the user conversion it declares
// to it, with the access the conversion needs: a borrowed leaf converts when
// the conversion borrows.
template <typename Binding, typename Step, typename Context, typename Value>
decltype(auto) convert_step(Context &context, Value &&value) {
  using target = interface_t<Step, Binding>;
  using source = remove_cvref_t<Value>;
  if constexpr (converts<target, source, Value &&>()) {
    return Step::yield(context, [&] {
      return convert_to<target>(std::forward<Value>(value));
    });
  } else {
    return unsupported_route{};
  }
}

// A consumed object moves out of a layer, a sole-owner handle shares its
// element, or an array hands out its first element or its address.
template <typename Step, typename Context, typename Value>
decltype(auto) consume_step(Context &context, Value &&value) {
  using value_type = remove_cvref_t<Value>;
  using traits = layer_traits<value_type>;
  constexpr const route_rule &rule = Step::rule;
  if constexpr (rule.from_layer == layer_kind::alt) {
    return Step::reach(
        context, std::move(traits::template element<Step::argument>(value)));
  } else if constexpr (rule.from_layer == layer_kind::array) {
    return Step::yield(context, [&] { return &value[0]; });
  } else if constexpr (rule.to_layer == layer_kind::leaf) {
    return Step::reach(context, std::move(traits::element(value)));
  } else if constexpr (rule.to_layer == layer_kind::share) {
    if constexpr (has_share_v<traits, value_type>) {
      return Step::yield(context, [&] {
        return traits::share(static_cast<value_type &&>(value));
      });
    } else {
      return unsupported_route{};
    }
  } else if constexpr (rule.to_layer == layer_kind::ptr &&
                       std::is_array_v<value_type>) {
    return Step::yield(context, [&] { return std::addressof(value); });
  } else {
    return unsupported_route{};
  }
}

// A borrowed object reaches the object a layer holds, or is copied, or its
// address is taken, or it decays.
template <typename Step, typename Context, typename Value>
decltype(auto) borrow_step(Context &context, Value &value) {
  using value_type = remove_cvref_t<Value>;
  using traits = layer_traits<value_type>;
  constexpr const route_rule &rule = Step::rule;
  if constexpr (rule.from_layer == layer_kind::ref) {
    // A reference layer is the object it aliases.
    return Step::reach(context, value);
  } else if constexpr (rule.from_layer == layer_kind::alt) {
    return Step::reach(context,
                       traits::template element<Step::argument>(value));
  } else if constexpr (rule.from_layer != layer_kind::leaf &&
                       rule.to_layer == layer_kind::leaf) {
    return Step::reach(context, traits::element(value));
  } else if constexpr (rule.to_layer == layer_kind::leaf) {
    return Step::yield(context, [&] { return value_type(value); });
  } else if constexpr (rule.from_layer == layer_kind::leaf) {
    return Step::yield(context, [&] { return std::addressof(value); });
  } else if constexpr (rule.to_layer == layer_kind::ptr) {
    return Step::yield(context, [&] { return &value[0]; });
  } else {
    return unsupported_route{};
  }
}

template <typename Binding, std::size_t Term, std::size_t Steps,
          std::size_t Step, typename Context, typename Value>
decltype(auto) walk_step(Context &context, Value &&value) {
  using current = step<Binding, Term, Steps, Step>;
  constexpr const route_rule &rule = current::rule;
  if constexpr (rule.cached) {
    return keep_step<Binding, current>(context, value);
  } else if constexpr (rule.converts) {
    return convert_step<Binding, current>(context, std::forward<Value>(value));
  } else if constexpr (rule.upcast) {
    return upcast_step<Binding, current>(context, std::forward<Value>(value));
  } else if constexpr (rule.from_category == value_category::rvalue) {
    return consume_step<current>(context, std::forward<Value>(value));
  } else {
    return borrow_step<current>(context, value);
  }
}
} // namespace detail

// Walks the first `Steps` rules of term `Term` from the stored object that
// `source()` yields: a reference to the object of a borrowing scope, or the
// object a consuming scope constructs for the request. A walk from a
// constructed object returns a value, which outlives the object.
template <typename Binding, std::size_t Term, std::size_t Steps,
          typename Context, typename Source>
DINGO_ALWAYS_INLINE decltype(auto) walk_from(Context &context,
                                             Source &&source) {
  if constexpr (std::is_lvalue_reference_v<decltype(source())>) {
    return walk<Binding, Term, Steps>(context, source());
  } else if constexpr (Steps == 0) {
    return source();
  } else {
    using result = detail::remove_cvref_t<decltype(walk<Binding, Term, Steps>(
        context, source()))>;
    return result(walk<Binding, Term, Steps>(context, source()));
  }
}

// The object the binding hands over at the boundary of term `Term`.
template <typename Binding, std::size_t Term, typename Context, typename Source>
DINGO_ALWAYS_INLINE decltype(auto) walk_to_boundary(Context &context,
                                                    Source &&source) {
  constexpr const completion_plan &plan = plan_v<Binding, Term>;
  if constexpr (plan.upcast) {
    using interface = interface_object_t<
        type_list_at_t<plan.upcast_argument, typename Binding::interfaces>>;
    return static_cast<interface *>(
        walk_from<Binding, Term, plan.binding_steps>(context, source));
  } else {
    return walk_from<Binding, Term, plan.binding_steps>(context, source);
  }
}

namespace detail {
// The type of the boundary object, computed from the walk instantiations that
// walk_to_boundary uses, so it instantiates no other function.
template <typename Binding, std::size_t Term,
          bool Upcast = plan_v<Binding, Term>.upcast>
struct boundary {
  using type = remove_cvref_t<
      decltype(walk<Binding, Term, plan_v<Binding, Term>.binding_steps>(
          std::declval<typename Binding::context_type &>(),
          std::declval<typename Binding::yielded_type>()))>;
};

template <typename Binding, std::size_t Term>
struct boundary<Binding, Term, true> {
  using type = std::conditional_t<
      std::is_same_v<typename boundary<Binding, Term, false>::type,
                     unsupported_route>,
      unsupported_route,
      interface_object_t<type_list_at_t<plan_v<Binding, Term>.upcast_argument,
                                        typename Binding::interfaces>> *>;
};
} // namespace detail

template <typename Binding, std::size_t Term>
using boundary_t = typename detail::boundary<Binding, Term>::type;

// Whether the binding walks term `Term` to its boundary.
template <typename Binding, std::size_t Term>
inline constexpr bool walk_supported_v =
    plan_v<Binding, Term>.supported &&
    !std::is_same_v<boundary_t<Binding, Term>, unsupported_route>;

namespace detail {
template <typename Type, typename = void> struct first_element {
  using type = void;
};

template <typename Type>
struct first_element<Type,
                     std::void_t<typename layer_traits<Type>::arguments>> {
  using type = type_list_at_t<0, typename layer_traits<Type>::arguments>;
};

template <typename Traits, typename Element, typename = void>
inline constexpr bool has_adopt_v = false;

template <typename Traits, typename Element>
inline constexpr bool has_adopt_v<
    Traits, Element,
    std::void_t<decltype(Traits::adopt(std::declval<Element *>()))>> = true;
} // namespace detail

namespace detail {
// Converts to the object `make()` returns, so a layer constructs it in place.
template <typename Type, typename Make> class converts_to {
public:
  explicit converts_to(Make &make) : make_(make) {}

  operator Type() const { return make_(); }

private:
  Make &make_;
};
} // namespace detail

// How the request makes `Target` from the boundary object, which `make()`
// returns, by the creating rules `Outer, Inner...`, outermost first:
// `boundary` is the type of the boundary object, and `valid` tells whether the
// rules make `Target`. Every layer constructs its element in place.
template <typename Target, rule_id... Rules> struct created_layers {
  using boundary = Target;
  static constexpr bool valid =
      !std::is_void_v<Target> && !std::is_array_v<Target>;

  template <typename Make> static auto make(Make &&make) { return make(); }
};

template <typename Target, rule_id Outer, rule_id... Inner>
struct created_layers<Target, Outer, Inner...> {
private:
  using traits = layer_traits<Target>;
  using element = typename detail::first_element<Target>::type;
  using inner = created_layers<element, Inner...>;
  static constexpr const route_rule &rule =
      route_rules[static_cast<std::size_t>(Outer)];
  static constexpr bool wraps = Outer == rule_id::wrap_optional;
  static constexpr bool kind_matches = traits::kind == rule.to_layer;

public:
  using boundary =
      std::conditional_t<wraps, typename inner::boundary, element *>;

  static constexpr bool valid = [] {
    if constexpr (!kind_matches || std::is_void_v<element>) {
      return false;
    } else if constexpr (wraps) {
      return inner::valid;
    } else {
      return sizeof...(Inner) == 0 && detail::has_adopt_v<traits, element>;
    }
  }();

  template <typename Make> static auto make(Make &&make) {
    if constexpr (wraps) {
      auto element_of = [&make] { return inner::make(make); };
      return traits::make(
          detail::converts_to<element, decltype(element_of)>(element_of));
    } else {
      return traits::adopt(make());
    }
  }
};

namespace detail {
template <typename Target, typename Binding, std::size_t Term,
          std::size_t... Indices>
created_layers<
    Target, plan_v<Binding, Term>.creating[sizeof...(Indices) - 1 - Indices]...>
    plan_layers(std::index_sequence<Indices...>);
} // namespace detail

// The creating rules of term `Term` applied to make `Target`.
template <typename Target, typename Binding, std::size_t Term>
using planned_layers_t = decltype(detail::plan_layers<Target, Binding, Term>(
    std::make_index_sequence<plan_v<Binding, Term>.created>()));

} // namespace dingo::core
