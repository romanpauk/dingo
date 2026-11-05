//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/base/construction_scope.h>
#include <dingo/binding/binding_collection.h>
#include <dingo/binding/binding_selection.h>
#include <dingo/type/collection_traits.h>
#include <dingo/type/key.h>

#include <cstddef>
#include <type_traits>
#include <utility>

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4702)
#endif

namespace dingo {
namespace detail {

template <typename Selection>
struct is_runtime_binding_selection : std::false_type {};

template <typename Binding>
struct is_runtime_binding_selection<runtime_binding_selection<Binding>>
    : std::true_type {};

template <typename Count> struct is_constant_count : std::false_type {};

template <std::size_t Count>
struct is_constant_count<std::integral_constant<std::size_t, Count>>
    : std::true_type {};

// The request algorithm of the containers. A container is the host of the
// algorithm; it provides the binding table and the fallbacks:
//
// - select_request<Request>(key): a selection of its bindings
// - resolve_selected<Request, R>(selection, scope, context)
// - ambiguous<Request>(context): rejects an ambiguous selection
// - parent_serves<Lookup, LookupKey>() and parent(): the parent that answers
//   requests the host does not bind
// - collection_count<R>(key) and construct_request_collection<R>(scope,
//   context, key), for hosts that construct collections
//
// The origin of a request is the container it entered through; it answers the
// requests that no container of the chain binds.
//
// A static selection and a static collection count carry their value in their
// type, so the algorithm branches on them at compile time and instantiates
// only the branch they select. Every branch calls the host directly: a static
// dependency chain instantiates the algorithm once per link, and each call
// level counts against the template depth of the chain.
struct request_resolution {
  template <typename Request, typename R, bool MayAutoConstruct, typename Host,
            typename Context, typename Origin, typename LookupKey>
  static R resolve(Host &host, construction_scope scope, Context &context,
                   Origin &origin, LookupKey key) {
    auto selection = host.template select_request<Request>(key);
    using selection_type = decltype(selection);
    if constexpr (is_runtime_binding_selection<selection_type>::value) {
      return resolve<Request, R, MayAutoConstruct>(host, selection, scope,
                                                   context, origin, key);
    } else if constexpr (selection_type::status == binding_status::found) {
      return host.template resolve_selected<Request, R>(selection, scope,
                                                        context);
    } else if constexpr (selection_type::status == binding_status::ambiguous) {
      host.template ambiguous<Request>(context);
    } else {
      return resolve_missing<Request, R, MayAutoConstruct,
                             typename Request::lookup_type>(
          host, scope, context, origin, key);
    }
  }

  template <typename Request, typename R, bool MayAutoConstruct, typename Host,
            typename Binding, typename Context, typename Origin,
            typename LookupKey>
  static R resolve(Host &host, runtime_binding_selection<Binding> selection,
                   construction_scope scope, Context &context, Origin &origin,
                   LookupKey &key) {
    if (selection.status == binding_status::ambiguous) {
      host.template ambiguous<Request>(context);
    }
    if (selection.status == binding_status::found) {
      return host.template resolve_selected<Request, R>(selection, scope,
                                                        context);
    }
    return resolve_missing<Request, R, MayAutoConstruct,
                           typename Request::lookup_type>(host, scope, context,
                                                          origin, key);
  }

  // A no-key collection that is bound as a type resolves as that binding.
  // Otherwise the collection is constructed from the bindings of its element,
  // and a host without any is answered by the parent or the origin.
  template <typename Request, typename R, typename Host, typename Context,
            typename Origin, typename LookupKey>
  static R resolve_collection(Host &host, construction_scope scope,
                              Context &context, Origin &origin, LookupKey key) {
    if constexpr (is_no_lookup_key_v<LookupKey>) {
      auto selection = host.template select_request<Request>(key);
      using selection_type = decltype(selection);
      if constexpr (is_runtime_binding_selection<selection_type>::value) {
        if (selection.status == binding_status::ambiguous) {
          host.template ambiguous<Request>(context);
        }
        if (selection.status == binding_status::found) {
          return host.template resolve_selected<Request, R>(selection, scope,
                                                            context);
        }
      } else if constexpr (selection_type::status == binding_status::found) {
        return host.template resolve_selected<Request, R>(selection, scope,
                                                          context);
      } else if constexpr (selection_type::status ==
                           binding_status::ambiguous) {
        host.template ambiguous<Request>(context);
      }
    }

    auto count = host.template collection_count<R>(key);
    if constexpr (is_constant_count<decltype(count)>::value) {
      if constexpr (decltype(count)::value == 0) {
        return resolve_missing<Request, R, false,
                               typename collection_traits<R>::resolve_type>(
            host, scope, context, origin, key);
      } else {
        return construct_collection<Request, R>(host, scope, context, origin,
                                                key);
      }
    } else {
      if (count == 0) {
        return resolve_missing<Request, R, false,
                               typename collection_traits<R>::resolve_type>(
            host, scope, context, origin, key);
      }
      return construct_collection<Request, R>(host, scope, context, origin,
                                              key);
    }
  }

private:
  template <typename Request, typename R, typename Host, typename Context,
            typename Origin, typename LookupKey>
  static R construct_collection(Host &host, construction_scope scope,
                                Context &context, Origin &origin,
                                LookupKey &key) {
    if constexpr (default_collection_append_v<R>) {
      return host.template construct_request_collection<R>(scope, context, key);
    } else {
      return origin.template unresolved<Request, false, LookupKey, R>(
          scope, context, key);
    }
  }

  // The parent answers a miss only when it declares the lookup; the origin
  // answers the misses of the whole chain.
  template <typename Request, typename R, bool MayAutoConstruct,
            typename Lookup, typename Host, typename Context, typename Origin,
            typename LookupKey>
  static R resolve_missing(Host &host, construction_scope scope,
                           Context &context, Origin &origin, LookupKey &key) {
    if constexpr (Host::template parent_serves<Lookup, LookupKey>()) {
      if (auto *parent = host.parent()) {
        return parent->template resolve<typename Request::user_type,
                                        Request::removes_rvalue_references>(
            scope, context, origin, key);
      }
    }
    return origin.template unresolved<Request, MayAutoConstruct, LookupKey, R>(
        scope, context, key);
  }
};

} // namespace detail
} // namespace dingo

#ifdef _MSC_VER
#pragma warning(pop)
#endif
