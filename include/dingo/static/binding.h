//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/base/type_list.h>
#include <dingo/binding/binding_model.h>
#include <dingo/binding/binding_resolution.h>
#include <dingo/resolution/cache.h>
#include <dingo/static/registration.h>
#include <dingo/type/annotated.h>

#include <type_traits>
#include <utility>

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4702)
#endif

namespace dingo {
namespace detail {

template <typename BindingModel>
struct binding_factory_is_default_constructible
    : std::bool_constant<std::is_default_constructible_v<
          typename BindingModel::factory_type>> {};

template <typename BindingModel>
struct binding_storage_is_default_constructible
    : std::bool_constant<std::is_default_constructible_v<
          typename BindingModel::storage_type>> {};

struct no_dependency_context {
  template <typename T, typename Container> T resolve(Container &) = delete;
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
  using interface_cache_types = resolution_cache_types_t<
      typename binding_resolutions<typename annotated_traits<Interface>::type,
                                   storage_type>::type,
      storage_shape_t<storage_type>>;

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

template <typename State, typename Host, typename BindingModel>
struct binding_activation {
  State &state;
  Host &host;
  construction_scope scope;

  template <typename Request, typename Context>
  decltype(auto) resolve_request(construction_scope construction,
                                 Context &context) {
    using local_bindings = typename BindingModel::bindings_type;
    if constexpr (std::is_void_v<local_bindings>) {
      return host.template resolve<typename Request::user_type,
                                   Request::removes_rvalue_references>(
          construction, context, detail::no_lookup_key());
    } else {
      return state.template resolve_local_binding<Request, local_bindings,
                                                  BindingModel>(construction,
                                                                host, context);
    }
  }

  template <typename Request, typename Context, typename LookupKey,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  decltype(auto) resolve_request(construction_scope construction,
                                 Context &context, LookupKey key) {
    using local_bindings = typename BindingModel::bindings_type;
    if constexpr (std::is_void_v<local_bindings>) {
      return host.template resolve<typename Request::user_type,
                                   Request::removes_rvalue_references>(
          construction, context, std::move(key));
    } else {
      return state.template resolve_local_binding<Request, local_bindings,
                                                  BindingModel, LookupKey>(
          construction, host, context, std::move(key));
    }
  }

  template <typename T, bool RemoveRvalueReferences, typename Context>
  decltype(auto) resolve(construction_scope construction, Context &context) {
    return resolve_request<request_type<T, RemoveRvalueReferences>>(
        construction, context);
  }

  template <typename T, bool RemoveRvalueReferences, typename Context,
            typename LookupKey,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  decltype(auto) resolve(construction_scope construction, Context &context,
                         LookupKey key) {
    return resolve_request<request_type<T, RemoveRvalueReferences>>(
        construction, context, std::move(key));
  }

  template <typename T, typename Context>
  decltype(auto) resolve(Context &context) {
    return state.template resolve_binding_type<T, BindingModel>(scope, host,
                                                                context);
  }

  template <typename T, typename Context, typename Source>
  decltype(auto) retain_conversion(Context &context, Source &&source) {
    return detail::retain_binding_conversion<T>(
        scope, state.template get_storage_for_model<BindingModel>(),
        state.template get_conversion_cache_for_model<BindingModel>(), context,
        std::forward<Source>(source));
  }
};

} // namespace detail
} // namespace dingo

#ifdef _MSC_VER
#pragma warning(pop)
#endif
