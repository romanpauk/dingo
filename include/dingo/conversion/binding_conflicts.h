//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

// Where two bindings collide in the lookup declarations of a container: they
// share an interface whose domain holds one binding, and the bound of the key
// values of the domain of a binding. A backend asks it when it registers a
// binding.

#include <dingo/conversion/route.h>
#include <dingo/lookup/declarations.h>

#include <cstddef>

namespace dingo {

// Whether a binding of the key of `added` conflicts with `existing`: they
// share an interface whose domain holds one binding. The values of their keys
// are compared by the backend.
// Whether two bindings conflict in the key domain `key` of `added`.
constexpr bool conflicts_in(const lookup_table &table,
                            const binding_descriptor &existing,
                            const binding_descriptor &added,
                            const key_descriptor &key) {
  if (!has_key_domain(existing, key)) {
    return false;
  }
  auto conflict_on = [&](type_id interface) {
    return serves_interface(existing, interface) &&
           !holds_many(table, interface, key);
  };
  if (added.interfaces.size == 0) {
    for (std::size_t index = 0; index < added.leaves.size; ++index) {
      if (conflict_on(added.leaves.items[index])) {
        return true;
      }
    }
    return false;
  }
  for (std::size_t index = 0; index < added.interfaces.size; ++index) {
    if (conflict_on(added.interfaces[index].tree.nodes[0].type)) {
      return true;
    }
  }
  return false;
}

constexpr bool conflicts(const lookup_table &table,
                         const binding_descriptor &existing,
                         const binding_descriptor &added) {
  if (conflicts_in(table, existing, added, added.key)) {
    return true;
  }
  for (std::size_t index = 0; index < added.aliases.size; ++index) {
    if (conflicts_in(table, existing, added, added.aliases[index])) {
      return true;
    }
  }
  return false;
}

// The bound of the key values of the domain of a binding: the least bound of
// the declarations of the domains of its interfaces, or of the leaves its
// stored type tree names without interfaces.
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
  std::size_t bound = unbounded;
  if (binding.interfaces.size == 0) {
    for (std::size_t index = 0; index < binding.leaves.size; ++index) {
      const std::size_t of = bound_of(binding.leaves.items[index]);
      bound = of < bound ? of : bound;
    }
    return bound;
  }
  for (std::size_t index = 0; index < binding.interfaces.size; ++index) {
    const std::size_t of =
        bound_of(binding.interfaces[index].tree.nodes[0].type);
    bound = of < bound ? of : bound;
  }
  return bound;
}

} // namespace dingo
