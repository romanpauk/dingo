//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/base/config.h>

#include <dingo/base/construction_scope.h>
#include <dingo/base/type_descriptor.h>
#include <dingo/binding/interface.h>
#include <dingo/binding/resolution.h>
#include <dingo/request/conversion.h>
#include <dingo/runtime/conversion.h>
#include <dingo/storage/retention.h>

#include <cassert>
#include <cstddef>
#include <memory>
#include <type_traits>
#include <utility>

// A request as the runtime container resolves it: what a resolution makes and
// keeps, the conversions of a request type for each binding, and the value the
// request takes of what a conversion reached.

namespace dingo {
namespace detail {
// What a runtime resolution makes: kept with the binding when it lives as
// long as the container, in the context in the scope of the request
// otherwise.
template <typename Context>
class context_store final : public execution::runtime_store {
public:
  // The types a failure names are those of the request and of the binding,
  // which live as long as the program.
  context_store(Context &context, construction_scope scope, retention *retained,
                const type_descriptor &requested,
                const type_descriptor &registered) noexcept
      : context_(context), scope_(scope), retained_(retained),
        requested_(&requested), registered_(&registered) {}

  // Rejects a request of what an empty layer does not hold.
  [[noreturn]] void absent() override {
    throw_type_not_convertible(*requested_, *registered_, context_);
  }

  // Keeps an object of a layout, which construct constructs of make: with
  // the binding when it is stable, in the context in the scope of the
  // request otherwise.
  DINGO_NOINLINE void *keep(bool stable, const object_layout &layout,
                            execution::construct_function construct,
                            void *make) override {
    auto place_made = [&](void *place) { construct(place, make); };
    if (stable) {
      // A binding that keeps no instance is converted to keep nothing made of
      // it.
      assert(retained_ != nullptr);
      return retained_->keep(&layout, context_, layout, place_made);
    }
    return context_.place(scope_, layout, place_made);
  }

  // The same, of a type a typed route names: what lives as long as the
  // request, in the context, of this store; what lives as long as the
  // container, with the binding, of part<true>(). Each is kept as an erased
  // one is, by the layout of its type and a function that constructs it, so
  // that a type a route keeps adds no code of the store of its own.
  template <bool Stable, typename T, typename Make> T &keep_made(Make &make) {
    return *static_cast<T *>(keep(
        Stable, object_layout_v<T>,
        &execution::construct_made<T, std::remove_reference_t<Make>>,
        const_cast<void *>(static_cast<const void *>(std::addressof(make)))));
  }
  template <typename T, typename Make> T &keep(Make &&make) {
    return keep_made<false, T>(make);
  }

  struct stable_store {
    context_store &store;
    [[noreturn]] void absent() { store.absent(); }
    template <typename T, typename Make> T &keep(Make &&make) {
      return store.template keep_made<true, T>(make);
    }
  };

  template <bool Stable> auto part() {
    if constexpr (Stable) {
      return stable_store{*this};
    } else {
      return transient_store{*this};
    }
  }

  struct transient_store {
    context_store &store;
    [[noreturn]] void absent() { store.absent(); }
    template <typename T, typename Make> T &keep(Make &&make) {
      return store.template keep<T>(std::forward<Make>(make));
    }
  };

private:
  Context &context_;
  construction_scope scope_;
  retention *retained_;
  const type_descriptor *requested_;
  const type_descriptor *registered_;
};

struct runtime_request;
struct selected_conversion;

// The selection of the extension of a request of the fewest copies and moves
// of the leaf, of the layer sources of a binding, for a request that needs
// what it refers to to live as long as the container (stable) or not.
using extension_selection =
    selected_conversion (*)(const execution::binding_layout &layout,
                            const runtime_request &request, bool stable);

// A request of type T as a runtime resolution converts it: the interface it
// names, the type it is of, how it takes it, and its extensions of the layer
// sources of a binding of the leaf, and of an abstract interface of it, with
// their selection; none of a request that has no extension, so that a
// program that resolves none compiles no selection.
struct runtime_request {
  type_descriptor requested;
  // The key of the interface the request names, which only a binding of it
  // resolves.
  const void *interface_key;
  bool modeled;
  // The key of the type the request is of, without its own qualifiers, the
  // category it takes it in and its own qualifiers.
  const void *shape;
  semantics::category category;
  std::uint8_t quals;
  const execution::extension_table *extensions[2];
  extension_selection extend;
};

// The address of what satisfies a request of the shape Shape, of what a
// conversion reaches: the object a request refers to, or what a pointer it
// reached points to; what a request takes or moves, which it consumes; a copy
// is made of the object. A value whose last step the request makes is made of
// what the conversion reached before it.
inline resolved_address
finish_request(execution::erased_carrier reached,
               const execution::runtime_conversion &conversion,
               const execution::type_table *source_table) {
  using access = resolved_address::access_kind;
  using made_kind = resolved_address::made_kind;
  switch (conversion.last) {
  case execution::finish::copy:
    return {reached.object, access::borrow, made_kind::copy, 0,
            conversion.quals};
  case execution::finish::run:
    return {reached.object, access::consume, made_kind::run, conversion.depth,
            0};
  default:
    break;
  }
  switch (conversion.access) {
  case semantics::access::point:
    // What no step converted points of the type of the layer source.
    return {(conversion.count ? conversion.reached : source_table)
                ->pointee(reached.object),
            access::borrow};
  case semantics::access::take:
  case semantics::access::move:
    return {reached.object, access::consume};
  default:
    return {reached.object, access::borrow};
  }
}

// The conversion a request of a binding is resolved by: the answer of the
// closure of the binding, as the code compiled it, when it has one for the
// request; else the extension of the request, as its code compiled it, of
// a layer source of the binding.
struct selected_conversion {
  const execution::closed_answer *answer;
  const execution::runtime_conversion *derived;
  // The state of the closure of the binding a derived conversion extends, and
  // how the request takes what its route reaches.
  std::uint16_t route;
  resolved_address (*finish)(execution::erased_carrier reached,
                             const execution::runtime_conversion &derived,
                             const execution::type_table *source_table,
                             execution::runtime_store &store);

  bool stable_address() const noexcept {
    return derived != nullptr ? derived->stable_address
                              : answer->stable_address;
  }
};

// The address of what satisfies a request of an extension, of what the route
// of the binding reached: after the calls the extension takes after the
// route, as finish_request takes it; of the table of the type of the layer
// source, when it takes none.
DINGO_NOINLINE inline resolved_address
finish_derived(execution::erased_carrier reached,
               const execution::runtime_conversion &derived,
               const execution::type_table *source_table,
               execution::runtime_store &store) {
  for (std::size_t index = 0; index < derived.count; ++index) {
    const auto &call = derived.calls[index];
    reached = (reached.owned ? call.owned : call.borrowed)(reached, call.stable,
                                                           store);
  }
  return finish_request(reached, derived, source_table);
}

// Selects the extension of a request of the fewest copies and moves of the
// leaf, of the layer sources of a binding; none when no layer source of the
// binding has one.
DINGO_NOINLINE inline selected_conversion
select_extension(const execution::binding_layout &layout,
                 const runtime_request &request, bool stable) {
  const semantics::lifetime needed =
      stable ? semantics::lifetime::stable : semantics::lifetime::request;
  const execution::extension_table &table =
      *request.extensions[layout.abstract ? 1 : 0];
  selected_conversion chosen{nullptr, nullptr, 0, &finish_derived};
  semantics::extension_cost fewest;
  for (std::size_t group = 0; group < table.group_count; ++group) {
    const execution::extension_group &nested = table.groups[group];
    for (std::size_t index = 0; index < layout.layer_source_count; ++index) {
      const execution::layer_source &source = layout.layer_sources[index];
      if (source.shape != nested.shape) {
        continue;
      }
      for (std::size_t entry = nested.first;
           entry < nested.first + nested.count; ++entry) {
        const execution::extension_entry &each = table.entries[entry];
        if (each.form != source.code || each.conversions[needed] == nullptr) {
          continue;
        }
        if (fewest.consider(source.made, source.absent, each.fewest[needed],
                            each.absent[needed])) {
          chosen.derived = each.conversions[needed];
          chosen.route = source.state;
        }
      }
    }
  }
  return chosen;
}

// The address of what satisfies a request of an answer of a closure, of the
// object the route of the answer reached, as the answer records it.
inline resolved_address finish_answer(void *reached,
                                      const execution::closed_answer &answer,
                                      const execution::binding_layout &layout) {
  using access = resolved_address::access_kind;
  switch (answer.access) {
  case semantics::access::point:
    return {layout.tables[answer.route]->pointee(reached), access::borrow};
  case semantics::access::take:
  case semantics::access::move:
    return {reached, access::consume};
  default:
    return {reached, access::borrow};
  }
}

// The C++ properties of a request type, as a runtime resolution converts it.
template <typename T> struct request_execution {
  using category = requested_category<T>;
  using shape = typename category::shape;
  // The innermost type of the request: what a binding of its interface
  // stores the leaf of, or is the leaf as its interface.
  using element_type = execution::innermost_t<shape, void>;
  using stack = stack_t<shape, element_type>;
  static constexpr bool modeled =
      category::modeled && stack::element != stack_element::other;
  // Whether a binding of a leaf of another type may bind the element as its
  // abstract interface: not of a complete type that is no class or is a final
  // one.
  static constexpr bool abstract_possible() {
    if constexpr (is_complete_v<element_type>) {
      return std::is_class_v<element_type> && !std::is_final_v<element_type>;
    } else {
      return true;
    }
  }
};

// The extensions of a request type of the abstract interface of a leaf: none
// of an element no leaf of another type is bound as.
template <typename T>
constexpr const execution::extension_table *abstract_extensions() {
  if constexpr (request_execution<T>::abstract_possible()) {
    return execution::runtime_extensions<T, true>::table();
  } else {
    return &execution::no_extensions;
  }
}

template <typename T> constexpr runtime_request make_runtime_request() {
  using executed = request_execution<T>;
  if constexpr (!executed::modeled) {
    return {type_descriptor_v<T>, nullptr, false, nullptr, {}, 0, {}, nullptr};
  } else {
    using shape = typename executed::shape;
    return {type_descriptor_v<T>,
            &execution::shape_key<execution::request_interface_t<shape>>,
            true,
            &execution::shape_key<std::remove_cv_t<shape>>,
            executed::category::value,
            quals_v<shape>,
            {execution::runtime_extensions<T, false>::table(),
             abstract_extensions<T>()},
            execution::runtime_extensions<T, false>::table() !=
                        &execution::no_extensions ||
                    abstract_extensions<T>() != &execution::no_extensions
                ? &select_extension
                : nullptr};
  }
}

template <typename T>
inline constexpr runtime_request runtime_request_v = make_runtime_request<T>();
} // namespace detail

namespace detail {
// A value of T made of the value at an address, of the type the request
// spells at a depth K, moved into all the layers above it at once.
template <typename T, typename Element, typename Types, std::size_t K>
T wrap_nested(void *address) {
  // The value is moved into what holds it of the qualifiers it needs.
  using nested_type =
      std::remove_cv_t<typename Types::template nested<K>::type>;
  return execution::wrap_value<Element, Element, T>(
      std::move(*static_cast<nested_type *>(address)));
}

// A value of T made of the run of wraps the conversion left to it: the makers
// of the depths a run of wraps makes T of.
template <typename T, typename Element, typename Types, std::size_t K>
constexpr T (*nested_maker())(void *) {
  if constexpr (execution::layer_types<T, Element,
                                       Element>::template runs<K>()) {
    return &wrap_nested<T, Element, Types, K>;
  } else {
    return nullptr;
  }
}

template <typename T, typename Element, typename Types, std::size_t... K>
T wrap_result(resolved_address result, std::index_sequence<K...>) {
  using maker = T (*)(void *);
  constexpr maker makers[] = {nested_maker<T, Element, Types, K + 1>()...};
  return makers[result.depth - 1](result.address);
}

// The value a request of T takes, of what a conversion reached: made where it
// is returned, of a copy or of a run of wraps the conversion left to it, or as
// resolved_value gives it.
template <typename T> T returned_value(resolved_address result) {
  using made_kind = resolved_address::made_kind;
  if constexpr (std::is_reference_v<T> || std::is_pointer_v<T>) {
    return resolved_value<T>(result);
  } else {
    using value_type = std::remove_cv_t<T>;
    if (result.made == made_kind::copy) {
      if constexpr (std::is_constructible_v<value_type,
                                            const volatile value_type &>) {
        if (result.quals & semantics::volatile_qual) {
          return value_type(
              *static_cast<const volatile value_type *>(result.address));
        }
      }
      if constexpr (is_copy_constructible_v<value_type>) {
        return value_type(*static_cast<const value_type *>(result.address));
      }
    }
    using executed = request_execution<T>;
    using types = execution::request_types<T, typename executed::element_type>;
    if constexpr (types::depth != 0) {
      if (result.made == made_kind::run) {
        return wrap_result<value_type, typename executed::element_type, types>(
            result, std::make_index_sequence<types::depth>{});
      }
    }
    return resolved_value<T>(result);
  }
}
} // namespace detail

} // namespace dingo
