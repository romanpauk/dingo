//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/base/config.h>
#include <dingo/request/binding_semantics.h>
#include <dingo/semantics/extension.h>

#include <cstdint>
#include <type_traits>

// The extension of a request from a layer source of a binding, derived as the
// code compiles of the request type alone: its context, its request and its
// nested sources, in the universe of its own layers, of which
// semantics/extension.h derives each extension.

namespace dingo::detail::execution {

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

  // The nested sources of the request.
  static constexpr semantics::nested_list make_nested() {
    if constexpr (modeled) {
      return semantics::nested_sources(goal(), context());
    } else {
      // GCC 13 rejects copy-list-initialization of this aggregate from {}.
      return semantics::nested_list{};
    }
  }
  static constexpr semantics::nested_list nested = make_nested();

  // The forms of a nested source of the request.
  static constexpr semantics::form_list
  forms(const semantics::nested_entry &group) {
    return semantics::nested_forms(group, goal(), context());
  }

  // The derivations of the extension of a nested source of a form, for both
  // lifetimes.
  static constexpr semantics::extension_derivations
  extended(const semantics::nested_entry &group, std::uint16_t code) {
    return semantics::nested_extension(group, code, context(), goal(),
                                       category::value);
  }
};

} // namespace dingo::detail::execution
