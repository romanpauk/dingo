//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

// The static storage: one storage slot and one conversion-cache slot per
// registration, plus the storage state the static registry owns. The activation
// set and the binding resolver live in binding.h.

#include <dingo/base/type_list.h>
#include <dingo/binding/binding_model.h>
#include <dingo/binding/binding_resolution.h>
#include <dingo/resolution/cache.h>
#include <dingo/type/annotated.h>
#include <dingo/type/rebind_type.h>

#include <type_traits>

namespace dingo {
namespace detail {

template <typename Registration>
struct binding_storage_slot
    : detail::binding_slot<
          typename detail::binding_model<Registration>::storage_type> {
  using binding_model = detail::binding_model<Registration>;
  using storage_type = typename binding_model::storage_type;
};

template <typename Request, typename StorageType>
inline constexpr bool stored_request_identity_v = [] {
  using stored_type = std::remove_cv_t<
      std::remove_reference_t<typename StorageType::stored_type>>;
  return std::is_same_v<Request, stored_type> ||
         std::is_same_v<Request, stored_type &> ||
         std::is_same_v<Request, const stored_type &> ||
         std::is_same_v<Request, stored_type *> ||
         std::is_same_v<Request, const stored_type *>;
}();

template <typename BindingModel, bool StoredInterfaces, typename... Interfaces>
struct binding_cache_types_impl;

template <typename BindingModel, typename... Interfaces>
struct binding_cache_types_impl<BindingModel, false, Interfaces...> {
private:
  using storage_type = typename BindingModel::storage_type;

  template <typename Interface>
  using interface_cache_types = binding_conversion_cache_types_t<
      typename annotated_traits<Interface>::type, storage_type>;

public:
  using type =
      type_list_unique_t<type_list_cat_t<interface_cache_types<Interfaces>...>>;
};

template <typename BindingModel, typename... Interfaces>
struct binding_cache_types_impl<BindingModel, true, Interfaces...> {
  using type = type_list<>;
};

template <typename BindingModel, typename Interfaces>
struct binding_cache_types;

template <typename BindingModel>
inline constexpr bool binding_has_cacheless_conversion_shape_v = [] {
  using conversions = typename BindingModel::conversions_type;
  return conversions::is_stable &&
         std::is_same_v<typename conversions::value_types,
                        type_list<runtime_type>> &&
         std::is_same_v<typename conversions::lvalue_reference_types,
                        type_list<runtime_type &>> &&
         std::is_same_v<typename conversions::rvalue_reference_types,
                        type_list<>> &&
         std::is_same_v<typename conversions::pointer_types,
                        type_list<runtime_type *>>;
}();

template <typename BindingModel>
inline constexpr bool binding_uses_default_conversions_v = std::is_same_v<
    typename BindingModel::conversions_type,
    detail::conversions<typename BindingModel::storage_tag,
                        typename BindingModel::registered_storage_type,
                        runtime_type>>;

template <typename Request, typename BindingModel>
inline constexpr bool binding_has_cacheless_default_leaf_v =
    binding_uses_default_conversions_v<BindingModel> &&
    BindingModel::conversions_type::is_stable &&
    std::is_same_v<std::remove_cv_t<Request>,
                   std::remove_cv_t<typename BindingModel::stored_leaf_type>>;

template <typename BindingModel, typename... Interfaces>
struct binding_cache_types<BindingModel, type_list<Interfaces...>>
    // Exact stored requests and stable default leaf requests cannot retain an
    // adapted value, so enumerating every resolution route cannot add a slot.
    : binding_cache_types_impl<
          BindingModel,
          (binding_has_cacheless_conversion_shape_v<BindingModel> &&
           (stored_request_identity_v<
                typename annotated_traits<Interfaces>::type,
                typename BindingModel::storage_type> &&
            ...)) ||
              (binding_has_cacheless_default_leaf_v<
                   typename annotated_traits<Interfaces>::type, BindingModel> &&
               ...),
          Interfaces...> {};

template <typename BindingModel>
using binding_cache_types_t =
    typename binding_cache_types<BindingModel,
                                 typename BindingModel::interface_types>::type;

// The cache types are only enumerated for stable conversions; naming them in
// the primary template would instantiate them for every binding.
template <typename BindingModel,
          bool Stable =
              cache::is_stable_storage_v<typename BindingModel::storage_type>>
inline constexpr bool has_binding_conversion_cache_v = false;

template <typename BindingModel>
inline constexpr bool has_binding_conversion_cache_v<BindingModel, true> =
    type_list_size_v<binding_cache_types_t<BindingModel>> != 0;

template <typename Registration, bool Enabled = has_binding_conversion_cache_v<
                                     detail::binding_model<Registration>>>
struct binding_conversion_cache_slot;

template <typename Registration, bool Enabled> struct registration_cache_types {
  using binding_model = detail::binding_model<Registration>;
  using type = binding_cache_types_t<binding_model>;
};

template <typename Registration>
struct registration_cache_types<Registration, false> {
  using type = type_list<>;
};

template <typename Registration, bool Enabled>
using registration_conversion_cache_base = binding_cache_slot<
    Enabled, typename registration_cache_types<Registration, Enabled>::type>;

template <typename Registration, bool Enabled>
struct binding_conversion_cache_slot
    : registration_conversion_cache_base<Registration, Enabled> {
  using binding_model = detail::binding_model<Registration>;
  using conversions_type = typename binding_model::storage_type::conversions;
  using base_type = registration_conversion_cache_base<Registration, Enabled>;

  using base_type::construct_conversion;
};

template <typename... Registrations>
class static_storage : private binding_conversion_cache_slot<Registrations>...,
                       private binding_storage_slot<Registrations>... {
  template <typename Registration>
  using storage_slot = binding_storage_slot<Registration>;

  template <typename Registration>
  using conversion_cache_slot = binding_conversion_cache_slot<Registration>;

public:
  template <typename Registration> auto &get_storage() {
    return static_cast<storage_slot<Registration> &>(*this).storage();
  }

  template <typename BindingModel> auto &get_slot_for_model() {
    return static_cast<
        storage_slot<typename BindingModel::registration_type> &>(*this);
  }

  template <typename BindingModel> auto &get_storage_for_model() {
    return get_slot_for_model<BindingModel>().storage();
  }

  template <typename BindingModel> auto &get_conversion_cache_for_model() {
    return static_cast<
        conversion_cache_slot<typename BindingModel::registration_type> &>(
        *this);
  }
};

template <typename... Registrations>
using static_storage_state = static_storage<Registrations...>;

} // namespace detail
} // namespace dingo
