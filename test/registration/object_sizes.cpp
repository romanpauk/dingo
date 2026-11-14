//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#include <dingo/container.h>
#include <dingo/runtime/memory.h>
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

// The runtime container holds its registry: its lookups and its runtime data.
using size_registry_type = detail::container_implementation_t<container<>>;

using size_service_proxy =
    decltype(std::declval<container<> &>()
                 .register_type<scope<unique>, storage<size_service>>());
using size_dependency_proxy =
    decltype(std::declval<container<> &>()
                 .register_type<scope<unique>, storage<size_dependency>>());
using size_static_container = container<static_container_traits>;
using size_static_service_proxy =
    decltype(std::declval<size_static_container &>()
                 .register_type<scope<unique>, storage<size_service>>());
using size_static_dependency_proxy =
    decltype(std::declval<size_static_container &>()
                 .register_type<scope<unique>, storage<size_dependency>>());

static_assert(std::is_same_v<typename size_service_proxy::container_type,
                             typename size_dependency_proxy::container_type>);
static_assert(
    std::is_same_v<typename size_static_service_proxy::container_type,
                   typename size_static_dependency_proxy::container_type>);

struct size_probe_registry : size_registry_type {
  using base_type = size_registry_type;

  using typename base_type::base_lookup_entry;
  using typename base_type::lookup_index_entries;
  using typename base_type::runtime_bindings_state;
  using typename base_type::runtime_lookup_value;
  using typename base_type::runtime_scope_state;
  using runtime_binding_interface_type = runtime_binding_interface<
      typename size_registry_type::rtti_type,
      runtime_context<typename size_registry_type::allocator_type>>;
};

using size_registration =
    type_registration<scope<shared>, storage<size_implementation>,
                      interfaces<size_interface>>;
using size_model = detail::binding_model<size_registration>;
using size_storage = typename size_model::storage_type;
using size_instance_container = detail::container_implementation_t<container<>>;

// The record of a registration: its header, the cell of its storage, and the
// binding of each of its interfaces.
template <typename Registration>
using size_record_for = detail::runtime_binding_record<
    detail::binding_cell_t<detail::binding_model<Registration>>,
    size_probe_registry::runtime_binding_interface_type,
    type_list_size_v<
        typename detail::binding_model<Registration>::interface_types>>;

using size_record = size_record_for<size_registration>;
using size_multi_interface_registration =
    type_registration<scope<shared>,
                      storage<std::shared_ptr<size_multi_implementation>>,
                      interfaces<size_interface, size_second_interface>>;
using size_multi_interface_record =
    size_record_for<size_multi_interface_registration>;

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

using size_unique_record = size_record_for<
    type_registration<scope<unique>, storage<size_implementation>>>;
using size_external_record = size_record_for<
    type_registration<scope<external>, storage<size_implementation &>>>;
using size_shared_ptr_record = size_record_for<type_registration<
    scope<shared>, storage<std::shared_ptr<size_implementation>>>>;
using size_shared_cyclical_record = size_record_for<type_registration<
    scope<shared_cyclical>, storage<std::shared_ptr<size_implementation>>>>;

template <typename T>
void expect_size_at_most(const char *name, std::size_t expected) {
  SCOPED_TRACE(name);
  EXPECT_LE(sizeof(T), expected);
}

} // namespace

TEST(object_sizes_test, runtime_container_and_lookup_sizes) {
  if constexpr (sizeof(void *) == 8) {
    // Root owners carry the dense table of lazily allocated child lookup
    // states. A level of the local bindings of a registration is a container
    // of the same type, which borrows the runtime data of its root.
    expect_size_at_most<container<>>("container<>", 120);
    expect_size_at_most<size_instance_container>("registration container", 120);
    expect_size_at_most<size_probe_registry::runtime_bindings_state>(
        "runtime bindings state", 48);
    expect_size_at_most<size_probe_registry::runtime_scope_state>(
        "runtime scope state", 56);
    expect_size_at_most<
        runtime_memory<typename size_registry_type::allocator_type>>(
        "container runtime", 32);
    expect_size_at_most<detail::object_store<arena<>>>("object store", 8);
    expect_size_at_most<runtime_transaction<std::allocator<char>>>(
        "runtime transaction", 72);
    // Runtime context carries one hidden construction-policy stack head.
    expect_size_at_most<runtime_context<std::allocator<char>>>(
        "runtime context", 40);
    expect_size_at_most<size_storage>("shared registration storage", 16);
    // The record of a registration holds, in one allocation, the request
    // cache its interfaces share, the container it is made in and the level
    // of its local bindings, the cell of its storage, and the node and the
    // header of the binding of each interface. A storage that keeps its
    // instance keeps what is converted and composed of it too, behind one
    // pointer.
    expect_size_at_most<runtime_binding_header>("runtime binding header", 32);
    expect_size_at_most<size_probe_registry::runtime_binding_interface_type>(
        "runtime binding of an interface", 16);
    EXPECT_LE(size_record::size, 72u);
    EXPECT_LE(size_unique_record::size, 56u);
    EXPECT_LE(size_external_record::size, 64u);
    EXPECT_LE(size_shared_ptr_record::size, 80u);
    EXPECT_LE(size_shared_cyclical_record::size, 80u);
    EXPECT_LE(size_multi_interface_record::size, 96u);

    expect_size_at_most<size_probe_registry::runtime_lookup_value>(
        "runtime lookup value", 8);
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

  static_assert(!std::is_polymorphic_v<size_storage>);
  static_assert(sizeof(size_base_lookup_key) == sizeof(size_type_index) * 2);
  static_assert(sizeof(size_probe_registry::runtime_lookup_value) ==
                sizeof(void *));
}

} // namespace dingo
