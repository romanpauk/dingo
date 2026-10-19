//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/base/construction_scope.h>
#include <dingo/base/exceptions.h>
#include <dingo/base/type_descriptor.h>
#include <dingo/base/type_list.h>
#include <dingo/binding/request.h>
#include <dingo/context/context.h>
#include <dingo/dependency/collection.h>
#include <dingo/dependency/normalized_type.h>
#include <dingo/key/annotated.h>
#include <dingo/key/key.h>
#include <dingo/resolution/resolution.h>
#include <dingo/static/activation/requests.h>
#include <dingo/static/activation/storage.h>
#include <dingo/type/type_traits.h>

#include <cstddef>
#include <type_traits>
#include <utility>

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4702)
#endif

namespace dingo {
namespace detail {

struct no_dependency_context {
  template <typename T, typename Container> T resolve(Container &) = delete;
};

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

} // namespace detail
} // namespace dingo

#ifdef _MSC_VER
#pragma warning(pop)
#endif
