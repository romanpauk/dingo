//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

// Type trees of C++ types as constexpr values. A type tree is a leaf wrapped in
// wrappers; it is stored in preorder, so a wrapper is followed by the type
// trees of its arguments. A wrapper is of a wrapper semantics, which the type
// trees know by its descriptor: its identity, properties, and forms. Every
// wrapper of a wrapper carries the identity of the wrapper's family, so two
// wrappers of the same kind stay apart, while a family of any_family stands for
// a wrapper a route creates.

#include <dingo/base/mechanics.h>

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

// The family of a wrapper that a route creates, which a wrapper of any family
// providing the wrapper semantics serves.
struct any_family_tag {};
inline constexpr type_id any_family = type_id_v<any_family_tag>;

inline constexpr std::size_t type_tree_capacity = 16;
inline constexpr std::size_t max_semantics_operations = 32;

// A wrapper semantics: the identity of its tag, its property bits, the
// route forms it has, by bit, and for each form that leads to another kind, by
// bit of `targeted`, that kind. A leaf is of the leaf kind, which is no
// wrapper. Kinds compare by their identity, never by address, which a compiler
// may not compare in a constant expression.
struct semantics_descriptor {
  type_id id = nullptr;
  bool wrapper = false;
  std::uint16_t properties = 0;
  std::uint32_t forms = 0;
  std::uint32_t targeted = 0;
  const semantics_descriptor *targets[max_semantics_operations] = {};
};

inline constexpr semantics_descriptor leaf_semantics{};

constexpr bool is_wrapper(const semantics_descriptor *kind) {
  return kind->wrapper;
}

// Whether two wrappers are of one kind, or are both leaves.
constexpr bool same_semantics(const semantics_descriptor *left,
                              const semantics_descriptor *right) {
  if (!left->wrapper || !right->wrapper) {
    return left->wrapper == right->wrapper;
  }
  return left->id == right->id;
}

struct type_tree_node {
  // The kind of a wrapper, or the leaf kind.
  const semantics_descriptor *kind = &leaf_semantics;
  std::uint8_t arity = 0;
  std::uint32_t extent = 0;
  type_id family = nullptr;
  // The type of a leaf; a wrapper is identified by its kind and family.
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

struct type_tree {
  type_tree_node nodes[type_tree_capacity] = {};
  std::size_t size = 0;
};

// The number of nodes of the subtree that starts at `index`.
constexpr std::size_t subtree_size(const type_tree &value, std::size_t index) {
  std::size_t end = index + 1;
  for (std::size_t pending = value.nodes[index].arity; pending > 0; --pending) {
    end += subtree_size(value, end);
  }
  return end - index;
}

// The subtree of argument `argument` of the wrapper at `index`.
constexpr std::size_t argument_index(const type_tree &value, std::size_t index,
                                     std::size_t argument) {
  std::size_t position = index + 1;
  for (std::size_t skipped = 0; skipped < argument; ++skipped) {
    position += subtree_size(value, position);
  }
  return position;
}

constexpr type_tree subtree(const type_tree &value, std::size_t index) {
  type_tree result;
  result.size = subtree_size(value, index);
  for (std::size_t offset = 0; offset < result.size; ++offset) {
    result.nodes[offset] = value.nodes[index + offset];
  }
  return result;
}

// Whether `derived` serves `requested`: the same nodes, where any_family
// matches any family.
constexpr bool type_trees_match(const type_tree &derived,
                                const type_tree &requested) {
  if (derived.size != requested.size) {
    return false;
  }
  for (std::size_t index = 0; index < derived.size; ++index) {
    const type_tree_node &d = derived.nodes[index];
    const type_tree_node &r = requested.nodes[index];
    if (!same_semantics(d.kind, r.kind) || d.arity != r.arity ||
        d.extent != r.extent) {
      return false;
    }
    if (!is_wrapper(d.kind)) {
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

constexpr bool operator==(const type_tree &left, const type_tree &right) {
  if (left.size != right.size) {
    return false;
  }
  for (std::size_t index = 0; index < left.size; ++index) {
    const type_tree_node &l = left.nodes[index];
    const type_tree_node &r = right.nodes[index];
    if (!same_semantics(l.kind, r.kind) || l.arity != r.arity ||
        l.extent != r.extent || l.family != r.family || l.type != r.type ||
        l.copyable != r.copyable || l.constructible != r.constructible ||
        l.qualifiers != r.qualifiers) {
      return false;
    }
  }
  return true;
}

constexpr bool operator!=(const type_tree &left, const type_tree &right) {
  return !(left == right);
}

// A wrapper of `kind` of `family` applied to `argument`.
constexpr type_tree wrap(const semantics_descriptor *kind, type_id family,
                         bool copyable, const type_tree &argument,
                         std::uint32_t extent = 0) {
  type_tree result;
  result.size = argument.size + 1;
  result.nodes[0] =
      type_tree_node{kind, 1, extent, family, nullptr, copyable, true, 0};
  for (std::size_t index = 0; index < argument.size; ++index) {
    result.nodes[index + 1] = argument.nodes[index];
  }
  return result;
}

// The type tree with the leaf of its first-argument chain replaced by `leaf`.
constexpr type_tree replace_leaf(const type_tree &value,
                                 const type_tree &leaf) {
  type_tree result;
  std::size_t index = 0;
  while (is_wrapper(value.nodes[index].kind)) {
    result.nodes[index] = value.nodes[index];
    // The rest of a variadic wrapper is not on the chain and is kept.
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
