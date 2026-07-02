//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#include <dingo/container.h>
#include <dingo/storage/shared.h>

#include <gtest/gtest.h>

#include <cstddef>
#include <memory>
#include <type_traits>
#include <utility>

namespace dingo {
namespace {

struct size_service {};

struct size_interface {
  virtual ~size_interface() = default;
};

struct size_implementation : size_interface {};

struct size_dependency {};

size_dependency make_size_dependency() { return {}; }

using size_registry_type = typename container<>::registry_type;

struct size_probe_registry : size_registry_type {
  using base_type = size_registry_type;

  using typename base_type::default_lookup_entry;
  using typename base_type::lookup_index_entries;
  using typename base_type::runtime_binding_value;
  using typename base_type::runtime_bindings_state;
  using typename base_type::runtime_lookup_binding_view;
  using typename base_type::runtime_lookup_value;

  template <typename Interface, typename Storage, typename BindingState,
            typename Key>
  using binding = typename base_type::template runtime_registration_binding_t<
      Interface, Storage, BindingState, Key>;

  template <typename Interfaces, typename Storage, typename BindingState,
            typename Key>
  using owner = typename base_type::template runtime_binding_value_owner_t<
      Interfaces, Storage, BindingState, Key>;
};

using size_registration =
    type_registration<scope<shared>, storage<size_implementation>,
                      interfaces<size_interface>>;
using size_model = detail::binding_model<size_registration>;
using size_storage = typename size_model::storage_type;
using size_instance_container =
    runtime_registry<dynamic_container_traits,
                     typename dynamic_container_traits::allocator_type,
                     container<>, container<>>;
using size_binding_state =
    detail::runtime_binding_state_t<container<>, size_instance_container,
                                    size_storage,
                                    typename size_model::bindings_type>;
using size_binding = size_probe_registry::binding<size_interface, size_storage,
                                                  size_binding_state, none_t>;
using size_single_owner =
    size_probe_registry::owner<type_list<size_interface>, size_storage,
                               size_binding_state, none_t>;
using size_shared_state_owner =
    size_probe_registry::owner<type_list<size_interface, size_implementation>,
                               size_storage,
                               std::shared_ptr<size_binding_state>, none_t>;

using size_type_index = typename size_registry_type::rtti_type::type_index;
using size_type_map_value = size_probe_registry::runtime_lookup_value;
using size_type_map_entry =
    std::pair<const size_type_index, size_type_map_value>;
using size_type_map =
    dynamic_type_map<size_type_map_value,
                     typename size_registry_type::rtti_type,
                     typename size_registry_type::allocator_type>;
using size_static_type_map_node =
    static_type_map_node<size_type_map_value, struct size_type_map_tag>;

using size_default_lookup_key =
    detail::default_lookup_key<typename size_registry_type::rtti_type>;
using size_default_lookup_backend =
    detail::lookup_backend<size_probe_registry::default_lookup_entry,
                           size_type_map_value,
                           typename size_registry_type::allocator_type>;
using size_runtime_lookup_entry =
    detail::lookup_entry<size_interface, runtime_key<int>, one, ordered>;
using size_runtime_lookup_backend =
    detail::lookup_backend<size_runtime_lookup_entry, size_type_map_value,
                           typename size_registry_type::allocator_type>;
using size_no_key_lookup_entry =
    detail::lookup_entry<size_interface, no_key, one, ordered>;
using size_no_key_lookup_backend =
    detail::lookup_backend<size_no_key_lookup_entry, size_type_map_value,
                           typename size_registry_type::allocator_type>;
using size_typed_key_lookup_entry =
    detail::lookup_entry<size_interface, typed_key<int>, many, ordered>;
using size_typed_key_lookup_backend =
    detail::lookup_backend<size_typed_key_lookup_entry, size_type_map_value,
                           typename size_registry_type::allocator_type>;
using size_lookup_index =
    detail::lookup_index<size_probe_registry::lookup_index_entries,
                         size_type_map_value,
                         typename size_registry_type::allocator_type>;

using size_local_binding = bind<scope<shared>, storage<size_dependency>,
                                factory<function<make_size_dependency>>>;
using size_local_registration =
    type_registration<scope<shared>, storage<size_service>,
                      bindings<size_local_binding>>;
using size_local_model = detail::binding_model<size_local_registration>;
using size_resolution_container =
    detail::runtime_binding_resolution_container_t<
        container<>, container<>, typename size_local_model::bindings_type>;

template <typename T>
void expect_size_at_most(const char *name, std::size_t expected) {
  SCOPED_TRACE(name);
  EXPECT_LE(sizeof(T), expected);
}

} // namespace

TEST(object_sizes_test, runtime_container_and_lookup_sizes) {
  if constexpr (sizeof(void *) == 8) {
    expect_size_at_most<container<>>("container<>", 64);
    expect_size_at_most<size_registry_type>("runtime registry", 56);
    expect_size_at_most<size_probe_registry::runtime_bindings_state>(
        "runtime bindings state", 48);

    expect_size_at_most<detail::context_closure>("context closure", 128);
    expect_size_at_most<size_binding_state>("runtime binding state", 176);
    expect_size_at_most<size_binding>("runtime binding", 192);
    expect_size_at_most<size_single_owner>("single-interface binding owner",
                                           208);
    expect_size_at_most<size_shared_state_owner>(
        "multi-interface shared-state binding owner", 56);

    expect_size_at_most<size_probe_registry::runtime_lookup_binding_view>(
        "runtime lookup binding view", 8);
    expect_size_at_most<size_probe_registry::runtime_binding_value>(
        "runtime binding value base", 8);
    expect_size_at_most<size_probe_registry::runtime_lookup_value>(
        "runtime lookup value", 32);

    expect_size_at_most<size_type_index>("type index", 8);
    expect_size_at_most<size_default_lookup_key>("default lookup key", 16);
    expect_size_at_most<size_type_map_entry>("dynamic type map entry", 40);
    expect_size_at_most<size_type_map>("dynamic type map", 48);
    expect_size_at_most<size_static_type_map_node>("static type map node", 56);

    expect_size_at_most<size_default_lookup_backend>("default lookup backend",
                                                     48);
    expect_size_at_most<size_runtime_lookup_backend>(
        "runtime-key lookup backend", 48);
    expect_size_at_most<size_no_key_lookup_backend>("no-key lookup backend",
                                                    40);
    expect_size_at_most<size_typed_key_lookup_backend>(
        "typed-key lookup backend", 24);
    expect_size_at_most<size_lookup_index>("lookup index", 48);
  }

  static_assert(sizeof(size_resolution_container) <
                sizeof(detail::binding_scope<size_local_binding>));
  static_assert(sizeof(size_default_lookup_key) == sizeof(size_type_index) * 2);
  static_assert(sizeof(size_probe_registry::runtime_lookup_binding_view) ==
                sizeof(void *));
  static_assert(sizeof(size_probe_registry::runtime_binding_value) ==
                sizeof(void *));
  static_assert(sizeof(size_probe_registry::runtime_lookup_value) >=
                sizeof(size_probe_registry::runtime_lookup_binding_view));
}

} // namespace dingo
