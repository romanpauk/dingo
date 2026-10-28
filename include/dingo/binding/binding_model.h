//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/base/config.h>

#include <dingo/base/type_list.h>
#include <dingo/registration/requirements.h>
#include <dingo/registration/type_registration.h>
#include <dingo/storage/interface_storage_traits.h>
#include <dingo/storage/storage.h>
#include <dingo/type/rebind_type.h>

#include <type_traits>

namespace dingo {
namespace detail {

template <typename Interface, typename BindingModel> struct binding {
  using interface_type = Interface;
  using binding_model_type = BindingModel;
  using storage_type = typename BindingModel::storage_type;
  using key_type = typename BindingModel::key_type;
};

// The registration facts that do not depend on the completeness of the scope
// tag. The two model specializations below add the storage the completion
// check allows.
template <typename Registration> struct binding_model_base {
  using registration_type = Registration;
  using interface_types = typename Registration::interface_type::type;
  using registered_storage_type = typename Registration::storage_type::type;
  using scope_type = typename Registration::scope_type;
  using storage_tag = typename scope_type::type;
  using factory_type = typename Registration::factory_type::type;
  using registration_key_type = typename Registration::key_type;
  using key_type = detail::make_lookup_key_t<registration_key_type>;
  using conversions_type = typename Registration::conversions_type::type;
  using dependencies_type = typename Registration::dependencies_type;
};

template <typename Registration, bool StorageTagIsComplete>
struct binding_model_impl : binding_model_base<Registration> {
  using base = binding_model_base<Registration>;
  using registered_storage_type = typename base::registered_storage_type;

  static constexpr bool storage_tag_is_complete = StorageTagIsComplete;
  static constexpr bool use_interface_as_stored_leaf = false;
  using stored_leaf_type = leaf_type_t<registered_storage_type>;
  using stored_type = rebind_leaf_t<registered_storage_type, stored_leaf_type>;
  using storage_type = void;
  using requirements = void;
  static constexpr bool valid = false;
};

template <typename Registration>
struct binding_model_impl<Registration, true>
    : binding_model_base<Registration> {
  using base = binding_model_base<Registration>;
  using interface_types = typename base::interface_types;
  using registered_storage_type = typename base::registered_storage_type;
  using storage_tag = typename base::storage_tag;
  using factory_type = typename base::factory_type;
  using conversions_type = typename base::conversions_type;

  static constexpr bool storage_tag_is_complete = true;
  static constexpr bool use_interface_as_stored_leaf =
      use_interface_as_stored_leaf_v<registered_storage_type, interface_types>;

  // A raw dynamic array has no extent, so no storage can size, construct, or
  // destroy it. Bounded arrays and owning array handles carry their shape.
  static_assert(!std::is_array_v<registered_storage_type> ||
                    std::extent_v<registered_storage_type, 0> != 0,
                "raw dynamic array storage has no element count; register "
                "T[N] or an owning array handle instead");

  using stored_leaf_type = std::conditional_t<
      use_interface_as_stored_leaf,
      typename annotated_traits<type_list_head_t<interface_types>>::type,
      leaf_type_t<registered_storage_type>>;
  using stored_type = rebind_leaf_t<registered_storage_type, stored_leaf_type>;
  using storage_type = storage<storage_tag, registered_storage_type,
                               stored_type, factory_type, conversions_type>;
  using requirements = registration_requirements<storage_type, interface_types,
                                                 typename storage_type::type>;

  static constexpr bool valid = requirements::valid;
};

template <typename Registration>
using binding_model = binding_model_impl<
    Registration,
    !requires_complete_type_v<typename Registration::scope_type::type> ||
        is_complete_v<typename Registration::scope_type::type>>;

template <typename BindingModel, typename InterfaceList>
struct binding_expansion_impl;

template <typename BindingModel, typename... Interfaces>
struct binding_expansion_impl<BindingModel, type_list<Interfaces...>> {
  using interface_bindings = type_list<binding<Interfaces, BindingModel>...>;
};

template <typename BindingModel>
using binding_expansion =
    binding_expansion_impl<BindingModel,
                           typename BindingModel::interface_types>;

} // namespace detail
} // namespace dingo
