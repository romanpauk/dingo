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
//
// A specialization names the layer kind, the wrapper family, and the argument
// types, and provides `rebind`, the same layer of other argument types,
// `element`, which reaches the element of a layer object, and `make`, which
// makes a layer object from the arguments of its element.

#include <dingo/core/base/mechanics.h>
#include <dingo/core/primitives/shape.h>
#include <dingo/core/vocabulary/vocabulary.gen.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <type_traits>
#include <utility>
#include <variant>

namespace dingo {

// The public extension point that makes a wrapper type a layer of the shape
// algebra. A specialization declares
//
//   kind       the layer_kind of the wrapper: own, share, opt, or alt
//   family     a type naming the family of the wrapper; a route converts a
//              wrapper only to another wrapper of its family, such as a
//              sole-owner handle to a sharing handle
//   arguments  the element types, as a type_list
//   rebind     the wrapper of other element types
//   element    the element of a wrapper object
//   make       a wrapper object made from the arguments of its element
//
// The standard wrappers specialize it below, as a user wrapper does.
template <typename Wrapper, typename = void> struct wrapper_traits {};

using layer_kind = core::layer_kind;

template <typename... Types> using type_list = core::type_list<Types...>;

// The family of the standard wrappers.
struct standard_family {};

template <typename Type> struct wrapper_traits<std::unique_ptr<Type>> {
  static constexpr layer_kind kind = layer_kind::own;
  using family = standard_family;
  using arguments = type_list<Type>;
  template <typename Element> using rebind = std::unique_ptr<Element>;

  static Type &element(const std::unique_ptr<Type> &value) { return *value; }

  template <typename... Args>
  static std::unique_ptr<Type> make(Args &&...args) {
    return std::make_unique<Type>(std::forward<Args>(args)...);
  }
};

// A handle of an array of runtime size: its element is the array.
template <typename Type> struct wrapper_traits<std::unique_ptr<Type[]>> {
  static constexpr layer_kind kind = layer_kind::own;
  using family = standard_family;
  using arguments = type_list<Type[]>;
  template <typename Element> using rebind = std::unique_ptr<Element>;
  using array_type = Type[];

  static array_type &element(const std::unique_ptr<Type[]> &value) {
    return *reinterpret_cast<array_type *>(value.get());
  }
};

template <typename Type> struct wrapper_traits<std::shared_ptr<Type[]>> {
  static constexpr layer_kind kind = layer_kind::share;
  using family = standard_family;
  using arguments = type_list<Type[]>;
  template <typename Element> using rebind = std::shared_ptr<Element>;
  using array_type = Type[];

  static array_type &element(const std::shared_ptr<Type[]> &value) {
    return *reinterpret_cast<array_type *>(value.get());
  }
};

template <typename Type> struct wrapper_traits<std::shared_ptr<Type>> {
  static constexpr layer_kind kind = layer_kind::share;
  using family = standard_family;
  using arguments = type_list<Type>;
  template <typename Element> using rebind = std::shared_ptr<Element>;

  static Type &element(const std::shared_ptr<Type> &value) { return *value; }

  template <typename... Args>
  static std::shared_ptr<Type> make(Args &&...args) {
    return std::make_shared<Type>(std::forward<Args>(args)...);
  }
};

template <typename Type> struct wrapper_traits<std::optional<Type>> {
  static constexpr layer_kind kind = layer_kind::opt;
  using family = standard_family;
  using arguments = type_list<Type>;
  template <typename Element> using rebind = std::optional<Element>;

  static Type &element(std::optional<Type> &value) { return *value; }
  static Type &&element(std::optional<Type> &&value) {
    return std::move(*value);
  }

  template <typename... Args> static std::optional<Type> make(Args &&...args) {
    return std::optional<Type>(std::in_place, std::forward<Args>(args)...);
  }
};

template <typename... Types> struct wrapper_traits<std::variant<Types...>> {
  static constexpr layer_kind kind = layer_kind::alt;
  using family = standard_family;
  using arguments = type_list<Types...>;
  template <typename... Elements> using rebind = std::variant<Elements...>;

  template <std::size_t Index>
  static auto &element(std::variant<Types...> &value) {
    return std::get<Index>(value);
  }
  template <std::size_t Index>
  static auto &&element(std::variant<Types...> &&value) {
    return std::get<Index>(std::move(value));
  }

  template <typename... Args>
  static std::variant<Types...> make(Args &&...args) {
    return std::variant<Types...>(std::in_place_index<0>,
                                  std::forward<Args>(args)...);
  }
};

} // namespace dingo

namespace dingo::core {

// A type that is not a layer is a leaf.
template <typename Type, typename = void> struct layer_traits {
  static constexpr layer_kind kind = layer_kind::leaf;
};

// A wrapper is the layer its wrapper_traits declare.
template <typename Type>
struct layer_traits<Type,
                    std::void_t<decltype(::dingo::wrapper_traits<Type>::kind)>>
    : ::dingo::wrapper_traits<Type> {};

template <typename Type> struct layer_traits<Type *> {
  static constexpr layer_kind kind = layer_kind::ptr;
  using arguments = type_list<Type>;
  template <typename Element> using rebind = Element *;

  static Type &element(Type *value) { return *value; }

  template <typename... Args> static Type *make(Args &&...args) {
    return new Type(std::forward<Args>(args)...);
  }
};

template <typename Type> struct layer_traits<Type &> {
  static constexpr layer_kind kind = layer_kind::ref;
  using arguments = type_list<Type>;
  template <typename Element> using rebind = Element &;

  static Type &element(Type &value) { return value; }
};

template <typename Type, std::size_t Extent> struct layer_traits<Type[Extent]> {
  static constexpr layer_kind kind = layer_kind::array;
  static constexpr std::size_t extent = Extent;
  using arguments = type_list<Type>;
  template <typename Element> using rebind = Element[Extent];
};

template <typename Type> struct layer_traits<Type[]> {
  static constexpr layer_kind kind = layer_kind::array_unbounded;
  using arguments = type_list<Type>;
  template <typename Element> using rebind = Element[];
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
  // Only a leaf has an identity of its own; a layer is its kind and family.
  if constexpr (traits::kind == layer_kind::leaf) {
    node.type = type_id_v<value_type>;
  }
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

namespace detail {
template <typename Type, typename = void> struct canonical {
  using type = std::remove_cv_t<Type>;
};

template <typename Traits, typename List> struct canonical_rebind;

template <typename Traits, typename... Arguments>
struct canonical_rebind<Traits, type_list<Arguments...>> {
  using type =
      typename Traits::template rebind<typename canonical<Arguments>::type...>;
};

template <typename Type>
struct canonical<Type,
                 std::enable_if_t<layer_traits<std::remove_cv_t<Type>>::kind !=
                                  layer_kind::leaf>> {
  using traits = layer_traits<std::remove_cv_t<Type>>;
  using type =
      typename canonical_rebind<traits, typename traits::arguments>::type;
};
} // namespace detail

// The type a shape describes: the type with cv-qualification removed at every
// layer. Two types of the same shape have the same canonical type.
template <typename Type>
using canonical_t = typename detail::canonical<Type>::type;

} // namespace dingo::core
