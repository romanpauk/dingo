//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

// The layer kind a C++ type is: the projection of the shape algebra of the spec
// onto C++ types. Raw pointers, references, and arrays are language layers.
// Wrappers are classified by layer_traits, which the standard wrappers
// specialize here and user wrappers through the public extension point; no
// wrapper gets other treatment.

#include <dingo/core/primitives/shape.h>
#include <dingo/core/vocabulary/vocabulary.gen.h>

#include <cstddef>
#include <memory>
#include <optional>
#include <type_traits>
#include <variant>

namespace dingo::core {

// A type that is not a layer is a leaf.
template <typename Type, typename = void> struct layer_traits {
  static constexpr layer_kind kind = layer_kind::leaf;
};

template <typename Type> struct layer_traits<Type *> {
  static constexpr layer_kind kind = layer_kind::ptr;
  using arguments = type_list<Type>;
};

template <typename Type> struct layer_traits<Type &> {
  static constexpr layer_kind kind = layer_kind::ref;
  using arguments = type_list<Type>;
};

template <typename Type, std::size_t Extent> struct layer_traits<Type[Extent]> {
  static constexpr layer_kind kind = layer_kind::array;
  static constexpr std::size_t extent = Extent;
  using arguments = type_list<Type>;
};

template <typename Type> struct layer_traits<Type[]> {
  static constexpr layer_kind kind = layer_kind::array_unbounded;
  using arguments = type_list<Type>;
};

// The family of the standard wrappers.
struct standard_family {};

template <typename Type> struct layer_traits<std::unique_ptr<Type>> {
  static constexpr layer_kind kind = layer_kind::own;
  using family = standard_family;
  using arguments = type_list<Type>;
};

template <typename Type> struct layer_traits<std::shared_ptr<Type>> {
  static constexpr layer_kind kind = layer_kind::share;
  using family = standard_family;
  using arguments = type_list<Type>;
};

template <typename Type> struct layer_traits<std::optional<Type>> {
  static constexpr layer_kind kind = layer_kind::opt;
  using family = standard_family;
  using arguments = type_list<Type>;
};

template <typename... Types> struct layer_traits<std::variant<Types...>> {
  static constexpr layer_kind kind = layer_kind::alt;
  using family = standard_family;
  using arguments = type_list<Types...>;
};

namespace detail {
template <typename Traits, typename = void> struct layer_family {
  static constexpr type_id value = nullptr;
};

template <typename Traits>
struct layer_family<Traits, std::void_t<typename Traits::family>> {
  static constexpr type_id value = type_id_v<typename Traits::family>;
};

template <typename Traits, typename = void> struct layer_extent {
  static constexpr std::uint32_t value = 0;
};

template <typename Traits>
struct layer_extent<Traits, std::void_t<decltype(Traits::extent)>> {
  static constexpr std::uint32_t value = Traits::extent;
};

template <typename Type> constexpr shape make_shape();

template <typename List> struct argument_shapes;

template <typename... Arguments>
struct argument_shapes<type_list<Arguments...>> {
  static constexpr void append(shape &result) {
    (append_one(result, make_shape<Arguments>()), ...);
  }

  static constexpr void append_one(shape &result, const shape &argument) {
    for (std::size_t index = 0; index < argument.size; ++index) {
      result.nodes[result.size + index] = argument.nodes[index];
    }
    result.size += argument.size;
  }
};

template <typename Type> constexpr shape make_shape() {
  using value_type = std::remove_cv_t<Type>;
  using traits = layer_traits<value_type>;
  shape result;
  shape_node &node = result.nodes[0];
  node.kind = static_cast<std::uint8_t>(traits::kind);
  node.type = type_id_v<value_type>;
  if constexpr (std::is_reference_v<value_type>) {
    node.copyable = false;
  } else {
    node.copyable = std::is_copy_constructible_v<value_type>;
  }
  node.constructible = !std::is_abstract_v<value_type>;
  result.size = 1;
  if constexpr (traits::kind != layer_kind::leaf) {
    node.family = layer_family<traits>::value;
    node.extent = layer_extent<traits>::value;
    node.arity = static_cast<std::uint8_t>(traits::arguments::size);
    argument_shapes<typename traits::arguments>::append(result);
  }
  return result;
}
} // namespace detail

// The shape of a C++ type; cv-qualification is not part of a shape.
template <typename Type>
inline constexpr shape shape_v = detail::make_shape<Type>();

} // namespace dingo::core
