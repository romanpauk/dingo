//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#include <dingo/container.h>
#include <dingo/resolution/conversion_cache.h>
#include <dingo/runtime/container_runtime.h>
#include <dingo/runtime/transaction.h>
#include <dingo/storage/external.h>
#include <dingo/storage/shared.h>
#include <dingo/storage/shared_cyclical.h>
#include <dingo/storage/unique.h>

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

struct size_second_interface {
  virtual ~size_second_interface() = default;
};

struct size_implementation : size_interface {};

struct size_multi_implementation : size_interface, size_second_interface {};

struct size_dependency {};

size_dependency make_size_dependency() { return {}; }

using size_registry_type = typename container<>::registry_type;

struct size_probe_registry : size_registry_type {
  using base_type = size_registry_type;

  using typename base_type::base_lookup_entry;
  using typename base_type::lookup_index_entries;
  using typename base_type::runtime_binding_value;
  using typename base_type::runtime_bindings_state;
  using typename base_type::runtime_lookup_binding_view;
  using typename base_type::runtime_lookup_value;

  template <typename Interface, typename Storage, typename BindingState,
            typename LookupKey>
  using binding = typename base_type::template runtime_registration_binding_t<
      Interface, Storage, BindingState, LookupKey>;

  template <typename Interfaces, typename Storage, typename BindingState,
            typename LookupKey>
  using owner = typename base_type::template runtime_binding_value_owner_t<
      Interfaces, Storage, BindingState, LookupKey>;
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
    detail::runtime_binding_state_t<size_registry_type, size_instance_container,
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
using size_multi_interface_registration =
    type_registration<scope<shared>,
                      storage<std::shared_ptr<size_multi_implementation>>,
                      interfaces<size_interface, size_second_interface>>;
using size_multi_interface_model =
    detail::binding_model<size_multi_interface_registration>;
using size_multi_interface_storage =
    typename size_multi_interface_model::storage_type;
using size_multi_interface_conversion_types =
    detail::runtime_binding_conversion_types_t<size_interface,
                                               size_multi_interface_storage>;
using size_multi_interface_conversion_cache =
    conversion_cache<size_multi_interface_conversion_types>;
using size_multi_interface_state = detail::runtime_binding_state_t<
    size_registry_type, size_instance_container, size_multi_interface_storage,
    typename size_multi_interface_model::bindings_type>;
using size_multi_interface_pointer_state_owner = size_probe_registry::owner<
    typename size_multi_interface_model::interface_types,
    size_multi_interface_storage, size_multi_interface_state *, none_t>;
using size_multi_interface_shared_state_owner = size_probe_registry::owner<
    typename size_multi_interface_model::interface_types,
    size_multi_interface_storage, std::shared_ptr<size_multi_interface_state>,
    none_t>;

using size_type_index = typename size_registry_type::rtti_type::type_index;
using size_type_map_value = size_probe_registry::runtime_lookup_value;

using size_base_lookup_key =
    detail::base_lookup_key<typename size_registry_type::rtti_type>;
using size_base_lookup_backend =
    detail::lookup_backend<size_probe_registry::base_lookup_entry,
                           size_type_map_value,
                           typename size_registry_type::allocator_type>;
using size_runtime_lookup_entry =
    detail::lookup_entry<size_interface, detail::lookup_key<key_value<int>>,
                         one, ordered>;
using size_runtime_lookup_backend =
    detail::lookup_backend<size_runtime_lookup_entry, size_type_map_value,
                           typename size_registry_type::allocator_type>;
using size_no_key_lookup_entry =
    detail::lookup_entry<size_interface, detail::no_lookup_key_t, one, ordered>;
using size_no_key_lookup_backend =
    detail::lookup_backend<size_no_key_lookup_entry, size_type_map_value,
                           typename size_registry_type::allocator_type>;
using size_typed_key_lookup_entry =
    detail::lookup_entry<size_interface, detail::lookup_key<key_type<int>>,
                         many, ordered>;
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

template <typename Registration>
using size_runtime_state_for = detail::runtime_binding_state_t<
    size_registry_type, size_instance_container,
    typename detail::binding_model<Registration>::storage_type,
    typename detail::binding_model<Registration>::bindings_type>;

using size_unique_state = size_runtime_state_for<
    type_registration<scope<unique>, storage<size_implementation>>>;
using size_external_state = size_runtime_state_for<
    type_registration<scope<external>, storage<size_implementation &>>>;
using size_shared_ptr_state = size_runtime_state_for<type_registration<
    scope<shared>, storage<std::shared_ptr<size_implementation>>>>;
using size_shared_cyclical_state = size_runtime_state_for<type_registration<
    scope<shared_cyclical>, storage<std::shared_ptr<size_implementation>>>>;
using size_local_state = size_runtime_state_for<size_local_registration>;

template <typename T>
void expect_size_at_most(const char *name, std::size_t expected) {
  SCOPED_TRACE(name);
  EXPECT_LE(sizeof(T), expected);
}

} // namespace

TEST(object_sizes_test, runtime_container_and_lookup_sizes) {
  if constexpr (sizeof(void *) == 8) {
    expect_size_at_most<container<>>("container<>", 104);
    expect_size_at_most<size_registry_type>("runtime registry", 96);
    expect_size_at_most<size_probe_registry::runtime_bindings_state>(
        "runtime bindings state", 48);
    expect_size_at_most<
        container_runtime<typename size_registry_type::allocator_type>>(
        "container runtime", 40);
    expect_size_at_most<runtime_transaction>("runtime transaction", 72);
    // Runtime context carries one hidden construction-policy stack head.
    expect_size_at_most<runtime_context>("runtime context", 40);
    expect_size_at_most<size_storage>("shared registration storage", 16);
    // Shared runtime state keeps storage/container pointers; retained source
    // data now lives in the runtime transaction journal instead of a binding
    // owned lifetime frame.
    expect_size_at_most<size_binding_state>("runtime binding state", 40);
    expect_size_at_most<size_unique_state>("unique runtime binding state", 24);
    expect_size_at_most<size_external_state>("external runtime binding state",
                                             32);
    expect_size_at_most<size_shared_ptr_state>(
        "shared_ptr runtime binding state", 48);
    expect_size_at_most<size_shared_cyclical_state>(
        "shared_cyclical runtime binding state", 56);
    expect_size_at_most<size_local_state>(
        "local-bindings runtime binding state", 40);
    expect_size_at_most<size_multi_interface_state>(
        "multi-interface runtime binding state", 48);
    expect_size_at_most<size_multi_interface_conversion_cache>(
        "pointer-backed conversion cache", 16);
    expect_size_at_most<size_binding>("runtime binding", 48);
    expect_size_at_most<size_single_owner>("single-interface binding owner",
                                           56);
    expect_size_at_most<size_multi_interface_pointer_state_owner>(
        "multi-interface pointer-state binding owner", 72);
    expect_size_at_most<size_multi_interface_shared_state_owner>(
        "multi-interface shared-state binding owner", 88);
    expect_size_at_most<size_shared_state_owner>(
        "single-interface shared-state binding owner", 56);

    expect_size_at_most<size_probe_registry::runtime_lookup_binding_view>(
        "runtime lookup binding view", 8);
    expect_size_at_most<size_probe_registry::runtime_binding_value>(
        "runtime binding value base", 8);
    expect_size_at_most<size_probe_registry::runtime_lookup_value>(
        "runtime lookup value", 16);

    expect_size_at_most<size_type_index>("type index", 8);
    expect_size_at_most<size_base_lookup_key>("base lookup key", 16);

    expect_size_at_most<size_base_lookup_backend>("base lookup backend", 48);
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
  static_assert(!std::is_polymorphic_v<size_storage>);
  static_assert(sizeof(size_multi_interface_conversion_cache) <=
                sizeof(void *) *
                    type_list_size_v<size_multi_interface_conversion_types>);
  static_assert(sizeof(size_base_lookup_key) == sizeof(size_type_index) * 2);
  static_assert(sizeof(size_probe_registry::runtime_lookup_binding_view) ==
                sizeof(void *));
  static_assert(sizeof(size_probe_registry::runtime_binding_value) ==
                sizeof(void *));
  static_assert(sizeof(size_probe_registry::runtime_lookup_value) >=
                sizeof(size_probe_registry::runtime_lookup_binding_view));
}

} // namespace dingo
