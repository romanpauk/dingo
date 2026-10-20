//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

// Shapes of C++ types as constexpr values. A shape is a leaf wrapped in layers;
// it is stored in preorder, so a layer is followed by the shapes of its
// arguments. Every layer kind of a wrapper carries the identity of the
// wrapper's family, so two wrappers of the same kind stay apart, while a
// family of any_family stands for a layer a route creates.

#include <dingo/core/base/mechanics.h>

#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace dingo::core {

// The identity of a type at runtime: the address of its tag.
template <typename Type>
inline constexpr const void *type_address_v = &type_tag<Type>::value;

// A type identity that compares by type name in constant evaluation and by
// address at runtime, for a compiler that cannot compare the addresses of
// distinct objects in a constant expression. A null identity has an empty
// name.
class named_type_id {
public:
  constexpr named_type_id(std::nullptr_t /*null*/ = nullptr) {}
  constexpr named_type_id(const void *address, const char *name)
      : address_(address), name_(name) {}

  friend constexpr bool operator==(const named_type_id &left,
                                   const named_type_id &right) {
    if (constant_evaluated()) {
      return same_name(left.name_, right.name_);
    }
    return left.address_ == right.address_;
  }

  friend constexpr bool operator!=(const named_type_id &left,
                                   const named_type_id &right) {
    return !(left == right);
  }

private:
  static constexpr bool same_name(const char *left, const char *right) {
    std::size_t index = 0;
    while (left[index] != '\0' && left[index] == right[index]) {
      ++index;
    }
    return left[index] == right[index];
  }

  const void *address_ = nullptr;
  const char *name_ = "";
};

// The identity of a type as a constexpr value, which compares for equality in
// constant expressions: the address of its tag, or a named identity.
using type_id = std::conditional_t<named_type_ids, named_type_id, const void *>;

namespace detail {
template <typename Type, bool Named = named_type_ids> struct type_id_of {
  static constexpr const void *value = type_address_v<Type>;
};

template <typename Type> struct type_id_of<Type, true> {
  static constexpr named_type_id value{type_address_v<Type>, type_name<Type>()};
};
} // namespace detail

template <typename Type>
inline constexpr type_id type_id_v = detail::type_id_of<Type>::value;

// The family of a layer that a route creates, which a wrapper of any family
// providing the layer kind serves.
struct any_family_tag {};
inline constexpr type_id any_family = type_id_v<any_family_tag>;

inline constexpr std::size_t shape_capacity = 16;

struct shape_node {
  // layer_kind of the node, held as its value so the primitives do not depend
  // on the generated vocabulary.
  std::uint8_t kind = 0;
  std::uint8_t arity = 0;
  std::uint32_t extent = 0;
  type_id family = nullptr;
  // The type of a leaf; a layer is identified by its kind and family.
  type_id type = nullptr;
  bool copyable = false;
  // Whether a leaf can be constructed by a factory: it is not abstract.
  bool constructible = true;
  // The cv-qualifiers of the object of the node, const_qualified and
  // volatile_qualified; an array has those of its elements.
  std::uint8_t qualifiers = 0;
};

inline constexpr std::uint8_t const_qualified = 1;
inline constexpr std::uint8_t volatile_qualified = 2;

struct shape {
  shape_node nodes[shape_capacity] = {};
  std::size_t size = 0;
};

// The number of nodes of the subtree that starts at `index`.
constexpr std::size_t subtree_size(const shape &value, std::size_t index) {
  std::size_t end = index + 1;
  for (std::size_t pending = value.nodes[index].arity; pending > 0; --pending) {
    end += subtree_size(value, end);
  }
  return end - index;
}

// The subtree of argument `argument` of the layer at `index`.
constexpr std::size_t argument_index(const shape &value, std::size_t index,
                                     std::size_t argument) {
  std::size_t position = index + 1;
  for (std::size_t skipped = 0; skipped < argument; ++skipped) {
    position += subtree_size(value, position);
  }
  return position;
}

constexpr shape subtree(const shape &value, std::size_t index) {
  shape result;
  result.size = subtree_size(value, index);
  for (std::size_t offset = 0; offset < result.size; ++offset) {
    result.nodes[offset] = value.nodes[index + offset];
  }
  return result;
}

// Whether `derived` serves `requested`: the same nodes, where any_family
// matches any family.
constexpr bool shapes_match(const shape &derived, const shape &requested) {
  if (derived.size != requested.size) {
    return false;
  }
  for (std::size_t index = 0; index < derived.size; ++index) {
    const shape_node &d = derived.nodes[index];
    const shape_node &r = requested.nodes[index];
    if (d.kind != r.kind || d.arity != r.arity || d.extent != r.extent) {
      return false;
    }
    if (d.kind == 0) {
      if (d.type != r.type) {
        return false;
      }
    } else if (d.family != r.family && d.family != any_family &&
               r.family != any_family) {
      return false;
    }
  }
  return true;
}

constexpr bool operator==(const shape &left, const shape &right) {
  if (left.size != right.size) {
    return false;
  }
  for (std::size_t index = 0; index < left.size; ++index) {
    const shape_node &l = left.nodes[index];
    const shape_node &r = right.nodes[index];
    if (l.kind != r.kind || l.arity != r.arity || l.extent != r.extent ||
        l.family != r.family || l.type != r.type || l.copyable != r.copyable ||
        l.constructible != r.constructible || l.qualifiers != r.qualifiers) {
      return false;
    }
  }
  return true;
}

constexpr bool operator!=(const shape &left, const shape &right) {
  return !(left == right);
}

// A layer of `kind` of `family` applied to `argument`.
constexpr shape wrap(std::uint8_t kind, type_id family, bool copyable,
                     const shape &argument, std::uint32_t extent = 0) {
  shape result;
  result.size = argument.size + 1;
  result.nodes[0] =
      shape_node{kind, 1, extent, family, nullptr, copyable, true, 0};
  for (std::size_t index = 0; index < argument.size; ++index) {
    result.nodes[index + 1] = argument.nodes[index];
  }
  return result;
}

// The shape with the leaf of its first-argument chain replaced by `leaf`.
constexpr shape replace_leaf(const shape &value, const shape &leaf) {
  shape result;
  std::size_t index = 0;
  while (value.nodes[index].kind != 0) {
    result.nodes[index] = value.nodes[index];
    // The rest of a variadic layer is not on the chain and is kept.
    if (value.nodes[index].arity != 1) {
      return value;
    }
    ++index;
  }
  for (std::size_t offset = 0; offset < leaf.size; ++offset) {
    result.nodes[index + offset] = leaf.nodes[offset];
  }
  result.size = index + leaf.size;
  return result;
}

} // namespace dingo::core
