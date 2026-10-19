//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/runtime/lookup/index.h>
#include <dingo/runtime/memory.h>

#include <cassert>
#include <cstddef>
#include <limits>
#include <memory>
#include <vector>

namespace dingo::detail {

struct runtime_data_owner_t {};
inline constexpr runtime_data_owner_t runtime_data_owner{};

template <typename BindingInterface> struct runtime_lookup_binding_view {
  BindingInterface *binding = nullptr;
};

template <typename BindingInterface> class runtime_lookup_value {
public:
  using binding_interface_type = BindingInterface;
  using binding_view_type = runtime_lookup_binding_view<BindingInterface>;

  explicit runtime_lookup_value(binding_view_type view) : view_(view) {}

  runtime_lookup_value(runtime_lookup_value &&) noexcept = default;
  runtime_lookup_value &operator=(runtime_lookup_value &&) noexcept = default;
  runtime_lookup_value(const runtime_lookup_value &) = delete;
  runtime_lookup_value &operator=(const runtime_lookup_value &) = delete;

  BindingInterface &binding() { return *view_.binding; }

private:
  binding_view_type view_;
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

  container_runtime<Allocator> runtime;
  BindingsState bindings;
  std::vector<scope_state_type *, scope_pointer_allocator> scope_states;
};

template <typename Data, bool OwnsData> class runtime_data_holder;

template <typename Data> class runtime_data_holder<Data, true> {
public:
  template <typename Allocator>
  explicit runtime_data_holder(Allocator &allocator) : data_(allocator) {}

  Data &get() noexcept { return data_; }

  static constexpr std::size_t scope_id() noexcept { return 0; }

private:
  Data data_;
};

template <typename Data> class runtime_data_holder<Data, false> {
public:
  static constexpr std::size_t invalid_scope =
      std::numeric_limits<std::size_t>::max();

  runtime_data_holder() = default;

  std::size_t scope_id() const noexcept { return scope_id_; }
  void scope_id(std::size_t value) noexcept { scope_id_ = value; }

private:
  std::size_t scope_id_ = invalid_scope;
};

} // namespace dingo::detail
