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
#include <dingo/binding/binding_core.h>
#include <dingo/binding/binding_model.h>
#include <dingo/binding/binding_resolution.h>
#include <dingo/binding/resolution_route.h>
#include <dingo/context/context_base.h>
#include <dingo/factory/factory_traits.h>
#include <dingo/resolution/cache.h>
#include <dingo/static/graph.h>
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

template <typename... Registrations> class basic_static_activation_set;

template <typename Registration>
struct binding_storage_slot
    : detail::binding_storage_holder<
          typename detail::binding_model<Registration>::storage_type> {
  using binding_model = detail::binding_model<Registration>;
  using storage_type = typename binding_model::storage_type;
};

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
    return static_cast<storage_slot<Registration> &>(*this).storage();
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

template <typename Request, typename Resolutions>
struct matching_binding_resolution;

// A stable default leaf request always selects the canonical runtime_type
// storage route, so avoid constructing and searching every published route.
template <typename Request, typename BindingModel>
inline constexpr bool binding_has_direct_default_leaf_resolution_v = [] {
  using request = unwrapped_static_request_t<Request>;
  using request_value = std::remove_reference_t<request>;
  using request_leaf = std::remove_cv_t<std::remove_pointer_t<request_value>>;
  using stored_leaf = typename BindingModel::stored_leaf_type;
  using conversions = typename BindingModel::conversions_type;
  constexpr bool publishes_request_category = [] {
    if constexpr (std::is_lvalue_reference_v<request>) {
      return type_list_contains_v<runtime_type &,
                                  typename conversions::lvalue_reference_types>;
    } else if constexpr (std::is_pointer_v<request_value>) {
      return type_list_contains_v<runtime_type *,
                                  typename conversions::pointer_types>;
    } else {
      return false;
    }
  }();
  return binding_uses_default_conversions_v<BindingModel> &&
         conversions::is_stable && publishes_request_category &&
         std::is_same_v<stored_leaf, std::remove_cv_t<stored_leaf>> &&
         std::is_same_v<request_leaf, stored_leaf>;
}();

template <typename Request>
struct matching_binding_resolution<Request, type_list<>> {
  using type = void;
};

template <typename Request, typename Head, typename... Tail>
struct matching_binding_resolution<Request, type_list<Head, Tail...>> {
private:
  using unwrapped_request = unwrapped_static_request_t<Request>;

public:
  using type = std::conditional_t<
      is_resolution_request_for_v<Head, unwrapped_request>, Head,
      typename matching_binding_resolution<Request, type_list<Tail...>>::type>;
};

// Resolution target shapes are disjoint, so static lookup only needs to form
// the category that can match the request. Reference and pointer requests are
// served by the same object routes.
template <typename Request, typename Interface, typename Storage,
          typename RequestType =
              std::remove_cv_t<unwrapped_static_request_t<Request>>>
struct request_binding_resolutions {
  using type =
      typename binding_value_resolutions<Interface,
                                         storage_shape_t<Storage>>::type;
};

template <typename Request, typename Interface, typename Storage,
          typename RequestType>
struct request_binding_resolutions<Request, Interface, Storage, RequestType &> {
  using type =
      typename binding_object_resolutions<Interface,
                                          storage_shape_t<Storage>>::type;
};

template <typename Request, typename Interface, typename Storage,
          typename RequestType>
struct request_binding_resolutions<Request, Interface, Storage,
                                   RequestType &&> {
  using type = typename binding_rvalue_reference_resolutions<
      Interface, storage_shape_t<Storage>>::type;
};

template <typename Request, typename Interface, typename Storage,
          typename RequestType>
struct request_binding_resolutions<Request, Interface, Storage, RequestType *> {
  using type =
      typename binding_object_resolutions<Interface,
                                          storage_shape_t<Storage>>::type;
};

template <typename Request, typename Interface, typename BindingModel,
          bool Direct = binding_has_direct_default_leaf_resolution_v<
              Request, BindingModel>>
struct binding_request_resolutions
    : request_binding_resolutions<Request, Interface,
                                  typename BindingModel::storage_type> {};

// The route of a stable default leaf request is the route the storage publishes
// for the leaf: the object route when the leaf's reference and pointer
// requests are served together, otherwise the route of the request's category.
template <typename Interface, typename Shape, bool Shared, bool Reference>
struct direct_default_leaf_route {
  using type = type_list<typename storage_resolution<
      std::conditional_t<Reference, runtime_type &, runtime_type *>, Interface,
      Shape, borrow, false>::type>;
};

template <typename Interface, typename Shape, bool Reference>
struct direct_default_leaf_route<Interface, Shape, true, Reference> {
private:
  using conversions = typename Shape::conversions;
  static constexpr bool serves_value =
      type_list_contains_v<runtime_type, typename conversions::value_types> &&
      type_list_size_v<typename conversions::rvalue_reference_types> == 0 &&
      is_default_value_request<Interface, Shape, runtime_type *>();

public:
  using type =
      typename shared_object_resolutions<type_list<runtime_type *>, Interface,
                                         Shape, serves_value>::type;
};

template <typename Interface, typename Shape,
          typename Conversions = typename Shape::conversions>
constexpr bool is_default_leaf_object_v =
    type_list_contains_v<runtime_type &,
                         typename Conversions::lvalue_reference_types> &&
    type_list_contains_v<runtime_type *, typename Conversions::pointer_types> &&
    is_default_object_request<Interface, Shape, runtime_type *>();

template <typename Request, typename Interface, typename Storage>
struct direct_default_leaf_request_resolution
    : direct_default_leaf_route<
          Interface, storage_shape_t<Storage>,
          is_default_leaf_object_v<Interface, storage_shape_t<Storage>>,
          std::is_lvalue_reference_v<unwrapped_static_request_t<Request>>> {};

template <typename Request, typename Interface, typename BindingModel>
struct binding_request_resolutions<Request, Interface, BindingModel, true>
    : direct_default_leaf_request_resolution<
          Request, Interface, typename BindingModel::storage_type> {};

template <typename Request, typename InterfaceBinding>
struct binding_supports_request {
private:
  using binding_model_type = typename InterfaceBinding::binding_model_type;
  using interface_type = typename InterfaceBinding::interface_type;
  using raw_interface_type = typename annotated_traits<interface_type>::type;
  using storage_type = typename binding_model_type::storage_type;
  using resolutions =
      typename binding_request_resolutions<Request, raw_interface_type,
                                           binding_model_type>::type;

public:
  using type = typename matching_binding_resolution<Request, resolutions>::type;

  static constexpr bool value = !std::is_void_v<type>;
};

template <typename Request, typename InterfaceBinding>
inline constexpr bool binding_supports_request_v =
    binding_supports_request<Request, InterfaceBinding>::value;

// Whether a static binding serves the request directly. A runtime key has no
// static selection, so it is never served statically.
template <typename Bindings, typename T, typename LookupKey>
constexpr bool static_request_present_v = [] {
  if constexpr (!is_static_lookup_key_definition_v<LookupKey>) {
    return false;
  } else {
    using request = request_type<std::remove_cv_t<T>, false>;
    using selection =
        typename Bindings::template selection<typename request::lookup_type,
                                              LookupKey>;
    return selection::status != binding_status::not_found;
  }
}();

template <typename Bindings, typename T, typename LookupKey>
constexpr bool static_request_served_v = [] {
  if constexpr (!is_static_lookup_key_definition_v<LookupKey>) {
    return false;
  } else {
    using request = request_type<std::remove_cv_t<T>, false>;
    using selection =
        typename Bindings::template selection<typename request::lookup_type,
                                              LookupKey>;
    if constexpr (selection::status == binding_status::ambiguous) {
      // Let the normal path report the ambiguity.
      return true;
    } else if constexpr (selection::status == binding_status::found) {
      return binding_supports_request_v<typename request::interface_type,
                                        typename selection::binding_type>;
    } else {
      return false;
    }
  }
}();

template <typename State, typename Host, typename BindingModel>
struct static_binding_policy {
  using storage_type = typename BindingModel::storage_type;

  State &state;
  Host &host;
  construction_scope scope;

  auto &storage() {
    return state.template get_storage_for_model<BindingModel>();
  }

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
    if constexpr (context_lvalue) {
      return with_context_lvalue_source(source_scope, context, storage,
                                        activation, std::forward<Fn>(fn));
    } else if constexpr (is_runtime_context_v<Context>) {
      return with_runtime_binding_source(source_scope, context, storage,
                                         activation, std::forward<Fn>(fn));
    } else if constexpr (!context_supports_construct<Context>::value) {
      if constexpr (retained_source) {
        return with_retained_binding_source(source_scope, context, storage,
                                            activation, std::forward<Fn>(fn));
      } else {
        return with_binding_source(source_scope, context, storage, activation,
                                   std::forward<Fn>(fn));
      }
    } else {
      using materialization_traits =
          storage_materialization_traits<typename StorageType::tag_type,
                                         typename StorageType::type>;
      if constexpr (has_context_materialize_source<materialization_traits,
                                                   Context, StorageType,
                                                   Activation>::value) {
        return with_runtime_binding_source(source_scope, context, storage,
                                           activation, std::forward<Fn>(fn));
      } else if constexpr (retained_source) {
        return with_retained_binding_source(source_scope, context, storage,
                                            activation, std::forward<Fn>(fn));
      } else {
        return with_binding_source(source_scope, context, storage, activation,
                                   std::forward<Fn>(fn));
      }
    }
  }

  template <typename Context, typename StorageType, typename Activation,
            typename Fn>
  decltype(auto) with_value_source(construction_scope source_scope,
                                   Context &context, StorageType &storage,
                                   Activation &activation, Fn &&fn) {
    return with_binding_source(source_scope, context, storage, activation,
                               std::forward<Fn>(fn));
  }

  binding_activation<State, Host, BindingModel> activation() const {
    return {state, host, scope};
  }

  static constexpr type_descriptor registered_type() {
    return detail::binding_registered_type<
        typename BindingModel::storage_type>();
  }
};

template <typename State, typename Host, typename InterfaceBinding>
struct static_binding_resolver
    : binding_core<static_binding_policy<
          State, Host, typename InterfaceBinding::binding_model_type>> {
  using binding_model_type = typename InterfaceBinding::binding_model_type;
  using interface_type = typename InterfaceBinding::interface_type;
  using raw_interface_type = typename annotated_traits<interface_type>::type;
  using storage_type = typename binding_model_type::storage_type;

  using policy_type = static_binding_policy<State, Host, binding_model_type>;
  using core_type = binding_core<policy_type>;

  static_binding_resolver(State &state, Host &host, construction_scope scope)
      : core_type(policy_type{state, host, scope}) {}

  static constexpr type_descriptor registered_type() {
    return detail::binding_registered_type<storage_type>();
  }

  template <typename Request, typename Context>
  Request resolve(Context &context) {
    static_assert(is_runtime_context_v<Context> ||
                      binding_supports_request_v<Request, InterfaceBinding>,
                  "static resolution cannot satisfy a request the storage "
                  "does not publish");
    using matched_resolution =
        typename binding_supports_request<Request, InterfaceBinding>::type;
    if constexpr (!std::is_void_v<matched_resolution> &&
                  !std::is_reference_v<Request> &&
                  !std::is_pointer_v<Request>) {
      return this->template consume_request<Request, matched_resolution>(
          context, [](auto &&instance) -> Request {
            return Request(std::forward<decltype(instance)>(instance));
          });
    } else {
      void *ptr = nullptr;
      if constexpr (!std::is_void_v<matched_resolution>) {
        ptr = this->template resolve_address<Request, matched_resolution>(
            context);
      } else {
        throw detail::make_type_not_convertible_exception(
            describe_type<Request>(), registered_type(), context);
      }
      return detail::convert_resolved_binding<Request>(ptr);
    }
  }

  template <typename Request, typename Context, typename Fn>
  decltype(auto) consume(Context &context, Fn &&fn) {
    static_assert(is_runtime_context_v<Context> ||
                      binding_supports_request_v<Request, InterfaceBinding>,
                  "static resolution cannot satisfy a request the storage "
                  "does not publish");
    using matched_resolution =
        typename binding_supports_request<Request, InterfaceBinding>::type;

    if constexpr (!std::is_void_v<matched_resolution>) {
      return this->template consume_request<Request, matched_resolution>(
          context, std::forward<Fn>(fn));
    } else {
      throw detail::make_type_not_convertible_exception(
          describe_type<Request>(), registered_type(), context);
    }
  }
};

template <typename T, typename BindingModel, typename State, typename Host,
          typename Context>
decltype(auto) evaluate_static_binding(construction_scope scope, State &state,
                                       Host &host, Context &context) {
  static_binding_policy<State, Host, BindingModel> policy{state, host, scope};
  binding_core<decltype(policy)> core{policy};
  return core.template resolve_value<T>(context);
}

template <typename InterfaceBinding, typename State, typename Host>
auto make_static_binding_resolver(construction_scope scope, State &state,
                                  Host &host) {
  return static_binding_resolver<State, Host, InterfaceBinding>{state, host,
                                                                scope};
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
      auto resolver = make_static_binding_resolver<binding>(scope, state, host);
      if constexpr (is_copy_constructible_v<resolve_type> &&
                    !std::is_reference_v<resolve_type>) {
        resolver.template consume<resolve_type>(context, [&](auto &&value) {
          fn(results, std::forward<decltype(value)>(value));
        });
      } else {
        fn(results, resolver.template resolve<resolve_type>(context));
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

template <typename T, typename LookupKey, typename StaticRegistryType,
          typename State, typename Host, typename Context>
T construct_static_collection_default_impl(construction_scope scope,
                                           State &state, Host &host,
                                           Context &context) {
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

  if constexpr (collection_type::has_fixed_size_construct) {
    T results =
        collection_type::make_fixed_size(type_list_size_v<interface_bindings>);
    std::size_t index = 0;
    append_static_collection_impl<T, LookupKey, StaticRegistryType>(
        scope, state, results, host, context, [&](auto &, auto &&value) {
          collection_type::set(results, index,
                               std::forward<decltype(value)>(value));
          ++index;
        });
    return results;
  } else {
    return construct_static_collection_impl<T, LookupKey, StaticRegistryType>(
        scope, state, host, context, [](auto &collection, auto &&value) {
          collection_type::add(collection,
                               std::forward<decltype(value)>(value));
        });
  }
}

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
      private static_storage<Registrations...> {
public:
  using static_storage<Registrations...>::get_conversion_cache_for_model;
  using static_storage<Registrations...>::get_local_scope_for_model;
  using static_storage<Registrations...>::get_storage;
  using static_storage<Registrations...>::get_storage_for_model;
};

template <typename... Registrations>
using static_storage_state = static_storage<Registrations...>;

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
