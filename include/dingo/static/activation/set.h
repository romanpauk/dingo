//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/base/construction_scope.h>
#include <dingo/base/exceptions.h>
#include <dingo/binding/selection.h>
#include <dingo/dependency/collection.h>
#include <dingo/key/key.h>
#include <dingo/static/activation/resolver.h>
#include <dingo/static/activation/storage.h>

#include <cstddef>
#include <utility>

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4702)
#endif

namespace dingo {
namespace detail {

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
      R results;
      auto append = [](auto &values, auto &&value) {
        collection_type::add(values, std::forward<decltype(value)>(value));
      };
      auto &local_scope =
          derived().template get_local_scope_for_model<BindingModel>();
      const auto local_count =
          local_scope
              .template append_static_collection<R, LookupKey, LocalRegistry>(
                  scope, results, host, context, append);
      const auto host_count = host.template append_collection<R>(
          scope, results, context, append, key);
      if (local_count + host_count == 0) {
        throw detail::make_collection_type_not_found_exception<
            R, typename collection_type::resolve_type>();
      }
      return results;
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
