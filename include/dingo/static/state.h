//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/binding/binding_collection.h>
#include <dingo/binding/binding_model.h>
#include <dingo/resolution/cache.h>
#include <dingo/static/binding.h>
#include <dingo/static/registration.h>
#include <dingo/static/resolution.h>
#include <dingo/type/collection_traits.h>
#include <dingo/type/key.h>

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

template <typename Registration,
          typename BindingsType =
              typename detail::binding_model<Registration>::bindings_type>
struct local_binding_scope_slot {};

template <typename Registration, typename... LocalRegistrations>
struct local_binding_scope_slot<Registration,
                                static_bindings<LocalRegistrations...>> {
  basic_static_activation_set<LocalRegistrations...> scope;
};

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
using registration_conversion_cache_base = binding_conversion_cache_base<
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

template <typename Derived> class basic_static_activation_set_base {
public:
  template <typename T, typename BindingModel, typename Host, typename Context>
  decltype(auto) resolve_binding_type(construction_scope scope, Host &host,
                                      Context &context) {
    return detail::evaluate_static_binding<T, BindingModel>(scope, derived(),
                                                            host, context);
  }

  template <typename InterfaceBinding, typename Host>
  auto make_binding_resolver(construction_scope scope, Host &host) {
    return detail::make_static_binding_resolver<InterfaceBinding>(
        scope, derived(), host);
  }

  template <typename Request, typename LocalRegistry, typename BindingModel,
            typename LookupKey, typename Host, typename Context,
            typename R = typename Request::result_type>
  decltype(auto) resolve_local_binding(construction_scope scope, Host &host,
                                       Context &context, LookupKey key) {
    if constexpr (collection_traits<R>::is_collection) {
      using collection_type = collection_traits<R>;
      auto append = [](auto &values, auto &&value) {
        collection_type::add(values, std::forward<decltype(value)>(value));
      };
      auto &local_scope =
          derived().template get_local_scope_for_model<BindingModel>();
      return construct_binding_collection<R>(
          static_collection_binding_count<LocalRegistry, R, LookupKey>() +
              host.template count_collection<R>(key),
          false, [&](R &results) {
            local_scope
                .template append_static_collection<R, LookupKey, LocalRegistry>(
                    scope, results, host, context, append);
            host.template append_collection<R>(scope, results, context, append,
                                               key);
          });
    } else {
      using selection =
          typename LocalRegistry::template selection<R, LookupKey>;
      if constexpr (selection::status == detail::binding_status::found) {
        using binding = typename selection::binding_type;
        auto &local_scope =
            derived().template get_local_scope_for_model<BindingModel>();
        auto resolver =
            local_scope.template make_binding_resolver<binding>(scope, host);
        return resolver.template resolve<R>(context);
      } else {
        return host.template resolve<typename Request::user_type,
                                     Request::removes_rvalue_references>(
            scope, context, std::move(key));
      }
    }
  }

  template <typename T, typename LookupKey, typename StaticRegistryType,
            typename Host, typename Fn, typename Context>
  std::size_t append_static_collection(construction_scope scope, T &results,
                                       Host &host, Context &context, Fn &&fn) {
    static_assert(is_lookup_key_v<LookupKey>);
    return detail::append_static_collection_impl<T, LookupKey,
                                                 StaticRegistryType>(
        scope, derived(), results, host, context, std::forward<Fn>(fn));
  }

  template <typename T, typename LookupKey, typename StaticRegistryType,
            typename Host, typename Fn, typename Context>
  T construct_static_collection(construction_scope scope, Host &host,
                                Context &context, Fn &&fn) {
    static_assert(is_lookup_key_v<LookupKey>);
    return detail::construct_static_collection_impl<T, LookupKey,
                                                    StaticRegistryType>(
        scope, derived(), host, context, std::forward<Fn>(fn));
  }

  template <typename T, typename LookupKey, typename StaticRegistryType,
            typename Host, typename Context>
  T construct_static_collection(construction_scope scope, Host &host,
                                Context &context) {
    static_assert(is_lookup_key_v<LookupKey>);
    return detail::construct_static_collection_default_impl<T, LookupKey,
                                                            StaticRegistryType>(
        scope, derived(), host, context);
  }

private:
  Derived &derived() { return static_cast<Derived &>(*this); }
};

template <typename... Registrations>
class basic_static_activation_set
    : public basic_static_activation_set_base<
          basic_static_activation_set<Registrations...>>,
      private static_binding_storage<Registrations...> {
public:
  using static_binding_storage<
      Registrations...>::get_conversion_cache_for_model;
  using static_binding_storage<Registrations...>::get_local_scope_for_model;
  using static_binding_storage<Registrations...>::get_storage;
  using static_binding_storage<Registrations...>::get_storage_for_model;
};

template <typename... Registrations>
using static_storage_state = static_binding_storage<Registrations...>;

template <typename StorageState>
class basic_static_activation_set_view
    : public basic_static_activation_set_base<
          basic_static_activation_set_view<StorageState>> {
public:
  explicit basic_static_activation_set_view(StorageState &state)
      : state_(&state) {}

  template <typename Registration> auto &get_storage() {
    return state_->template get_storage<Registration>();
  }

  template <typename BindingModel> auto &get_storage_for_model() {
    return state_->template get_storage_for_model<BindingModel>();
  }

  template <typename BindingModel> auto &get_conversion_cache_for_model() {
    return state_->template get_conversion_cache_for_model<BindingModel>();
  }

  template <typename BindingModel> auto &get_local_scope_for_model() {
    return state_->template get_local_scope_for_model<BindingModel>();
  }

private:
  StorageState *state_;
};

} // namespace detail
} // namespace dingo

#ifdef _MSC_VER
#pragma warning(pop)
#endif
