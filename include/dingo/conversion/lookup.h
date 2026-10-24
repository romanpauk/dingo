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

#include <dingo/base/mechanics.h>
#include <dingo/conversion/key.h>
#include <dingo/conversion/route.h>
#include <dingo/diagnostics/compile_diagnostics.gen.h>
#include <dingo/primitives/type_tree.h>

#include <cstddef>

namespace dingo::core {

// The bound of a domain whose key values no dense array indexes.
inline constexpr std::size_t unbounded = static_cast<std::size_t>(-1);

// A declaration of the domain of an interface and a key, or with a null
// interface the default of every domain no other declaration covers. A domain
// whose index is a dense array stores the key values 0 up to `bound`,
// excluded; the index itself is a type of the declaration.
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

// The declaration of the domain of `interface` and `key`: its own, or the
// default declaration, or none, which indexes its key values in order.
constexpr lookup_descriptor declaration_of(const lookup_table &table,
                                           type_id interface,
                                           const key_descriptor &key) {
  lookup_descriptor found;
  for (std::size_t index = 0; index < table.size; ++index) {
    const lookup_descriptor &declaration = table.items[index];
    if (declaration.interface == nullptr) {
      found = declaration;
    } else if (declaration.interface == interface &&
               same_key_domain(declaration.domain, key)) {
      return declaration;
    }
  }
  return found;
}

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
  if (added.interface_count == 0) {
    for (std::size_t index = 0; index < added.leaves.size; ++index) {
      if (conflict_on(added.leaves.items[index])) {
        return true;
      }
    }
    return false;
  }
  for (std::size_t index = 0; index < added.interface_count; ++index) {
    if (conflict_on(added.interfaces[index].nodes[0].type)) {
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
  for (std::size_t index = 0; index < added.alias_count; ++index) {
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
  if (binding.interface_count == 0) {
    for (std::size_t index = 0; index < binding.leaves.size; ++index) {
      const std::size_t of = bound_of(binding.leaves.items[index]);
      bound = of < bound ? of : bound;
    }
    return bound;
  }
  for (std::size_t index = 0; index < binding.interface_count; ++index) {
    const std::size_t of = bound_of(binding.interfaces[index].nodes[0].type);
    bound = of < bound ? of : bound;
  }
  return bound;
}

// A lookup declaration of the domains of the interfaces `Interfaces`, a
// type_list, and `Key`, a key of the domain, with `Many` bindings, whose key
// values the index of `Indexing` indexes, a type with a member template
// index<Value, Candidate> or void for the default, and whose key values are
// 0 up to `Bound`, excluded, unless it is unbounded; an empty list declares
// the default of every other domain.
template <typename Interfaces, typename Key, bool Many,
          typename Indexing = void, std::size_t Bound = unbounded>
struct lookup_declaration;

template <typename... Interfaces, typename Key, bool Many, typename Indexing,
          std::size_t Bound>
struct lookup_declaration<type_list<Interfaces...>, Key, Many, Indexing,
                          Bound> {
  using indexing = Indexing;
  using interface_list = type_list<Interfaces...>;
  using key_type = Key;
  static constexpr std::size_t size = sizeof...(Interfaces);
  static constexpr lookup_descriptor items[] = {
      {type_id_v<Interfaces>, Key::descriptor, Many, Bound}...};
};

template <typename Key, bool Many, typename Indexing, std::size_t Bound>
struct lookup_declaration<type_list<>, Key, Many, Indexing, Bound> {
  using indexing = Indexing;
  using interface_list = type_list<>;
  using key_type = Key;
  static constexpr std::size_t size = 1;
  static constexpr lookup_descriptor items[] = {{nullptr, {}, Many, Bound}};
};

// The position in `Declarations` of the declaration of the domain of
// `interface` and `key`, as declaration_of() finds it: its own, or the last
// default declaration, or the size of the list.
template <typename... Declarations>
constexpr std::size_t declaration_position(type_id interface,
                                           const key_descriptor &key) {
  constexpr std::size_t count = sizeof...(Declarations);
  constexpr std::size_t sizes[] = {Declarations::size..., 0};
  constexpr const lookup_descriptor *items[] = {Declarations::items...,
                                                nullptr};
  std::size_t found = count;
  for (std::size_t position = 0; position < count; ++position) {
    for (std::size_t index = 0; index < sizes[position]; ++index) {
      const lookup_descriptor &declaration = items[position][index];
      if (declaration.interface == nullptr) {
        found = position;
      } else if (declaration.interface == interface &&
                 same_key_domain(declaration.domain, key)) {
        return position;
      }
    }
  }
  return found;
}

// The lookup declarations `Declarations`, a type_list: the position of the
// declaration of a domain, and the indexing of the declaration at a position,
// void past the end, where the default indexes the key values.
template <typename Declarations> struct declaration_list;

template <typename... Declarations>
struct declaration_list<type_list<Declarations...>> {
  static constexpr std::size_t position(type_id interface,
                                        const key_descriptor &key) {
    return declaration_position<Declarations...>(interface, key);
  }

  template <std::size_t Position, bool = (Position < sizeof...(Declarations))>
  struct at {
    using indexing = typename pack_at_t<Position, Declarations...>::indexing;
  };

  template <std::size_t Position> struct at<Position, false> {
    using indexing = void;
  };
};

template <typename Declarations> struct lookup_table_of;

template <typename... Declarations>
struct lookup_table_of<type_list<Declarations...>> {
  // The declaration and the interface of the first domain of an interface
  // and a key that a later declaration declares again, or none.
  struct repeated {
    std::size_t declaration = sizeof...(Declarations);
    std::size_t interface = 0;
  };

  static constexpr std::size_t sizes[sizeof...(Declarations) + 1] = {
      Declarations::interface_list::size..., 0};
  static constexpr const lookup_descriptor *items[sizeof...(Declarations) + 1] =
      {Declarations::items..., nullptr};

  // Whether a declaration after `first` declares the domain `declared` again.
  static constexpr bool declared_later(std::size_t first,
                                       const lookup_descriptor &declared) {
    for (std::size_t later = first + 1; later < sizeof...(Declarations);
         ++later) {
      for (std::size_t other = 0; other < sizes[later]; ++other) {
        const lookup_descriptor &again = items[later][other];
        if (again.interface == declared.interface &&
            same_key_domain(again.domain, declared.domain)) {
          return true;
        }
      }
    }
    return false;
  }

  static constexpr repeated find_repeated() {
    for (std::size_t first = 0; first < sizeof...(Declarations); ++first) {
      for (std::size_t index = 0; index < sizes[first]; ++index) {
        if (declared_later(first, items[first][index])) {
          return {first, index};
        }
      }
    }
    return {};
  }

  // Reports a domain declared twice. spec: diagnostics.lookup_declared_twice
  static constexpr bool declared_once() {
    constexpr repeated found = find_repeated();
    if constexpr (found.declaration != sizeof...(Declarations)) {
      using declaration = pack_at_t<found.declaration, Declarations...>;
      fail<
          diag::lookup_declared_twice,
          type_list_at_t<found.interface, typename declaration::interface_list>,
          typename detail::key_type_of<typename declaration::key_type>::type>();
    }
    return true;
  }
  static_assert(declared_once());

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
