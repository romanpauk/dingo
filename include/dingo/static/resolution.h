//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/base/exceptions.h>
#include <dingo/base/type_descriptor.h>
#include <dingo/base/type_list.h>
#include <dingo/binding/binding_collection.h>
#include <dingo/binding/binding_resolution.h>
#include <dingo/binding/binding_selection.h>
#include <dingo/factory/factory_traits.h>
#include <dingo/static/binding.h>
#include <dingo/static/selection.h>
#include <dingo/type/annotated.h>
#include <dingo/type/collection_traits.h>
#include <dingo/type/dependency_traits.h>
#include <dingo/type/key.h>
#include <dingo/type/normalized_type.h>

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

struct factory_probe_context {
  template <typename T, typename Container>
  T resolve(construction_scope, Container &) = delete;
};

struct factory_probe_container {};

template <typename Factory>
inline constexpr bool factory_has_no_dependencies_v =
    std::is_same_v<typename factory_traits<Factory>::dependencies, type_list<>>;

template <typename Factory, typename Type>
Type construct_factory_value_without_dependencies() {
  factory_probe_context context;
  factory_probe_container container;
  return Factory::template construct<Type>(ephemeral_scope, context, container);
}

template <typename Selection, typename Request,
          bool Enabled = Selection::status == binding_status::found>
struct binding_factory {
  static constexpr bool enabled = false;
};

template <typename Selection, typename Request>
struct binding_factory<Selection, Request, true> {
  using binding_type = typename Selection::binding_type;
  using factory_type = typename binding_type::binding_model_type::factory_type;

  static constexpr bool enabled = factory_has_no_dependencies_v<factory_type>;

  static normalized_type_t<Request> construct() {
    return construct_factory_value_without_dependencies<
        factory_type, normalized_type_t<Request>>();
  }
};

template <typename Request, typename Selection, typename ResolveNormalized>
request_result_t<Request, true>
construct_static_binding_value(ResolveNormalized &&resolve_normalized) {
  if constexpr (binding_factory<Selection, Request>::enabled) {
    return type_traits<std::decay_t<Request>>::make(
        binding_factory<Selection, Request>::construct());
  } else {
    auto &&value = std::forward<ResolveNormalized>(resolve_normalized)();
    return type_traits<std::decay_t<Request>>::make(
        std::forward<decltype(value)>(value));
  }
}

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

template <typename State, typename Host, typename InterfaceBinding>
struct static_binding_resolver {
  using binding_model_type = typename InterfaceBinding::binding_model_type;
  using interface_type = typename InterfaceBinding::interface_type;
  using raw_interface_type = typename annotated_traits<interface_type>::type;
  using storage_type = typename binding_model_type::storage_type;

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
    if constexpr (!std::is_void_v<matched_resolution> &&
                  !std::is_reference_v<Request> &&
                  !std::is_pointer_v<Request>) {
      return consume_request<Request, matched_resolution>(
          context, [](auto &&instance) -> Request {
            return Request(std::forward<decltype(instance)>(instance));
          });
    } else {
      void *ptr = nullptr;
      if constexpr (!std::is_void_v<matched_resolution>) {
        ptr = resolve_request_address<Request, matched_resolution>(context);
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
      return consume_request<Request, matched_resolution>(context,
                                                          std::forward<Fn>(fn));
    } else {
      throw detail::make_type_not_convertible_exception(
          describe_type<Request>(), registered_type(), context);
    }
  }

  // The address is deduced: a deduced return type is instantiated at the call
  // and does not add a level to the template depth of a dependency chain.
  template <typename Request, typename Resolution, typename Context>
  decltype(auto) resolve_request_address(Context &context) {
    using target_type =
        std::remove_reference_t<typename Resolution::target_type>;
    using operation = typename Resolution::operation;

    binding_activation<State, Host, binding_model_type> activation{state, host,
                                                                   scope};
    auto &&storage = state.template get_storage_for_model<binding_model_type>();
    auto materialize = [&](auto &&source) -> void * {
      auto &&instance =
          detail::apply_resolution<Request, Resolution, storage_type>(
              activation, context, std::forward<decltype(source)>(source));
      return detail::result_address<target_type>(
          scope, context, std::forward<decltype(instance)>(instance));
    };

    if constexpr ((std::is_lvalue_reference_v<Request> ||
                   std::is_pointer_v<Request>) &&
                  detail::materializes_value_source_v<
                      std::remove_reference_t<decltype(storage)>, Context,
                      decltype(activation)>) {
      return detail::with_context_lvalue_source(
          scope, context, storage, activation, std::move(materialize));
    } else if constexpr (is_runtime_context_v<Context>) {
      return detail::with_runtime_binding_source(
          scope, context, storage, activation, std::move(materialize));
    } else if constexpr (detail::operation_requires_source_retention_v<
                             operation> &&
                         !std::is_same_v<
                             std::remove_cv_t<std::remove_reference_t<Context>>,
                             no_dependency_context>) {
      return detail::with_retained_binding_source(
          scope, context, storage, activation, std::move(materialize));
    } else {
      return detail::with_binding_source(scope, context, storage, activation,
                                         std::move(materialize));
    }
  }

  template <typename Request, typename Resolution, typename Context,
            typename Fn>
  decltype(auto) consume_request(Context &context, Fn &&fn) {
    binding_activation<State, Host, binding_model_type> activation{state, host,
                                                                   scope};
    auto &&storage = state.template get_storage_for_model<binding_model_type>();
    if constexpr ((std::is_lvalue_reference_v<Request> ||
                   std::is_pointer_v<Request>) &&
                  detail::materializes_value_source_v<
                      std::remove_reference_t<decltype(storage)>, Context,
                      decltype(activation)>) {
      auto materialize = [&](auto &&source) -> decltype(auto) {
        auto &&instance =
            detail::apply_resolution<Request, Resolution, storage_type>(
                activation, context, std::forward<decltype(source)>(source));
        return detail::consume_resolved_binding<Request>(
            std::forward<decltype(instance)>(instance), std::forward<Fn>(fn));
      };
      return detail::with_context_lvalue_source(
          scope, context, storage, activation, std::move(materialize));
    } else if constexpr (is_runtime_context_v<Context>) {
      return detail::consume_runtime_binding_resolution_request<Request,
                                                                Resolution>(
          scope, context, storage, activation, activation,
          std::forward<Fn>(fn));
    } else {
      return detail::consume_binding_resolution_request<Request, Resolution>(
          scope, context, storage, activation, activation,
          std::forward<Fn>(fn));
    }
  }
};

template <typename T, typename BindingModel, typename State, typename Host,
          typename Context>
decltype(auto) evaluate_static_binding(construction_scope scope, State &state,
                                       Host &host, Context &context) {
  binding_activation<State, Host, BindingModel> activation{state, host, scope};
  return detail::with_tracked_binding_source(
      scope, context, state.template get_storage_for_model<BindingModel>(),
      activation, [&](auto &&source) -> decltype(auto) {
        return detail::resolve_binding_value<T>(
            activation, context, std::forward<decltype(source)>(source));
      });
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

  return construct_binding_collection<T>(
      type_list_size_v<interface_bindings>, false, [&](T &results) {
        append_static_collection_impl<T, LookupKey, StaticRegistryType>(
            scope, state, results, host, context, std::forward<Fn>(fn));
      });
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

} // namespace detail
} // namespace dingo

#ifdef _MSC_VER
#pragma warning(pop)
#endif
