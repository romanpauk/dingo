//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

// The static binding implementation: activation sets, storage slots, the
// binding resolver, and the static registry that owns them.

#include <dingo/base/exceptions.h>
#include <dingo/base/type_descriptor.h>
#include <dingo/binding/binding_collection.h>
#include <dingo/binding/binding_model.h>
#include <dingo/binding/binding_resolution.h>
#include <dingo/binding/routes.h>
#include <dingo/context/context_base.h>
#include <dingo/factory/factory_traits.h>
#include <dingo/resolution/cache.h>
#include <dingo/static/graph.h>
#include <dingo/static/storage.h>
#include <dingo/type/annotated.h>
#include <dingo/type/collection_traits.h>
#include <dingo/type/key.h>

#include <cassert>
#include <cstddef>
#include <type_traits>
#include <utility>

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4702)
#endif

namespace dingo {
namespace detail {

template <typename... Registrations> class static_registry_state;

struct no_dependency_context {
  template <typename T, typename Container> T resolve(Container &) = delete;
};

// The state of one static binding: its storage slot and its conversion cache.
// It is the static partner of runtime_binding_state.
template <typename Registration>
class static_binding_state
    : public static_binding_slot<Registration>,
      private binding_conversion_cache_slot<Registration> {
  using storage_slot = static_binding_slot<Registration>;
  using conversion_cache_slot = binding_conversion_cache_slot<Registration>;

public:
  using storage_slot::storage;

  auto &conversion_cache() {
    return static_cast<conversion_cache_slot &>(*this);
  }
};

// Whether a static binding serves the request directly. A runtime key has no
// static selection, so it is never served statically. The probe answers both
// the presence and the route question from one selection.
template <typename Bindings, typename T, typename LookupKey,
          bool Static = is_static_lookup_key_definition_v<LookupKey>>
struct static_request_state {
  static constexpr bool present = false;
  static constexpr bool served = false;
};

template <typename Bindings, typename T, typename LookupKey>
struct static_request_state<Bindings, T, LookupKey, true> {
private:
  using request = request_type<std::remove_cv_t<T>, false>;
  using selection =
      typename Bindings::template selection<typename request::lookup_type,
                                            LookupKey>;

public:
  static constexpr bool present =
      selection::status != binding_status::not_found;
  static constexpr bool served = [] {
    if constexpr (selection::status == binding_status::ambiguous) {
      return true;
    } else if constexpr (selection::status == binding_status::found) {
      return binding_supports_request_v<typename request::interface_type,
                                        typename selection::binding_type>;
    } else {
      return false;
    }
  }();
};

template <typename State, typename Host, typename BindingModel>
struct static_binding_policy {
  using storage_type = typename BindingModel::storage_type;

  State &state;
  Host &host;
  construction_scope scope;

  auto &slot() { return state.template get<BindingModel>(); }

  template <typename Operation, typename Request, typename Context,
            typename StorageType, typename Activation, typename Fn>
  decltype(auto) with_source(construction_scope source_scope, Context &context,
                             StorageType &storage, Activation &activation,
                             Fn &&fn) {
    constexpr bool context_lvalue =
        (std::is_lvalue_reference_v<Request> || std::is_pointer_v<Request>) &&
        source_category_of<StorageType> == source_category::consume_value;
    constexpr bool retained_source = Operation::requires_source_retention &&
                                     context_supports_retained_frame_v<Context>;
    constexpr source_lifetime lifetime = [] {
      if constexpr (context_lvalue) {
        return source_lifetime::context_lvalue;
      } else if constexpr (is_runtime_context_v<Context>) {
        return source_lifetime::context;
      } else if constexpr (!context_supports_construct<Context>::value) {
        return retained_source ? source_lifetime::retained
                               : source_lifetime::local;
      } else {
        using materialization_traits =
            storage_materialization_traits<typename StorageType::tag_type,
                                           typename StorageType::type>;
        if constexpr (has_context_materialize_source<materialization_traits,
                                                     Context, StorageType,
                                                     Activation>::value) {
          return source_lifetime::context;
        } else {
          return retained_source ? source_lifetime::retained
                                 : source_lifetime::local;
        }
      }
    }();
    return with_materialized_source<lifetime>(source_scope, context, storage,
                                              activation, std::forward<Fn>(fn));
  }

  template <typename Context, typename StorageType, typename Activation,
            typename Fn>
  decltype(auto) with_value_source(construction_scope source_scope,
                                   Context &context, StorageType &storage,
                                   Activation &activation, Fn &&fn) {
    return with_materialized_source<source_lifetime::local>(
        source_scope, context, storage, activation, std::forward<Fn>(fn));
  }

  template <typename T, bool RemoveRvalueReferences, typename Context,
            typename LookupKey = detail::no_lookup_key_t>
  decltype(auto) resolve(construction_scope construction, Context &context,
                         LookupKey key = {}) {
    return host.template resolve<T, RemoveRvalueReferences>(
        construction, context, std::move(key));
  }

  template <typename T, typename Context, typename Source>
  decltype(auto) retain_conversion(Context &context, Source &&source) {
    return detail::retain_binding_conversion<T>(
        scope, state.template get<BindingModel>().storage(),
        state.template get<BindingModel>().conversion_cache(), context,
        std::forward<Source>(source));
  }

  static constexpr type_descriptor registered_type() {
    return detail::binding_registered_type<
        typename BindingModel::storage_type>();
  }
};

// The typed execution of one selected static route. The policy supplies the
// state, the host, the source lifetime, and the conversion cache; these
// functions own materialization, route application, and result shaping.
template <typename Request, typename Resolution, typename Policy,
          typename Context>
void *resolve_static_address(Policy &policy, Context &context) {
  using operation = typename Resolution::operation;
  auto &storage = policy.slot().storage();
  auto materialize = [&](auto &&source) -> void * {
    return resolve_address_from_source<Resolution,
                                       typename Policy::storage_type>(
               policy.scope, policy, context,
               std::forward<decltype(source)>(source), describe_type<Request>(),
               policy.registered_type())
        .address;
  };
  return policy.template with_source<operation, Request>(
      policy.scope, context, storage, policy, std::move(materialize));
}

template <typename Request, typename Resolution, typename Policy,
          typename Context, typename Fn>
decltype(auto) consume_static_request(Policy &policy, Context &context,
                                      Fn &&fn) {
  using operation = typename Resolution::operation;
  auto &storage = policy.slot().storage();
  auto consume = [&](auto &&source) -> decltype(auto) {
    return apply_consumed_resolution<Request, Resolution,
                                     typename Policy::storage_type>(
        policy, context, std::forward<decltype(source)>(source),
        std::forward<Fn>(fn));
  };
  return policy.template with_source<operation, Request>(
      policy.scope, context, storage, policy, std::move(consume));
}

template <typename Request, typename InterfaceBinding, typename Policy,
          typename Context>
Request resolve_static_binding(Policy &policy, Context &context) {
  static_assert(is_runtime_context_v<Context> ||
                    binding_supports_request_v<Request, InterfaceBinding>,
                "static resolution cannot satisfy a request the storage "
                "does not publish");
  using matched_resolution =
      typename binding_supports_request<Request, InterfaceBinding>::type;
  if constexpr (!std::is_void_v<matched_resolution> &&
                !std::is_reference_v<Request> && !std::is_pointer_v<Request>) {
    return consume_static_request<Request, matched_resolution>(
        policy, context, [](auto &&instance) -> Request {
          return Request(std::forward<decltype(instance)>(instance));
        });
  } else {
    void *ptr = nullptr;
    if constexpr (!std::is_void_v<matched_resolution>) {
      ptr =
          resolve_static_address<Request, matched_resolution>(policy, context);
    } else {
      throw detail::make_type_not_convertible_exception(
          describe_type<Request>(), policy.registered_type(), context);
    }
    return detail::convert_resolved_binding<Request>(ptr);
  }
}

template <typename Request, typename InterfaceBinding, typename Policy,
          typename Context, typename Fn>
decltype(auto) consume_static_binding(Policy &policy, Context &context,
                                      Fn &&fn) {
  static_assert(is_runtime_context_v<Context> ||
                    binding_supports_request_v<Request, InterfaceBinding>,
                "static resolution cannot satisfy a request the storage "
                "does not publish");
  using matched_resolution =
      typename binding_supports_request<Request, InterfaceBinding>::type;

  if constexpr (!std::is_void_v<matched_resolution>) {
    return consume_static_request<Request, matched_resolution>(
        policy, context, std::forward<Fn>(fn));
  } else {
    throw detail::make_type_not_convertible_exception(
        describe_type<Request>(), policy.registered_type(), context);
  }
}

template <typename T, typename LookupKey, typename StaticRegistryType,
          typename State, typename Host, typename Fn, typename Context>
std::size_t append_static_collection_impl(construction_scope scope,
                                          State &state, T &results, Host &host,
                                          Context &context, Fn &&fn) {
  static_assert(is_lookup_key_v<LookupKey>);
  using collection_type = collection_traits<T>;
  using resolve_type = typename collection_type::resolve_type;
  using interface_bindings = typename StaticRegistryType::template bindings<
      normalized_type_t<resolve_type>, LookupKey>;

  static_assert(collection_type::is_collection,
                "missing collection_traits specialization for type T");

  constexpr std::size_t count = type_list_size_v<interface_bindings>;
  if constexpr (count != 0) {
    dingo::for_each(interface_bindings{}, [&](auto binding_iterator) {
      using binding = typename decltype(binding_iterator)::type;
      static_binding_policy<State, Host, typename binding::binding_model_type>
          policy{state, host, scope};
      if constexpr (is_copy_constructible_v<resolve_type> &&
                    !std::is_reference_v<resolve_type>) {
        consume_static_binding<resolve_type, binding>(
            policy, context, [&](auto &&value) {
              fn(results, std::forward<decltype(value)>(value));
            });
      } else {
        fn(results,
           resolve_static_binding<resolve_type, binding>(policy, context));
      }
    });
  }

  return count;
}

template <typename T, typename LookupKey, typename StaticRegistryType,
          typename State, typename Host, typename Fn, typename Context>
T construct_static_collection_impl(construction_scope scope, State &state,
                                   Host &host, Context &context, Fn &&fn) {
  static_assert(is_lookup_key_v<LookupKey>);
  using collection_type = collection_traits<T>;
  using resolve_type = typename collection_type::resolve_type;
  using interface_bindings = typename StaticRegistryType::template bindings<
      normalized_type_t<resolve_type>, LookupKey>;

  static_assert(collection_type::is_collection,
                "missing collection_traits specialization for type T");
  static_assert(
      type_list_size_v<interface_bindings> != 0,
      "static container cannot construct a collection for an unbound type");

  T results;
  collection_type::reserve(results, type_list_size_v<interface_bindings>);
  append_static_collection_impl<T, LookupKey, StaticRegistryType>(
      scope, state, results, host, context, std::forward<Fn>(fn));
  return results;
}

template <typename Derived> class static_registry_state_base {
public:
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
    using collection_type = collection_traits<T>;
    return detail::construct_static_collection_impl<T, LookupKey,
                                                    StaticRegistryType>(
        scope, derived(), host, context, [](auto &collection, auto &&value) {
          collection_type::add(collection,
                               std::forward<decltype(value)>(value));
        });
  }

private:
  Derived &derived() { return static_cast<Derived &>(*this); }
};

template <typename... Registrations>
class static_registry_state : public static_registry_state_base<
                                  static_registry_state<Registrations...>>,
                              private static_binding_state<Registrations>... {
  template <typename Registration>
  using binding_state = static_binding_state<Registration>;

public:
  // One accessor for the slot and conversion cache of a model.
  template <typename BindingModel> auto &get() {
    return static_cast<
        binding_state<typename BindingModel::registration_type> &>(*this);
  }
};

} // namespace detail
} // namespace dingo

#ifdef _MSC_VER
#pragma warning(pop)
#endif
