//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/base/exceptions.h>
#include <dingo/base/type_descriptor.h>
#include <dingo/binding/binding_activation.h>
#include <dingo/binding/binding_cell.h>
#include <dingo/binding/binding_collection.h>
#include <dingo/binding/binding_model.h>
#include <dingo/binding/binding_resolution.h>
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

template <typename... Registrations> class static_binding_storage;

template <typename Registration> struct binding_cell_slot {
  binding_cell_t<detail::binding_model<Registration>> cell;
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
  static_binding_storage<LocalRegistrations...> scope;
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

// The dependencies of a static binding resolve through its local bindings
// first, then through the host.
template <typename LocalStorage, typename LocalBindings, typename Host>
struct local_binding_router {
  LocalStorage &local;
  Host &host;

  template <typename T, bool RemoveRvalueReferences, typename Context,
            typename LookupKey,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  decltype(auto) resolve(construction_scope scope, Context &context,
                         LookupKey key) {
    using request = request_type<T, RemoveRvalueReferences>;
    using R = typename request::result_type;
    if constexpr (collection_traits<R>::is_collection) {
      using collection_type = collection_traits<R>;
      R results;
      auto append = [](auto &values, auto &&value) {
        collection_type::add(values, std::forward<decltype(value)>(value));
      };
      const auto local_count =
          local.template append_collection<R, LookupKey, LocalBindings>(
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
          typename LocalBindings::template selection<R, LookupKey>;
      if constexpr (selection::status == detail::binding_status::found) {
        return local
            .template resolver<typename selection::binding_type>(scope, host)
            .template resolve<R>(context);
      } else {
        return host.template resolve<typename request::user_type,
                                     request::removes_rvalue_references>(
            scope, context, std::move(key));
      }
    }
  }
};

template <typename State, typename Host, typename BindingModel,
          typename LocalBindings = typename BindingModel::bindings_type>
struct static_binding_dependencies {
  using type = Host &;

  static type make(State &, Host &host) { return host; }
};

template <typename State, typename Host, typename BindingModel,
          typename... LocalRegistrations>
struct static_binding_dependencies<State, Host, BindingModel,
                                   static_bindings<LocalRegistrations...>> {
  using type =
      local_binding_router<static_binding_storage<LocalRegistrations...>,
                           static_bindings<LocalRegistrations...>, Host>;

  static type make(State &state, Host &host) {
    return {state.template get_local_scope_for_model<BindingModel>(), host};
  }
};

// Resolves one interface of a static binding: selects the resolution of a
// request at compile time and applies it through a binding activation.
template <typename State, typename Host, typename InterfaceBinding>
struct static_binding_resolver {
  using binding_model_type = typename InterfaceBinding::binding_model_type;
  using storage_type = typename binding_model_type::storage_type;
  using dependencies_type =
      static_binding_dependencies<State, Host, binding_model_type>;
  using activation_type =
      binding_activation<binding_cell_t<binding_model_type>,
                         typename dependencies_type::type, false>;

  State &state;
  Host &host;
  construction_scope scope;

  static constexpr type_descriptor registered_type() {
    return describe_type<typename storage_type::type>();
  }

  template <typename Request, typename Context>
  Request resolve(Context &context) {
    static_assert(is_runtime_context_v<Context> ||
                      binding_supports_request_v<Request, InterfaceBinding>,
                  "static resolution cannot satisfy a request the storage "
                  "does not publish");
    using matched_resolution =
        typename binding_supports_request<Request, InterfaceBinding>::type;
    if constexpr (std::is_void_v<matched_resolution>) {
      throw detail::make_type_not_convertible_exception(
          describe_type<Request>(), registered_type(), context);
    } else if constexpr (!std::is_reference_v<Request> &&
                         !std::is_pointer_v<Request>) {
      auto activation = make_activation();
      return activation.template consume_request<Request, matched_resolution>(
          context, [](auto &&instance) -> Request {
            return Request(std::forward<decltype(instance)>(instance));
          });
    } else {
      auto activation = make_activation();
      return detail::convert_resolved_binding<Request>(
          activation
              .template resolve_request_address<Request, matched_resolution>(
                  context));
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
      auto activation = make_activation();
      return activation.template consume_request<Request, matched_resolution>(
          context, std::forward<Fn>(fn));
    } else {
      throw detail::make_type_not_convertible_exception(
          describe_type<Request>(), registered_type(), context);
    }
  }

private:
  activation_type make_activation() {
    return {state.template get_cell_for_model<binding_model_type>(),
            dependencies_type::make(state, host), scope};
  }
};

// The storage of a static binding set, and the resolution entry points over
// it. Local bindings of a registration keep their own storage in a slot.
template <typename... Registrations>
class static_binding_storage
    : private binding_cell_slot<Registrations>...,
      private local_binding_scope_slot<Registrations>... {
public:
  template <typename BindingModel> auto &get_cell_for_model() {
    return static_cast<
               binding_cell_slot<typename BindingModel::registration_type> &>(
               *this)
        .cell;
  }

  template <typename BindingModel> auto &get_local_scope_for_model() {
    return static_cast<local_binding_scope_slot<
        typename BindingModel::registration_type> &>(*this)
        .scope;
  }

  template <typename InterfaceBinding, typename Host>
  auto resolver(construction_scope scope, Host &host) {
    return static_binding_resolver<static_binding_storage, Host,
                                   InterfaceBinding>{*this, host, scope};
  }

  template <typename T, typename LookupKey, typename Bindings, typename Host,
            typename Context, typename Fn>
  std::size_t append_collection(construction_scope scope, T &results,
                                Host &host, Context &context, Fn &&fn) {
    static_assert(is_lookup_key_v<LookupKey>);
    using collection_type = collection_traits<T>;
    using resolve_type = typename collection_type::resolve_type;
    using interface_bindings =
        typename Bindings::template bindings<normalized_type_t<resolve_type>,
                                             LookupKey>;

    static_assert(collection_type::is_collection,
                  "missing collection_traits specialization for type T");

    dingo::for_each(interface_bindings{}, [&](auto binding_iterator) {
      using binding = typename decltype(binding_iterator)::type;
      auto binding_resolver = resolver<binding>(scope, host);
      if constexpr (is_copy_constructible_v<resolve_type> &&
                    !std::is_reference_v<resolve_type>) {
        binding_resolver.template consume<resolve_type>(
            context, [&](auto &&value) {
              fn(results, std::forward<decltype(value)>(value));
            });
      } else {
        fn(results, binding_resolver.template resolve<resolve_type>(context));
      }
    });
    return type_list_size_v<interface_bindings>;
  }

  template <typename T, typename LookupKey, typename Bindings, typename Host,
            typename Context, typename Fn>
  T construct_collection(construction_scope scope, Host &host, Context &context,
                         Fn &&fn) {
    constexpr std::size_t count =
        static_collection_binding_count<Bindings, T, LookupKey>();
    static_assert(
        count != 0,
        "static container cannot construct a collection for an unbound type");

    T results;
    collection_traits<T>::reserve(results, count);
    append_collection<T, LookupKey, Bindings>(scope, results, host, context,
                                              std::forward<Fn>(fn));
    return results;
  }

  template <typename T, typename LookupKey, typename Bindings, typename Host,
            typename Context>
  T construct_collection(construction_scope scope, Host &host,
                         Context &context) {
    using collection_type = collection_traits<T>;
    constexpr std::size_t count =
        static_collection_binding_count<Bindings, T, LookupKey>();
    static_assert(
        count != 0,
        "static container cannot construct a collection for an unbound type");

    if constexpr (collection_type::has_fixed_size_construct) {
      T results = collection_type::make_fixed_size(count);
      std::size_t index = 0;
      append_collection<T, LookupKey, Bindings>(
          scope, results, host, context, [&](auto &, auto &&value) {
            collection_type::set(results, index,
                                 std::forward<decltype(value)>(value));
            ++index;
          });
      return results;
    } else {
      return construct_collection<T, LookupKey, Bindings>(
          scope, host, context, [](auto &collection, auto &&value) {
            collection_type::add(collection,
                                 std::forward<decltype(value)>(value));
          });
    }
  }
};

} // namespace detail
} // namespace dingo

#ifdef _MSC_VER
#pragma warning(pop)
#endif
