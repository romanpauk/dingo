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

#include <dingo/core/base/mechanics.h>
#include <dingo/core/engine/derive.h>
#include <dingo/core/primitives/shape.h>
#include <dingo/core/projection/layer_traits.h>
#include <dingo/core/vocabulary/vocabulary.gen.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <type_traits>
#include <utility>

namespace dingo::core {

template <typename List> struct interface_shapes;

template <typename... Interfaces>
struct interface_shapes<type_list<Interfaces...>> {
  static constexpr binding_descriptor apply(binding_descriptor binding) {
    std::size_t index = 0;
    ((binding.interfaces[index++] = shape_v<Interfaces>), ...);
    binding.interface_count = sizeof...(Interfaces);
    return binding;
  }
};

template <scope_id Scope, typename Stored, typename Interfaces>
struct binding_type {
  using stored_type = Stored;
  using interfaces = Interfaces;
  static constexpr scope_id scope = Scope;

  static constexpr binding_descriptor make_descriptor() {
    binding_descriptor binding;
    binding.stored = shape_v<Stored>;
    binding.scope = Scope;
    return interface_shapes<Interfaces>::apply(binding);
  }

  static constexpr binding_descriptor descriptor = make_descriptor();
  static constexpr registration_status status = register_status(descriptor);
  static constexpr terms published = derive(descriptor);
  static constexpr std::uint8_t exposed = exposed_categories(descriptor);
};

// The result of walking a route whose operation the next implementation does
// not implement yet.
struct unsupported_route {};

namespace detail {
template <typename Type>
using remove_cvref_t = std::remove_cv_t<std::remove_reference_t<Type>>;

template <typename Binding, std::size_t Term, std::size_t Steps,
          std::size_t Step, typename Value>
decltype(auto) walk_step(Value &&value);

// Continues the walk with the result of a step that yields a value: the last
// step returns it directly, so it is constructed in place by the caller.
template <typename Binding, std::size_t Term, std::size_t Steps,
          std::size_t Step, typename Make>
decltype(auto) continue_with(Make &&make) {
  if constexpr (Step + 1 == Steps) {
    return make();
  } else {
    return walk_step<Binding, Term, Steps, Step + 1>(make());
  }
}
} // namespace detail

// Applies the first `Steps` rules of term `Term` of `Binding` to `value`, the
// stored object. A route that ends in an lvalue yields a reference, any other
// a value.
template <typename Binding, std::size_t Term,
          std::size_t Steps = Binding::published.items[Term].rule_count,
          typename Value>
decltype(auto) walk(Value &&value) {
  if constexpr (Steps == 0) {
    return static_cast<Value &&>(value);
  } else {
    return detail::walk_step<Binding, Term, Steps, 0>(
        std::forward<Value>(value));
  }
}

namespace detail {
template <typename Binding, std::size_t Term, std::size_t Steps,
          std::size_t Step, typename Value>
decltype(auto) walk_step(Value &&value) {
  constexpr const term &route = Binding::published.items[Term];
  using value_type = remove_cvref_t<Value>;
  constexpr const route_rule &rule =
      route_rules[static_cast<std::size_t>(route.rules[Step])];
  constexpr std::size_t argument = route.arguments[Step];
  using traits = layer_traits<value_type>;
  // A step that reaches an lvalue passes it on, the last one returns it.
  auto reach = [](auto &&reached) -> decltype(auto) {
    if constexpr (Step + 1 == Steps) {
      return static_cast<decltype(reached) &&>(reached);
    } else {
      return walk_step<Binding, Term, Steps, Step + 1>(
          static_cast<decltype(reached) &&>(reached));
    }
  };
  if constexpr (rule.upcast && rule.to_layer == layer_kind::leaf) {
    using interface = type_list_at_t<argument, typename Binding::interfaces>;
    return reach(static_cast<interface &>(value));
  } else if constexpr (rule.upcast && rule.to_layer == layer_kind::ptr) {
    using interface = type_list_at_t<argument, typename Binding::interfaces>;
    return continue_with<Binding, Term, Steps, Step>(
        [&] { return static_cast<interface *>(value); });
  } else if constexpr (rule.from_layer == layer_kind::ref && !rule.upcast) {
    // A reference layer is the object it aliases.
    return reach(value);
  } else if constexpr (rule.from_layer == layer_kind::alt &&
                       rule.to_layer == layer_kind::leaf &&
                       rule.from_category == value_category::lvalue) {
    return reach(traits::template element<argument>(value));
  } else if constexpr (rule.from_layer != layer_kind::leaf &&
                       rule.to_layer == layer_kind::leaf &&
                       rule.from_category == value_category::lvalue) {
    return reach(traits::element(value));
  } else if constexpr (rule.from_layer == layer_kind::leaf &&
                       rule.to_layer == layer_kind::leaf) {
    return continue_with<Binding, Term, Steps, Step>(
        [&] { return value_type(value); });
  } else if constexpr (rule.from_layer == layer_kind::leaf &&
                       rule.to_layer == layer_kind::ptr &&
                       rule.from_category == value_category::lvalue) {
    return continue_with<Binding, Term, Steps, Step>(
        [&] { return std::addressof(value); });
  } else if constexpr ((rule.from_layer == layer_kind::array ||
                        rule.from_layer == layer_kind::array_unbounded) &&
                       rule.to_layer == layer_kind::ptr &&
                       rule.from_category == value_category::lvalue) {
    return continue_with<Binding, Term, Steps, Step>([&] { return &value[0]; });
  } else {
    return unsupported_route{};
  }
}
} // namespace detail

// The number of rules of a term the binding walks before the erasure boundary
// of a runtime container: a last copy, address-of, or decay of a borrowed
// object depends only on the requested type and is applied by the request.
template <typename Binding, std::size_t Term>
constexpr std::size_t binding_steps() {
  constexpr const term &route = Binding::published.items[Term];
  if constexpr (route.rule_count == 0) {
    return 0;
  } else {
    constexpr rule_id last = route.rules[route.rule_count - 1];
    if constexpr (last == rule_id::copy || last == rule_id::address_of ||
                  last == rule_id::decay_array ||
                  last == rule_id::decay_unbounded_array) {
      return route.rule_count - 1;
    } else {
      return route.rule_count;
    }
  }
}

template <typename Binding, std::size_t Term>
using walk_result_t = decltype(walk<Binding, Term>(
    std::declval<typename Binding::stored_type &>()));

template <typename Binding, std::size_t Term>
inline constexpr bool walk_supported_v =
    !std::is_same_v<detail::remove_cvref_t<walk_result_t<Binding, Term>>,
                    unsupported_route>;

} // namespace dingo::core
