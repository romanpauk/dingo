//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/base/config.h>
#include <dingo/request/classification.h>
#include <dingo/semantics/model.h>
#include <dingo/type/collection_traits.h>
#include <dingo/type/type_traits.h>

#include <cassert>
#include <cstdint>
#include <memory>
#include <type_traits>
#include <utility>

// The semantics of a binding of the types of what it stores: the stack of a
// type in a universe of layers, and the shape of a binding as the semantics
// know it, of no C++ type, which every type of the shape shares.

namespace dingo::detail::execution {

// The innermost type of a stack toward Leaf: the leaf, the base of it the
// stack holds, or another type.
template <typename T, typename Leaf> struct innermost {
  template <typename U> struct tag {
    using type = U;
  };
  static auto make() {
    using bare = std::remove_cv_t<T>;
    if constexpr (std::is_same_v<bare, Leaf>) {
      return tag<Leaf>{};
    } else if constexpr (std::is_pointer_v<bare>) {
      return tag<typename innermost<std::remove_pointer_t<bare>, Leaf>::type>{};
    } else if constexpr (std::is_array_v<bare>) {
      return tag<typename innermost<std::remove_extent_t<bare>, Leaf>::type>{};
    } else if constexpr (type_traits<bare>::enabled) {
      return tag<typename innermost<typename type_traits<bare>::value_type,
                                    Leaf>::type>{};
    } else if constexpr (is_alternative_type_v<bare>) {
      using held = typename alternatives_stack<
          Leaf, alternative_type_alternatives_t<bare>>::alternative;
      if constexpr (std::is_void_v<held>) {
        return tag<bare>{};
      } else {
        return tag<typename innermost<held, Leaf>::type>{};
      }
    } else {
      return tag<bare>{};
    }
  }
  using type = typename decltype(make())::type;
};

template <typename T, typename Leaf>
using innermost_t = typename innermost<T, Leaf>::type;

// The interface a request names: its innermost type, of no qualifiers, or
// the innermost raw array, which is an interface of its own; a handle of an
// array names its element.
template <typename T> struct request_interface {
  template <typename U> struct tag {
    using type = U;
  };
  static auto make() {
    using bare = std::remove_cv_t<T>;
    if constexpr (std::is_array_v<bare>) {
      return tag<std::remove_cv_t<bare>>{};
    } else if constexpr (std::is_pointer_v<bare>) {
      return tag<
          typename request_interface<std::remove_pointer_t<bare>>::type>{};
    } else if constexpr (type_traits<bare>::enabled) {
      return tag<typename request_interface<
          typename type_traits<bare>::value_type>::type>{};
    } else {
      return tag<bare>{};
    }
  }
  using type = typename decltype(make())::type;
};

template <typename T>
using request_interface_t = typename request_interface<T>::type;

// The layers of the owning handles a pointer the request owns may be taken
// over by: those of owning_handles, which a user adds a family to.
template <typename Handles> struct owning_layers;
template <typename... Handles> struct owning_layers<type_list<Handles...>> {
  using type = type_list<wrapper_layer_tag<Handles>...>;
};

template <typename List, typename Other> struct joined;
template <typename... Items, typename... Others>
struct joined<type_list<Items...>, type_list<Others...>> {
  using type = type_list<Items..., Others...>;
};

// The layers of a stack in a universe.
template <typename Universe, typename... Tags>
constexpr void place_layers(semantics::layer *layers, type_list<Tags...>) {
  std::size_t index = 0;
  ((layers[index++] = Universe::template index<Tags>()), ...);
}

constexpr semantics::element semantic_element(stack_element element) {
  return element == stack_element::abstract ? semantics::element::abstract
                                            : semantics::element::leaf;
}

// The shape of a binding as the semantics know it, of no C++ type: its
// context, the universe of its layers and its stored object, encoded as
// integers. The semantics of a binding are a template of its shape, so that
// a constant expression of them is evaluated once for every shape, not for
// every type of it.
struct shape_code {
  std::uint64_t entries[4 + 2 * semantics::max_kinds] = {};
  std::size_t count = 0;
};

constexpr std::uint64_t bit(bool value, unsigned index) {
  return static_cast<std::uint64_t>(value) << index;
}
constexpr bool test_bit(std::uint64_t code, unsigned index) {
  return ((code >> index) & 1u) != 0;
}

constexpr std::uint64_t encode_kind(const semantics::layer_kind &kind) {
  return static_cast<std::uint64_t>(kind.owns) | bit(kind.shares_element, 2) |
         bit(kind.copyable, 3) | bit(kind.wraps_value, 4) |
         bit(kind.holds_element_only, 5) | bit(kind.relocatable, 6) |
         bit(kind.gives_element, 7) | bit(kind.bounded, 8) |
         bit(kind.may_be_empty, 9) | bit(kind.has_empty_value, 10) |
         bit(kind.adopts, 11) | bit(kind.decays, 12) | bit(kind.adopted, 13) |
         bit(kind.adopted_array, 14) | bit(kind.rebinds, 15) |
         bit(kind.observes, 16) | bit(kind.qualifies, 17) |
         bit(kind.adopts_bounded, 18) |
         (static_cast<std::uint64_t>(kind.transfers) << 32);
}

constexpr semantics::layer_kind decode_kind(std::uint64_t code,
                                            std::uint64_t unknown_bound) {
  semantics::layer_kind made;
  made.owns = static_cast<semantics::owner>(code & 3u);
  made.shares_element = test_bit(code, 2);
  made.copyable = test_bit(code, 3);
  made.wraps_value = test_bit(code, 4);
  made.holds_element_only = test_bit(code, 5);
  made.relocatable = test_bit(code, 6);
  made.gives_element = test_bit(code, 7);
  made.bounded = test_bit(code, 8);
  made.may_be_empty = test_bit(code, 9);
  made.has_empty_value = test_bit(code, 10);
  made.adopts = test_bit(code, 11);
  made.decays = test_bit(code, 12);
  made.adopted = test_bit(code, 13);
  made.adopted_array = test_bit(code, 14);
  made.rebinds = test_bit(code, 15);
  made.observes = test_bit(code, 16);
  made.qualifies = test_bit(code, 17);
  made.adopts_bounded = test_bit(code, 18);
  made.transfers = static_cast<std::uint32_t>(code >> 32);
  made.converts_unknown_bound = static_cast<std::uint32_t>(unknown_bound);
  return made;
}

// The context of a binding, of no universe.
constexpr std::uint64_t encode_context(const semantics::context &context) {
  return bit(context.leaf.copyable, 0) | bit(context.leaf.movable, 1) |
         bit(context.leaf.default_constructible, 2) |
         bit(context.leaf.factory_made, 3) |
         bit(context.leaf.copies_volatile, 4) | bit(context.scope.makes, 5) |
         bit(context.scope.given, 6) | bit(context.scope.publishes_early, 7) |
         bit(context.abstract, 8) |
         (static_cast<std::uint64_t>(context.limits) << 16);
}

constexpr semantics::context decode_context(std::uint64_t code,
                                            const semantics::universe *layers) {
  return {{test_bit(code, 0), test_bit(code, 1), test_bit(code, 2),
           test_bit(code, 3), test_bit(code, 4)},
          {test_bit(code, 5), test_bit(code, 6), test_bit(code, 7)},
          test_bit(code, 8),
          static_cast<std::uint8_t>(code >> 16),
          layers};
}

// An object, in two integers.
constexpr std::uint64_t encode_object(const semantics::object &object) {
  std::uint64_t made = 0;
  for (std::size_t index = 0; index < semantics::max_layers; ++index) {
    made |= static_cast<std::uint64_t>(object.layers[index]) << (8 * index);
  }
  return made | (static_cast<std::uint64_t>(object.depth) << 48) |
         (static_cast<std::uint64_t>(object.element) << 52) |
         (static_cast<std::uint64_t>(object.origin) << 54) |
         bit(object.instance, 56) | bit(object.unconstructed, 57) |
         bit(object.absent, 58) | bit(object.owns_pointee, 59) |
         bit(object.made_layer, 60) | bit(object.qualified, 61);
}

constexpr std::uint64_t encode_object_detail(const semantics::object &object) {
  std::uint64_t made = 0;
  for (std::size_t index = 0; index <= semantics::max_layers; ++index) {
    made |= static_cast<std::uint64_t>(object.lifetimes[index]) << (2 * index);
    made |= static_cast<std::uint64_t>(object.quals[index] & 15u)
            << (14 + 4 * index);
  }
  return made | (static_cast<std::uint64_t>(object.contained) << 42) |
         (static_cast<std::uint64_t>(object.full & 15u) << 50);
}

constexpr semantics::object decode_object(std::uint64_t code,
                                          std::uint64_t detail) {
  semantics::object made;
  for (std::size_t index = 0; index < semantics::max_layers; ++index) {
    made.layers[index] = static_cast<semantics::layer>(code >> (8 * index));
  }
  made.depth = static_cast<std::uint8_t>((code >> 48) & 15u);
  made.element = static_cast<semantics::element>((code >> 52) & 3u);
  made.origin = static_cast<semantics::origin>((code >> 54) & 3u);
  made.instance = test_bit(code, 56);
  made.unconstructed = test_bit(code, 57);
  made.absent = test_bit(code, 58);
  made.owns_pointee = test_bit(code, 59);
  made.made_layer = test_bit(code, 60);
  made.qualified = test_bit(code, 61);
  for (std::size_t index = 0; index <= semantics::max_layers; ++index) {
    made.lifetimes[index] =
        static_cast<semantics::lifetime>((detail >> (2 * index)) & 3u);
    made.quals[index] =
        static_cast<std::uint8_t>((detail >> (14 + 4 * index)) & 15u);
  }
  made.contained = static_cast<semantics::layer>((detail >> 42) & 255u);
  made.full = static_cast<std::uint8_t>((detail >> 50) & 15u);
  return made;
}

constexpr shape_code encode_shape(const semantics::context &context,
                                  const semantics::object &start) {
  shape_code made;
  made.entries[made.count++] = encode_context(context);
  made.entries[made.count++] = encode_object(start);
  made.entries[made.count++] = encode_object_detail(start);
  made.entries[made.count++] = context.layers->count;
  for (std::size_t index = 0; index < context.layers->count; ++index) {
    made.entries[made.count++] = encode_kind(context.layers->kinds[index]);
    made.entries[made.count++] =
        context.layers->kinds[index].converts_unknown_bound;
  }
  return made;
}

// The semantics of a binding of a shape: what it publishes, and the layer
// sources of it closed under every step a request takes but a wrap, in the
// layers the binding knows, which a request reaches from.
template <std::uint64_t... Code> struct binding_shape {
  static constexpr std::uint64_t code[] = {Code...};
  static constexpr semantics::universe make_universe() {
    semantics::universe made;
    made.count = static_cast<std::size_t>(code[3]);
    for (std::size_t index = 0; index < made.count; ++index) {
      made.kinds[index] = decode_kind(code[4 + 2 * index], code[5 + 2 * index]);
    }
    return made;
  }
  static constexpr semantics::universe universe = make_universe();
  static constexpr semantics::context context =
      decode_context(code[0], &universe);
  static constexpr semantics::object start = decode_object(code[1], code[2]);
  static constexpr semantics::search sources =
      semantics::layer_sources(start, context);
  static constexpr semantics::search closed =
      semantics::closed_layer_sources(sources, context);
};

// The shape of the semantics of a binding, for a request of its leaf or of its
// abstract interface. It is made outside the semantics, which are complete by
// the time it is: MSVC does not evaluate a function of a class in the
// declarations of the class.
template <typename Semantics, bool Abstract> struct encoded_shape {
  static constexpr shape_code code =
      encode_shape(Semantics::context(Abstract), Semantics::start());
  template <std::size_t... I>
  static auto make(std::index_sequence<I...>)
      -> binding_shape<code.entries[I]...>;
  using type = decltype(make(std::make_index_sequence<code.count>{}));
};

// What a binding of a stored object publishes, derived as the code compiles,
// of no request: the layer sources of its stored object, in a universe of the
// layers it takes steps into, those it stores and those of the handles a
// pointer it makes may be taken over by. The stored object is made for the
// request by a scope that makes its instance (Makes), or kept, and it is
// published early by a scope that does (Early).
template <typename Stored, typename Leaf, bool Makes, bool Early>
struct binding_semantics {
  using stored_stack = stack_t<Stored, Leaf>;
  using layers = typename joined<
      typename stored_stack::layers,
      typename owning_layers<typename owning_handles<
          typename owning_handles_key<Leaf>::type>::type>::type>::type;
  using universe_type = layer_universe<layers>;

  static constexpr semantics::context context(bool abstract) {
    return {
        leaf_traits_v<Leaf>(),
        {Makes, false, Early},
        abstract,
        static_cast<std::uint8_t>(
            static_cast<unsigned>(semantics::limitation::made_holder_handover) |
            static_cast<unsigned>(
                semantics::limitation::converted_copy_pointer) |
            static_cast<unsigned>(
                semantics::limitation::qualified_below_element)),
        &universe_type::value};
  }

  static constexpr semantics::object start() {
    semantics::object made;
    place_layers<universe_type>(made.layers, typename stored_stack::layers{});
    made.depth = static_cast<std::uint8_t>(
        type_list_size_v<typename stored_stack::layers>);
    made.element = semantic_element(stored_stack::element);
    stack_quals<Stored, Leaf>::fill(made.quals);
    for (std::size_t index = 0; index <= made.depth; ++index) {
      made.lifetimes[index] =
          Makes ? semantics::lifetime::request : semantics::lifetime::stable;
    }
    made.origin = Makes ? semantics::origin::made : semantics::origin::borrowed;
    made.instance = true;
    // A pointer a scope makes owns what it points to.
    made.owns_pointee =
        Makes && made.depth && made.layers[0] == semantics::pointer_layer;
    return made;
  }

  // The shape of the binding, for a request of the leaf, and for one of its
  // abstract interface.
  template <bool Abstract>
  using shape = typename encoded_shape<binding_semantics, Abstract>::type;

  // What the binding publishes for a request of the leaf, and for one of its
  // abstract interface, of its shape. Each is whole: a stored object of more
  // layer sources than a search of them holds does not compile.
  template <bool Abstract>
  static constexpr const semantics::search &sources = shape<Abstract>::sources;
  // The layer sources closed under every step a request takes but a wrap,
  // in the layers the binding knows, which a request reaches from.
  template <bool Abstract>
  static constexpr const semantics::search &closed = shape<Abstract>::closed;
  template <bool Abstract> static constexpr bool whole() {
    static_assert(!sources<Abstract>.overflow && !closed<Abstract>.overflow,
                  "the stored object publishes more layer sources than "
                  "semantics::max_states");
    return true;
  }
};

} // namespace dingo::detail::execution
