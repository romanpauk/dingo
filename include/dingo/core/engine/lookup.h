//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

// The lookup declarations of a container: the cardinality of the domains of
// its interfaces and keys, one binding or many, whether two bindings may
// share a key, and the bound of the key values a dense array indexes.
// spec: lookup_declarations, index_structures

#include <dingo/core/base/mechanics.h>
#include <dingo/core/engine/derive.h>
#include <dingo/core/primitives/shape.h>
#include <dingo/core/projection/key.h>

#include <cstddef>

namespace dingo::core {

// The bound of a domain whose key values no dense array indexes.
inline constexpr std::size_t unbounded = static_cast<std::size_t>(-1);

// A declaration of the domain of an interface and a key, or with a null
// interface the default of every domain no other declaration covers. A domain
// indexed by a dense array stores the key values 0 up to `bound`, excluded.
struct lookup_descriptor {
  type_id interface = nullptr;
  key_descriptor domain;
  bool many = false;
  std::size_t bound = unbounded;
};

struct lookup_table {
  static constexpr std::size_t capacity = 32;
  lookup_descriptor items[capacity] = {};
  std::size_t size = 0;
};

// Whether the domain of `interface` and `key` holds many bindings: by its
// declaration, or by the default declaration, which holds one unless it is
// declared many.
constexpr bool holds_many(const lookup_table &table, type_id interface,
                          const key_descriptor &key) {
  bool many = false;
  for (std::size_t index = 0; index < table.size; ++index) {
    const lookup_descriptor &declaration = table.items[index];
    if (declaration.interface == nullptr) {
      many = declaration.many;
    } else if (declaration.interface == interface &&
               same_key_domain(declaration.domain, key)) {
      return declaration.many;
    }
  }
  return many;
}

// Whether a binding of the key of `added` conflicts with `existing`: they
// share an interface whose domain holds one binding. The values of their keys
// are compared by the backend.
constexpr bool conflicts(const lookup_table &table,
                         const binding_descriptor &existing,
                         const binding_descriptor &added) {
  if (!same_key_domain(existing.key, added.key)) {
    return false;
  }
  auto conflict_on = [&](type_id interface) {
    return serves_interface(existing, interface) &&
           !holds_many(table, interface, added.key);
  };
  if (added.interface_count == 0) {
    return conflict_on(innermost_leaf(added.stored));
  }
  for (std::size_t index = 0; index < added.interface_count; ++index) {
    if (conflict_on(added.interfaces[index].nodes[0].type)) {
      return true;
    }
  }
  return false;
}

// The bound of the key values of the domain of a binding: the least bound of
// the declarations of the domains of its interfaces, or of its leaf without
// interfaces.
constexpr std::size_t key_bound(const lookup_table &table,
                                const binding_descriptor &binding) {
  auto bound_of = [&](type_id interface) {
    for (std::size_t index = 0; index < table.size; ++index) {
      const lookup_descriptor &declaration = table.items[index];
      if (declaration.interface == interface &&
          same_key_domain(declaration.domain, binding.key)) {
        return declaration.bound;
      }
    }
    return unbounded;
  };
  if (binding.interface_count == 0) {
    return bound_of(innermost_leaf(binding.stored));
  }
  std::size_t bound = unbounded;
  for (std::size_t index = 0; index < binding.interface_count; ++index) {
    const std::size_t of = bound_of(binding.interfaces[index].nodes[0].type);
    bound = of < bound ? of : bound;
  }
  return bound;
}

// A lookup declaration of the domains of the interfaces `Interfaces`, a
// type_list, and `Key`, a key of the domain, with `Many` bindings, whose key
// values a dense array of `Bound` indexes unless it is unbounded; an empty
// list declares the default of every other domain.
template <typename Interfaces, typename Key, bool Many,
          std::size_t Bound = unbounded>
struct lookup_declaration;

template <typename... Interfaces, typename Key, bool Many, std::size_t Bound>
struct lookup_declaration<type_list<Interfaces...>, Key, Many, Bound> {
  static constexpr std::size_t size = sizeof...(Interfaces);
  static constexpr lookup_descriptor items[] = {
      {type_id_v<Interfaces>, Key::descriptor, Many, Bound}...};
};

template <typename Key, bool Many, std::size_t Bound>
struct lookup_declaration<type_list<>, Key, Many, Bound> {
  static constexpr std::size_t size = 1;
  static constexpr lookup_descriptor items[] = {{nullptr, {}, Many}};
};

template <typename Declarations> struct lookup_table_of;

template <typename... Declarations>
struct lookup_table_of<type_list<Declarations...>> {
  static constexpr lookup_table make() {
    lookup_table table;
    (append<Declarations>(table), ...);
    return table;
  }

  template <typename Declaration>
  static constexpr void append(lookup_table &table) {
    for (std::size_t index = 0; index < Declaration::size; ++index) {
      table.items[table.size++] = Declaration::items[index];
    }
  }

  static constexpr lookup_table value = make();
};

} // namespace dingo::core
