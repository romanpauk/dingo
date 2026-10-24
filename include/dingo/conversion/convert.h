//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

// A binding of the next implementation and the execution of its routes. A
// binding type derives the reachable types it serves at compile time, and walk
// applies the rules of one reachable type to the stored object, one C++
// operation per form: the operation of the wrapper_traits of the object, or of
// the language for an address.
//
// The binding applies the rules of a reachable type up to its boundary, and the
// request completes it: a last copy, address-of, or decay of a borrowed object
// depends only on the requested type, and a wrapper a rule creates from a leaf
// or a pointer may be of any family, whose wrapper type only the request names.
// A runtime container erases the binding at the boundary, and a static
// container completes the reachable type in the same way.

#include <dingo/base/mechanics.h>
#include <dingo/conversion/route.h>
#include <dingo/diagnostics/compile_diagnostics.gen.h>
#include <dingo/diagnostics/diagnostics.gen.h>
#include <dingo/diagnostics/type_name.h>
#include <dingo/factory/construct.h>
#include <dingo/memory/retained.h>
#include <dingo/storage/scopes.gen.h>
#include <dingo/type/annotated.h>
#include <dingo/type/type_conversion_traits.h>
#include <dingo/type/type_tree.h>
#include <dingo/type/wrapper_traits.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <type_traits>
#include <utility>

namespace dingo::core {

// The type trees of the declared interfaces of a binding of the stored leaf
// `Leaf`, and the ones the leaf declares a user conversion to.
template <typename Leaf, typename List> struct interface_type_trees;

template <typename Leaf, typename... Interfaces>
struct interface_type_trees<Leaf, type_list<Interfaces...>> {
  static_assert(require<sizeof...(Interfaces) <= max_interfaces,
                        diag::interface_limit, Leaf>());

  static constexpr binding_descriptor apply(binding_descriptor binding) {
    std::size_t index = 0;
    ((binding.interfaces[index++] = interface_type_tree_v<Interfaces>), ...);
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

// Whether a reachable type derives an object by a cached rule.
constexpr bool keeps_objects(const reachable_types &published) {
  for (std::size_t index = 0; index < published.size; ++index) {
    const reachable_type &route = published.items[index];
    for (std::size_t step = 0; step < route.rule_count; ++step) {
      if (info(route.rules[step].form).cached) {
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
    binding.stored = type_tree_v<Stored>;
    binding.leaves = named_leaves(binding.stored);
    binding.scope = scope;
    binding.declared_factory = !std::is_same_v<Factory, detected_factory>;
    binding.key = Key::descriptor;
    set_aliases(binding, KeyAliases());
    return interface_type_trees<leaf_of_t<Stored>, Interfaces>::apply(binding);
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
  static constexpr reachable_types published = derive(descriptor);
  static constexpr std::uint8_t exposed = exposed_categories(descriptor);
  // Whether a reachable type keeps an object it derives, in the context of the
  // walk.
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
  static_assert(require<status != registration_status::nested_wrappers,
                        diag::scope_nested_wrappers, stored>());
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

// How the request completes a reachable type the binding walked to its
// boundary: it refers to the borrowed object, copies it, takes its address or
// decays it; or the binding constructs the boundary object and the request
// makes the requested object from it.
enum class completion : std::uint8_t { refer, copy, address, construct };

// How the binding walks a reachable type: the completion of the reachable type,
// and the rules it applies before the request completes it.
struct completion_plan {
  completion kind = completion::construct;
  std::size_t binding_steps = 0;
  // Whether the route reaches into a variadic wrapper, whose held alternative
  // a request checks each time, so the address it borrows is not kept.
  bool rechecked = false;
};

constexpr completion_plan plan_completion(const reachable_type &route) {
  completion_plan plan;
  plan.binding_steps = route.rule_count;
  for (std::size_t step = 0; step < route.rule_count; ++step) {
    plan.rechecked =
        plan.rechecked ||
        has_properties(from_semantics(route.rules[step]), property::variadic);
  }
  if (route.category == value_category::lvalue) {
    plan.kind = completion::refer;
    return plan;
  }
  if (route.rule_count > 0) {
    switch (route.rules[route.rule_count - 1].form) {
    case operation::copy:
      plan.kind = completion::copy;
      plan.binding_steps = route.rule_count - 1;
      return plan;
    case operation::decay:
      plan.kind = completion::address;
      plan.binding_steps = route.rule_count - 1;
      return plan;
    default:
      break;
    }
  }
  return plan;
}

// How a request completes the route it takes: by the completion of the
// reachable type, or by taking the address of the object the binding refers to,
// or by making the wrappers it creates, innermost first, of the object the
// binding constructs.
struct request_plan {
  completion kind = completion::construct;
  operation creating[max_created_wrappers] = {};
  std::size_t created = 0;
};

constexpr request_plan plan_request(const completion_plan &reachable_type,
                                    const route_choice &choice) {
  request_plan plan;
  plan.kind = reachable_type.kind;
  if (choice.created_count == 0) {
    return plan;
  }
  const operation innermost = choice.created[0].form;
  if (innermost == operation::address ||
      innermost == operation::release_whole) {
    plan.kind = completion::address;
    return plan;
  }
  plan.kind = completion::construct;
  for (std::size_t index = 0; index < choice.created_count; ++index) {
    plan.creating[plan.created++] = choice.created[index].form;
  }
  return plan;
}

template <typename Binding, std::size_t ReachableType>
inline constexpr completion_plan plan_v =
    plan_completion(Binding::published.items[ReachableType]);

namespace detail {
template <typename Type>
using remove_cvref_t = std::remove_cv_t<std::remove_reference_t<Type>>;

template <typename Binding, std::size_t ReachableType, std::size_t Steps,
          std::size_t Step, typename Context, typename Value>
decltype(auto) walk_step(Context &context, Value &&value);

// Continues the walk with the result of a step that yields a value: the last
// step returns it directly, so it is constructed in place by the caller.
template <typename Binding, std::size_t ReachableType, std::size_t Steps,
          std::size_t Step, typename Context, typename Make>
decltype(auto) continue_with(Context &context, Make &&make) {
  if constexpr (Step + 1 == Steps) {
    return make();
  } else {
    return walk_step<Binding, ReachableType, Steps, Step + 1>(context, make());
  }
}
} // namespace detail

// Applies the first `Steps` rules of reachable type `Reachable type` of
// `Binding` to `value`, the stored object. A route that ends in an lvalue
// yields a reference, any other a value. `context` keeps the objects the cached
// rules derive.
template <
    typename Binding, std::size_t ReachableType,
    std::size_t Steps = Binding::published.items[ReachableType].rule_count,
    typename Context, typename Value>
decltype(auto) walk(Context &context, Value &&value) {
  if constexpr (Steps == 0) {
    return static_cast<Value &&>(value);
  } else {
    return detail::walk_step<Binding, ReachableType, Steps, 0>(
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

// The context of a walk to the address of the object a reachable type borrows:
// the walk yields the address of the object its last rule reaches, along the
// route it takes by the alternatives the objects hold, which is null, when
// `null_allowed`, for the element of a wrapper that holds none.
template <typename Context> struct address_walk {
  Context &context;
  bool null_allowed;

  template <typename Type, typename Make>
  Type &get_or_emplace(const void *key, Make &&make) {
    return context.template get_or_emplace<Type>(key, std::forward<Make>(make));
  }
};

template <typename Context> inline constexpr bool is_address_walk_v = false;

template <typename Context>
inline constexpr bool is_address_walk_v<address_walk<Context>> = true;

// Step `Step` of the walk of the first `Steps` rules of reachable type
// `Reachable type`.
template <typename Binding, std::size_t ReachableType, std::size_t Steps,
          std::size_t Step>
struct step {
  static constexpr const route_step &current =
      Binding::published.items[ReachableType].rules[Step];
  static constexpr const operation_info &rule = info(current.form);
  static constexpr operation form = current.form;
  // The kind the step derives, or the leaf kind for an element.
  static constexpr const semantics_descriptor *to = to_semantics(current);
  // The argument of the source wrapper the rule reaches, or for an upcast, the
  // index of the interface it converts to.
  static constexpr std::size_t argument =
      Binding::published.items[ReachableType].arguments[Step];
  static constexpr bool last = Step + 1 == Steps;

  // Passes on an lvalue the step reaches; the last step returns it, or its
  // address in a walk to an address.
  template <typename Context, typename Reached>
  static decltype(auto) reach(Context &context, Reached &&reached) {
    if constexpr (last && is_address_walk_v<Context>) {
      return std::addressof(reached);
    } else if constexpr (last) {
      return static_cast<Reached &&>(reached);
    } else {
      return walk_step<Binding, ReachableType, Steps, Step + 1>(
          context, static_cast<Reached &&>(reached));
    }
  }

  template <typename Context, typename Make>
  static decltype(auto) yield(Context &context, Make &&make) {
    return continue_with<Binding, ReachableType, Steps, Step>(
        context, std::forward<Make>(make));
  }
};

template <typename Step, typename Binding>
using interface_t = interface_object_t<
    type_list_at_t<Step::argument, typename Binding::interfaces>>;

// `Type` with the cv-qualifiers of `From`.
template <typename From, typename Type>
using copy_cv_t = std::conditional_t<
    std::is_const_v<From>,
    std::conditional_t<std::is_volatile_v<From>, const volatile Type,
                       const Type>,
    std::conditional_t<std::is_volatile_v<From>, volatile Type, Type>>;

// The interface of the step in place of the element of a wrapper, or of the
// object an address points to, with its qualifiers: a const object serves its
// interfaces as const ones.
template <typename Step, typename Binding, typename Element>
using qualified_interface_t = copy_cv_t<Element, interface_t<Step, Binding>>;

template <typename Wrapper>
using wrapper_element_t =
    type_list_at_t<0, typename wrapper_traits<Wrapper>::arguments>;

// A borrowed handle converts to the handle of the interface, which the binding
// keeps in the context, so the request can refer to it.
template <typename Binding, typename Step, typename Context, typename Value>
decltype(auto) keep_step(Context &context, Value &value) {
  using wrapper = remove_cvref_t<Value>;
  using target = typename wrapper_traits<wrapper>::template rebind<
      qualified_interface_t<Step, Binding, wrapper_element_t<wrapper>>>;
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
  // A leaf, or the element of an address, converts to a base of it.
  constexpr bool to_address = has_form(Step::to, operation::address);
  if constexpr (!is_wrapper(Step::to) || to_address) {
    using leaf = std::remove_pointer_t<remove_cvref_t<Value>>;
    static_assert(require<std::is_convertible_v<leaf *, interface *>,
                          diag::interface_not_derived,
                          typename Binding::stored_type, interface>());
  }
  if constexpr (!is_wrapper(Step::to)) {
    using target =
        qualified_interface_t<Step, Binding, std::remove_reference_t<Value>>;
    return Step::reach(context, static_cast<target &>(value));
  } else if constexpr (to_address) {
    using target =
        qualified_interface_t<Step, Binding,
                              std::remove_pointer_t<remove_cvref_t<Value>>>;
    return Step::yield(context, [&] { return static_cast<target *>(value); });
  } else {
    using wrapper = remove_cvref_t<Value>;
    using target = typename wrapper_traits<wrapper>::template rebind<
        qualified_interface_t<Step, Binding, wrapper_element_t<wrapper>>>;
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

// The element of a wrapper: the argument a step reaches of a wrapper of a
// variadic kind, which the object must hold, and the one element of another.
template <typename Step, typename Traits, typename Value>
decltype(auto) element_of(Value &&value) {
  if constexpr (has_properties(Step::current.kind, property::variadic)) {
    if (Traits::index(std::as_const(value)) != Step::argument) {
      raise_alternative_not_held(
          type_name_of<std::remove_cv_t<std::remove_reference_t<Value>>>());
    }
    return Traits::template element<Step::argument>(std::forward<Value>(value));
  } else {
    return Traits::element(std::forward<Value>(value));
  }
}

// Whether the object a step starts from is a wrapper of the kind of the step: a
// reference is not, as C++ refers to the object it aliases.
template <typename Step, typename Value>
inline constexpr bool is_step_wrapper_v =
    is_wrapper_v<Value> &&
    same_semantics(wrapper_semantics_v<Value>, Step::current.kind);

template <typename Traits, typename Value, typename = void>
inline constexpr bool has_element_check_v = false;

template <typename Traits, typename Value>
inline constexpr bool
    has_element_check_v<Traits, Value,
                        std::void_t<decltype(bool(Traits::has_element(
                            std::declval<const Value &>())))>> = true;

// Whether a step into the element of a wrapper checks that the object holds
// one: a nullable wrapper that holds its element in place has no element to
// refer to when it holds none.
template <typename Step, typename Traits, typename Value>
inline constexpr bool checks_element_v =
    has_properties(Step::current.kind, property::nullable) &&
    has_properties(Step::current.kind, property::in_place) &&
    has_element_check_v<Traits, Value>;

// A consumed object moves out of a wrapper, a handle shares its element, or an
// array hands out its first element or its address.
template <typename Step, typename Context, typename Value>
decltype(auto) consume_step(Context &context, Value &&value) {
  using value_type = remove_cvref_t<Value>;
  using traits = wrapper_traits<value_type>;
  if constexpr (Step::form == operation::take) {
    if constexpr (is_step_wrapper_v<Step, value_type>) {
      if constexpr (checks_element_v<Step, traits, value_type>) {
        if (!traits::has_element(std::as_const(value))) {
          raise_element_not_held(type_name_of<value_type>());
        }
      }
      return Step::reach(context, std::move(element_of<Step, traits>(value)));
    } else {
      return Step::reach(context, std::forward<Value>(value));
    }
  } else if constexpr (Step::form == operation::release) {
    return Step::yield(context, [&] { return traits::first(value); });
  } else if constexpr (Step::form == operation::share) {
    if constexpr (has_share_v<traits, value_type>) {
      return Step::yield(context, [&] {
        return traits::share(static_cast<value_type &&>(value));
      });
    } else {
      return unsupported_route{};
    }
  } else if constexpr (Step::form == operation::release_whole &&
                       std::is_array_v<value_type>) {
    return Step::yield(context, [&] { return std::addressof(value); });
  } else {
    return unsupported_route{};
  }
}

// A borrowed wrapper reaches the element it holds, which a nullable wrapper
// that holds it in place must hold; the last step of a walk to an address
// yields a null address for none, when the walk allows it.
template <typename Step, typename Traits, typename Context, typename Value>
decltype(auto) borrow_element(Context &context, Value &value) {
  using value_type = remove_cvref_t<Value>;
  if constexpr (checks_element_v<Step, Traits, value_type>) {
    if (!Traits::has_element(std::as_const(value))) {
      if constexpr (Step::last && is_address_walk_v<Context>) {
        if (context.null_allowed) {
          return decltype(std::addressof(element_of<Step, Traits>(value)))(
              nullptr);
        }
      }
      raise_element_not_held(type_name_of<value_type>());
    }
  }
  return Step::reach(context, element_of<Step, Traits>(value));
}

// A borrowed object reaches the object a wrapper holds, or is copied, or its
// address is taken, or it decays.
template <typename Step, typename Context, typename Value>
decltype(auto) borrow_step(Context &context, Value &value) {
  using value_type = remove_cvref_t<Value>;
  using traits = wrapper_traits<value_type>;
  if constexpr (Step::form == operation::borrow) {
    if constexpr (is_step_wrapper_v<Step, value_type>) {
      return borrow_element<Step, traits>(context, value);
    } else {
      return Step::reach(context, value);
    }
  } else if constexpr (Step::form == operation::copy) {
    return Step::yield(context, [&] { return value_type(value); });
  } else if constexpr (Step::form == operation::address) {
    return Step::yield(context, [&] { return std::addressof(value); });
  } else if constexpr (Step::form == operation::decay) {
    return Step::yield(context, [&] { return traits::first(value); });
  } else {
    return unsupported_route{};
  }
}

// The sibling of reachable type `Reachable type` that takes alternative
// `Alternative` at its step `Step`, or no_reachable_type.
template <typename Binding, std::size_t ReachableType, std::size_t Step,
          std::size_t Alternative>
inline constexpr std::size_t sibling_v = [] {
  const reachable_types &published = Binding::published;
  for (std::size_t index = 0; index < published.size; ++index) {
    const reachable_type &item = published.items[index];
    if (item.sibling && item.primary == ReachableType && item.branch == Step &&
        item.arguments[Step] == Alternative) {
      return index;
    }
  }
  return no_reachable_type;
}();

// Whether reachable type `Reachable type` has a sibling that parts from it at
// its step `Step`.
template <typename Binding, std::size_t ReachableType, std::size_t Step>
inline constexpr bool branches_v = [] {
  const reachable_types &published = Binding::published;
  for (std::size_t index = 0; index < published.size; ++index) {
    const reachable_type &item = published.items[index];
    if (item.sibling && item.primary == ReachableType && item.branch == Step) {
      return true;
    }
  }
  return false;
}();

template <typename Binding, std::size_t ReachableType, std::size_t Steps,
          std::size_t Step, typename Context, typename Value>
decltype(auto) take_step(Context &context, Value &&value);

// Continues the walk of reachable type `Reachable type` at its step `Step`,
// into a variadic wrapper whose object holds the alternative `held`, along the
// sibling that takes it from there, as many steps short of its end as the walk
// of `Reachable type` is; or along `Reachable type`, which reports that the
// object holds another alternative.
template <typename Binding, std::size_t ReachableType, std::size_t Steps,
          std::size_t Step, std::size_t Alternative, typename Context,
          typename Value>
decltype(auto) branch(std::size_t held, Context &context, Value &&value) {
  using arguments = typename wrapper_traits<remove_cvref_t<Value>>::arguments;
  if constexpr (Alternative == arguments::size) {
    return take_step<Binding, ReachableType, Steps, Step>(
        context, std::forward<Value>(value));
  } else {
    constexpr std::size_t sibling =
        sibling_v<Binding, ReachableType, Step, Alternative>;
    if constexpr (sibling != no_reachable_type) {
      if (held == Alternative) {
        constexpr std::size_t short_of =
            Binding::published.items[ReachableType].rule_count - Steps;
        return walk_step<
            Binding, sibling,
            Binding::published.items[sibling].rule_count - short_of, Step>(
            context, std::forward<Value>(value));
      }
    }
    return branch<Binding, ReachableType, Steps, Step, Alternative + 1>(
        held, context, std::forward<Value>(value));
  }
}

template <typename Binding, std::size_t ReachableType, std::size_t Steps,
          std::size_t Step, typename Context, typename Value>
decltype(auto) walk_step(Context &context, Value &&value) {
  if constexpr (branches_v<Binding, ReachableType, Step>) {
    using traits = wrapper_traits<remove_cvref_t<Value>>;
    const std::size_t held = traits::index(std::as_const(value));
    if (held != step<Binding, ReachableType, Steps, Step>::argument) {
      return branch<Binding, ReachableType, Steps, Step, 0>(
          held, context, std::forward<Value>(value));
    }
  }
  return take_step<Binding, ReachableType, Steps, Step>(
      context, std::forward<Value>(value));
}

template <typename Binding, std::size_t ReachableType, std::size_t Steps,
          std::size_t Step, typename Context, typename Value>
decltype(auto) take_step(Context &context, Value &&value) {
  using current = step<Binding, ReachableType, Steps, Step>;
  constexpr const operation_info &rule = current::rule;
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

// Walks the first `Steps` rules of reachable type `Reachable type` from the
// stored object that `source()` yields: a reference to the object of a
// borrowing scope, or the object a consuming scope constructs for the request.
// A walk from a constructed object returns a value, which outlives the object.
template <typename Binding, std::size_t ReachableType, std::size_t Steps,
          typename Context, typename Source>
DINGO_ALWAYS_INLINE decltype(auto) walk_from(Context &context,
                                             Source &&source) {
  if constexpr (std::is_lvalue_reference_v<decltype(source())>) {
    return walk<Binding, ReachableType, Steps>(context, source());
  } else if constexpr (Steps == 0) {
    return source();
  } else {
    using result =
        detail::remove_cvref_t<decltype(walk<Binding, ReachableType, Steps>(
            context, source()))>;
    return result(walk<Binding, ReachableType, Steps>(context, source()));
  }
}

namespace detail {
// Whether the last of the first `Steps` rules of reachable type `Reachable
// type` borrows the element of a nullable wrapper that holds it in place, which
// may hold none.
template <typename Binding, std::size_t ReachableType, std::size_t Steps>
constexpr bool reaches_held_element() {
  if constexpr (Steps == 0) {
    return false;
  } else {
    const route_step &last =
        Binding::published.items[ReachableType].rules[Steps - 1];
    return last.form == operation::borrow &&
           has_properties(last.kind, property::nullable) &&
           has_properties(last.kind, property::in_place);
  }
}
} // namespace detail

// The address of the object the first `Steps` rules of reachable type
// `Reachable type` reach from the stored object `source()` yields, which they
// borrow. When the last rule reaches the element of a wrapper that holds none,
// the address is null if `null_allowed`, and the request is refused otherwise.
// The last rule is the one of the route the walk takes: a sibling the object
// holds the alternative of may reach its object from a wrapper of another type.
template <typename Binding, std::size_t ReachableType, std::size_t Steps,
          typename Context, typename Source>
DINGO_ALWAYS_INLINE auto address_from(Context &context, Source &&source,
                                      bool null_allowed) {
  using pointer = decltype(std::addressof(
      walk_from<Binding, ReachableType, Steps>(context, source)));
  if constexpr (detail::reaches_held_element<Binding, ReachableType, Steps>() &&
                std::is_lvalue_reference_v<decltype(source())>) {
    detail::address_walk<Context> walk_context{context, null_allowed};
    return pointer(walk<Binding, ReachableType, Steps>(walk_context, source()));
  } else {
    return pointer(std::addressof(
        walk_from<Binding, ReachableType, Steps>(context, source)));
  }
}

// The object the binding hands over at the boundary of reachable type
// `Reachable type`.
template <typename Binding, std::size_t ReachableType, typename Context,
          typename Source>
DINGO_ALWAYS_INLINE decltype(auto) walk_to_boundary(Context &context,
                                                    Source &&source) {
  return walk_from<Binding, ReachableType,
                   plan_v<Binding, ReachableType>.binding_steps>(context,
                                                                 source);
}

namespace detail {
// The type of the boundary object, computed from the walk instantiations that
// walk_to_boundary uses, so it instantiates no other function.
template <typename Binding, std::size_t ReachableType> struct boundary {
  using type = remove_cvref_t<
      decltype(walk<Binding, ReachableType,
                    plan_v<Binding, ReachableType>.binding_steps>(
          std::declval<typename Binding::context_type &>(),
          std::declval<typename Binding::yielded_type>()))>;
};
} // namespace detail

template <typename Binding, std::size_t ReachableType>
using boundary_t = typename detail::boundary<Binding, ReachableType>::type;

// Whether the binding walks reachable type `Reachable type` to its boundary.
template <typename Binding, std::size_t ReachableType>
inline constexpr bool walk_supported_v =
    !std::is_same_v<boundary_t<Binding, ReachableType>, unsupported_route>;

namespace detail {
template <typename Type, typename = void> struct first_element {
  using type = void;
};

template <typename Type>
struct first_element<Type,
                     std::void_t<typename wrapper_traits<Type>::arguments>> {
  using type = type_list_at_t<0, typename wrapper_traits<Type>::arguments>;
};

template <typename Traits, typename Element, typename = void>
inline constexpr bool has_adopt_v = false;

template <typename Traits, typename Element>
inline constexpr bool has_adopt_v<
    Traits, Element,
    std::void_t<decltype(Traits::adopt(std::declval<Element *>()))>> = true;
} // namespace detail

namespace detail {
// Converts to the object `make()` returns, so a wrapper constructs it in place.
template <typename Type, typename Make> class converts_to {
public:
  explicit converts_to(Make &make) : make_(make) {}

  operator Type() const { return make_(); }

private:
  Make &make_;
};
} // namespace detail

// How the request makes `Target` from the boundary object, which `make()`
// returns, by the creating forms `Outer, Inner...`, outermost first:
// `boundary` is the type of the boundary object, and `valid` tells whether the
// forms make `Target`. Every wrapper constructs its element in place.
template <typename Target, operation... Forms> struct created_wrappers {
  using boundary = Target;
  static constexpr bool valid =
      !std::is_void_v<Target> && !std::is_array_v<Target>;

  template <typename Make> static auto make(Make &&make) { return make(); }
};

template <typename Target, operation Outer, operation... Inner>
struct created_wrappers<Target, Outer, Inner...> {
private:
  using traits = wrapper_traits<Target>;
  using element = typename detail::first_element<Target>::type;
  using inner = created_wrappers<element, Inner...>;
  static constexpr bool wraps = Outer == operation::wrap;
  static constexpr bool semantics_match =
      has_form(wrapper_semantics_v<Target>, Outer);

public:
  using boundary =
      std::conditional_t<wraps, typename inner::boundary, element *>;

  static constexpr bool valid = [] {
    if constexpr (!semantics_match || std::is_void_v<element>) {
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
template <typename Target, typename Plan, std::size_t... Indices>
created_wrappers<Target,
                 Plan::value.creating[sizeof...(Indices) - 1 - Indices]...>
    plan_wrappers(std::index_sequence<Indices...>);
} // namespace detail

// The creating forms of the request_plan `Plan::value` applied to make
// `Target`.
template <typename Target, typename Plan>
using planned_wrappers_t = decltype(detail::plan_wrappers<Target, Plan>(
    std::make_index_sequence<Plan::value.created>()));

} // namespace dingo::core
