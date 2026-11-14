//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/base/config.h>

#include <dingo/base/type_list.h>
#include <dingo/lookup/lookup_key.h>
#include <dingo/registration/requirements.h>
#include <dingo/registration/type_registration.h>
#include <dingo/storage/interface_storage_traits.h>
#include <dingo/storage/scope_traits.h>
#include <dingo/storage/storage.h>
#include <dingo/type/shape.h>

#include <type_traits>

namespace dingo {
namespace detail {

template <typename Interface, typename BindingModel> struct binding {
  using interface_type = Interface;
  using binding_model_type = BindingModel;
  using storage_type = typename BindingModel::storage_type;
  using key_type = typename BindingModel::key_type;
};

// What a registration declares.
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
  using bindings_type = typename Registration::bindings_type::type;
};

// A registration whose scope tag is not usable, or whose scope does not store
// the registered type, has no storage.
template <typename Registration, bool StorageTagIsComplete, bool Stored>
struct binding_model_impl : binding_model_base<Registration> {
  static constexpr bool storage_tag_is_complete = StorageTagIsComplete;
  static constexpr bool stored = false;
  static constexpr bool use_interface_as_stored_leaf = false;
  using stored_leaf_type = leaf_type_t<
      typename binding_model_base<Registration>::registered_storage_type>;
  using stored_type = rebind_leaf_t<
      typename binding_model_base<Registration>::registered_storage_type,
      stored_leaf_type>;
  using storage_type = void;
  using requirements = void;
  static constexpr bool valid = false;
};

template <typename Registration>
struct binding_model_impl<Registration, true, true>
    : binding_model_base<Registration> {
  using base = binding_model_base<Registration>;
  using typename base::interface_types;
  using typename base::registered_storage_type;

  static constexpr bool storage_tag_is_complete = true;
  static constexpr bool stored = true;
  static constexpr bool use_interface_as_stored_leaf =
      use_interface_as_stored_leaf_v<registered_storage_type, interface_types>;

  using stored_leaf_type =
      std::conditional_t<use_interface_as_stored_leaf,
                         unannotated_t<type_list_head_t<interface_types>>,
                         leaf_type_t<registered_storage_type>>;
  using stored_type = rebind_leaf_t<registered_storage_type, stored_leaf_type>;
  using storage_type =
      storage<typename base::storage_tag, registered_storage_type, stored_type,
              typename base::factory_type, typename base::conversions_type>;
  using requirements = registration_requirements<storage_type, interface_types,
                                                 typename storage_type::type>;

  static constexpr bool valid = requirements::valid;
};

// A scope tag is usable once it is complete and specializes scope_traits.
template <typename Tag>
inline constexpr bool scope_tag_is_usable_v =
    (!requires_complete_type_v<Tag> || is_complete_v<Tag>) &&
    scope_traits<Tag>::enabled;

// Whether the scope of a registration stores its type.
template <typename Registration> constexpr bool binding_is_stored() {
  using tag = typename Registration::scope_type::type;
  if constexpr (scope_tag_is_usable_v<tag>) {
    return has_storage_cell_v<tag, typename binding_model_base<
                                       Registration>::registered_storage_type>;
  } else {
    return false;
  }
}

template <typename Registration>
using binding_model = binding_model_impl<
    Registration,
    scope_tag_is_usable_v<typename Registration::scope_type::type>,
    binding_is_stored<Registration>()>;

// Checks a registration as both containers do, with the same message for
// the same mistake, and tells whether it is valid: an interface it declares,
// its scope, whether the scope stores its type, and the requirements of its
// storage and its interfaces.
template <typename... TypeArgs>
constexpr bool check_registration(type_registration<TypeArgs...> *) {
  using model = binding_model<type_registration<TypeArgs...>>;
  static_assert(!has_explicit_void_interface_v<TypeArgs...>,
                "interfaces<void> is not a valid registration target");
  static_assert(model::storage_tag_is_complete,
                "registered storage tag must be complete and specialize "
                "dingo::scope_traits; include the corresponding "
                "dingo/storage header");
  static_assert(!model::storage_tag_is_complete || model::stored,
                "the scope does not store this type");
  if constexpr (model::stored) {
    model::requirements::assert_valid();
  }
  return model::valid;
}

template <typename Registration>
inline constexpr bool registration_checked_v =
    check_registration(static_cast<Registration *>(nullptr));

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
