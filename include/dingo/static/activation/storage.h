//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/base/construction_scope.h>
#include <dingo/binding/model.h>
#include <dingo/binding/request.h>
#include <dingo/dependency/request.h>
#include <dingo/key/key.h>
#include <dingo/static/activation/cache.h>
#include <dingo/static/bindings.h>

#include <type_traits>
#include <utility>

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4702)
#endif

namespace dingo {
namespace detail {

template <typename... Registrations> class basic_static_activation_set;

template <typename Registration> struct binding_storage_slot {
  using binding_model = detail::binding_model<Registration>;
  using storage_type = typename binding_model::storage_type;

  storage_type storage;
};

template <typename BindingModel>
struct binding_factory_is_default_constructible
    : std::bool_constant<std::is_default_constructible_v<
          typename BindingModel::factory_type>> {};

template <typename BindingModel>
struct binding_storage_is_default_constructible
    : std::bool_constant<std::is_default_constructible_v<
          typename BindingModel::storage_type>> {};

template <typename Registration,
          typename BindingsType =
              typename detail::binding_model<Registration>::bindings_type>
struct local_binding_scope_slot {};

template <typename Registration, typename... LocalRegistrations>
struct local_binding_scope_slot<Registration,
                                static_bindings<LocalRegistrations...>> {
  basic_static_activation_set<LocalRegistrations...> scope;
};

template <typename... Registrations>
class static_binding_storage
    : private binding_conversion_cache_slot<Registrations>...,
      private binding_storage_slot<Registrations>...,
      private local_binding_scope_slot<Registrations>... {
  template <typename Registration>
  using storage_slot = binding_storage_slot<Registration>;

  template <typename Registration>
  using conversion_cache_slot = binding_conversion_cache_slot<Registration>;

  template <typename Registration>
  using local_scope_slot = local_binding_scope_slot<Registration>;

public:
  template <typename Registration> auto &get_storage() {
    return static_cast<storage_slot<Registration> &>(*this).storage;
  }

  template <typename BindingModel> auto &get_storage_for_model() {
    return get_storage<typename BindingModel::registration_type>();
  }

  template <typename BindingModel> auto &get_conversion_cache_for_model() {
    return static_cast<
        conversion_cache_slot<typename BindingModel::registration_type> &>(
        *this);
  }

  template <typename BindingModel> auto &get_local_scope_for_model() {
    return static_cast<
               local_scope_slot<typename BindingModel::registration_type> &>(
               *this)
        .scope;
  }
};

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
