//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/base/config.h>
#include <dingo/base/exceptions.h>
#include <dingo/base/none.h>
#include <dingo/base/type_list.h>
#include <dingo/binding/model.h>
#include <dingo/binding/selection.h>
#include <dingo/factory/auto_constructible.h>
#include <dingo/factory/callable.h>
#include <dingo/lookup/lookup.h>
#include <dingo/lookup/lookup_key.h>
#include <dingo/lookup/operations.h>
#include <dingo/memory/allocator.h>
#include <dingo/registration/requirements.h>
#include <dingo/request/collection.h>
#include <dingo/request/decomposition.h>
#include <dingo/resolution/algorithm.h>
#include <dingo/runtime/binding.h>
#include <dingo/runtime/container_traits.h>
#include <dingo/runtime/context.h>
#include <dingo/runtime/lookup_index.h>
#include <dingo/runtime/memory.h>
#include <dingo/runtime/transaction.h>
#include <dingo/storage/interface_storage_traits.h>
#include <dingo/type/annotated.h>
#include <dingo/type/collection_traits.h>
#include <dingo/type/shape.h>

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <functional>
#include <limits>
#include <map>
#include <memory>
#include <new>
#include <tuple>
#include <type_traits>
#include <typeindex>
#include <utility>
#include <variant>
#include <vector>

namespace dingo {
namespace detail {

template <typename ContainerTraits, typename Allocator,
          typename ParentContainer>
class runtime_container_impl;

// The binding a lookup entry selects.
template <typename BindingInterface> class runtime_lookup_value {
public:
  explicit runtime_lookup_value(BindingInterface *binding)
      : binding_(binding) {}

  runtime_lookup_value(runtime_lookup_value &&) noexcept = default;
  runtime_lookup_value &operator=(runtime_lookup_value &&) noexcept = default;
  runtime_lookup_value(const runtime_lookup_value &) = delete;
  runtime_lookup_value &operator=(const runtime_lookup_value &) = delete;

  BindingInterface &binding() { return *binding_; }

private:
  BindingInterface *binding_;
};

template <typename LookupEntries, typename BindingInterface, typename Allocator>
struct runtime_bindings_state {
  using value_type = runtime_lookup_value<BindingInterface>;

  explicit runtime_bindings_state(Allocator &allocator)
      : lookup_indexes(allocator) {}

  lookup_index<LookupEntries, value_type, Allocator> lookup_indexes;
};

template <typename BindingsState> struct runtime_scope_state {
  template <typename Allocator>
  runtime_scope_state(Allocator &allocator, void *parent)
      : bindings(allocator), registration_parent(parent) {}

  BindingsState bindings;
  void *registration_parent = nullptr;
};

template <typename Allocator, typename BindingsState>
struct runtime_registry_data {
  using scope_state_type = runtime_scope_state<BindingsState>;
  using scope_pointer_allocator = typename std::allocator_traits<
      Allocator>::template rebind_alloc<scope_state_type *>;

  explicit runtime_registry_data(Allocator &allocator)
      : runtime(allocator), bindings(allocator),
        scope_states(scope_pointer_allocator(allocator)) {}

  std::size_t create_scope(scope_state_type *state) {
    scope_states.push_back(state);
    return scope_states.size();
  }

  void release_scope(std::size_t scope, scope_state_type *state) noexcept {
    assert(scope > 0 && scope <= scope_states.size());
    assert(scope_states[scope - 1] == state);
    (void)state;
    scope_states[scope - 1] = nullptr;
    if (scope == scope_states.size()) {
      scope_states.pop_back();
    }
  }

  scope_state_type *find_scope(std::size_t scope) noexcept {
    assert(scope > 0 && scope <= scope_states.size());
    return scope_states[scope - 1];
  }

  runtime_memory<Allocator> runtime;
  BindingsState bindings;
  std::vector<scope_state_type *, scope_pointer_allocator> scope_states;
};

// The runtime data of a registry: the data a container owns, or, for a level
// of the local bindings of a registration, which borrows the data of its
// root, the scope of its bindings in that data and the level it falls back
// to. A level holds only one of them, and its registry knows which: a
// container is its own root.
template <typename Data, typename Level> class runtime_data_holder {
public:
  static constexpr std::size_t invalid_scope =
      std::numeric_limits<std::size_t>::max();

  template <typename Allocator> explicit runtime_data_holder(Allocator &alloc) {
    ::new (static_cast<void *>(std::addressof(data_))) Data(alloc);
  }

  explicit runtime_data_holder(Level *owner) noexcept {
    ::new (static_cast<void *>(std::addressof(local_))) local_state{owner};
  }

  runtime_data_holder(const runtime_data_holder &) = delete;
  runtime_data_holder &operator=(const runtime_data_holder &) = delete;

  // The union has no destructor of its own: the container destroys the data
  // it owns, by destroy().
  ~runtime_data_holder() {}

  void destroy(bool owns) noexcept {
    if (owns) {
      data_.~Data();
    }
  }

  Data &get() noexcept { return data_; }

  Level *owner() const noexcept { return local_.owner; }
  std::size_t scope_id() const noexcept { return local_.scope_id; }
  void scope_id(std::size_t value) noexcept { local_.scope_id = value; }

private:
  struct local_state {
    Level *owner;
    std::size_t scope_id = invalid_scope;
  };

  union {
    Data data_;
    local_state local_;
  };
};

template <typename Interface> struct key_value_interface {
  using type = normalized_type_t<Interface>;
};

template <> struct key_value_interface<void> {
  using type = void;
};

template <typename Interface>
using key_value_interface_t = typename key_value_interface<Interface>::type;

template <typename Container, typename Request, typename LookupKey,
          typename = void>
struct has_key_value_lookup_definition : std::false_type {};

template <typename Container, typename Request, typename LookupKey>
struct has_key_value_lookup_definition<
    Container, Request, LookupKey,
    std::void_t<typename Container::lookup_definition_type>>
    : std::bool_constant<!std::is_void_v<selected_lookup_entry_t<
          Request, typename std::decay_t<LookupKey>::definition_type,
          normalize_lookup_definitions_t<
              typename Container::lookup_definition_type>>>> {};

template <typename Container, typename Request, typename LookupKey>
inline constexpr bool has_key_value_lookup_definition_v =
    has_key_value_lookup_definition<Container, Request, LookupKey>::value;

template <typename Container, typename Request, typename LookupKey>
constexpr bool has_lookup() {
  static_assert(is_lookup_key_v<LookupKey>,
                "internal lookup queries require dingo::detail::lookup_key");
  using key_definition = typename std::decay_t<LookupKey>::definition_type;
  if constexpr (is_static_lookup_key_definition_v<key_definition>) {
    return true;
  } else {
    return has_key_value_lookup_definition_v<Container, Request, LookupKey>;
  }
}

} // namespace detail
} // namespace dingo
