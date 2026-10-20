//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

// The lookup declarations of a container: the cardinality of the domains of
// its interfaces and keys, one binding or many, and whether two bindings may
// share a key. spec: lookup_declarations

#include <dingo/core/base/mechanics.h>
#include <dingo/core/engine/derive.h>
#include <dingo/core/primitives/shape.h>
#include <dingo/core/projection/key.h>

#include <cstddef>

namespace dingo::core {

// A declaration of the domain of an interface and a key, or with a null
// interface the default of every domain no other declaration covers.
struct lookup_descriptor {
  type_id interface = nullptr;
  key_descriptor domain;
  bool many = false;
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

// A lookup declaration of the domains of the interfaces `Interfaces`, a
// type_list, and `Key`, a key of the domain, with `Many` bindings; an empty
// list declares the default of every other domain.
template <typename Interfaces, typename Key, bool Many>
struct lookup_declaration;

template <typename... Interfaces, typename Key, bool Many>
struct lookup_declaration<type_list<Interfaces...>, Key, Many> {
  static constexpr std::size_t size = sizeof...(Interfaces);
  static constexpr lookup_descriptor items[] = {
      {type_id_v<Interfaces>, Key::descriptor, Many}...};
};

template <typename Key, bool Many>
struct lookup_declaration<type_list<>, Key, Many> {
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
