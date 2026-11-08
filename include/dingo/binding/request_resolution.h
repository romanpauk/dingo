//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/base/config.h>
#include <dingo/base/construction_scope.h>
#include <dingo/base/exceptions.h>
#include <dingo/base/meta.h>
#include <dingo/base/none.h>
#include <dingo/binding/binding_collection.h>
#include <dingo/binding/binding_resolution.h>
#include <dingo/binding/binding_selection.h>
#include <dingo/factory/auto_constructible.h>
#include <dingo/factory/constructor.h>
#include <dingo/lookup/lookup_key.h>
#include <dingo/type/collection_traits.h>
#include <dingo/type/request.h>
#include <dingo/type/shape.h>
#include <dingo/type/type_traits.h>

#include <cstddef>
#include <type_traits>
#include <utility>

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4702)
#endif

namespace dingo::detail {

// Whether a request that no container binds may be constructed in place.
// Keep constructibility queries in the non-reference specialization. MSVC
// otherwise evaluates its constructibility intrinsics for incomplete reference
// targets while instantiating the dispatch trait.
template <typename T,
          bool IsNormalized =
              std::is_same_v<request_value_t<T>, std::decay_t<T>>,
          bool IsReference = std::is_reference_v<T>>
struct may_auto_construct : std::false_type {};

template <typename T>
struct may_auto_construct<T, true, false>
    : std::bool_constant<std::is_move_constructible_v<std::decay_t<T>> ||
                         std::is_copy_constructible_v<std::decay_t<T>>> {};

template <typename T>
struct may_auto_construct<T, true, true>
    : std::bool_constant<std::is_lvalue_reference_v<T> &&
                         std::is_const_v<std::remove_reference_t<T>> &&
                         is_auto_constructible<std::decay_t<T>>::value> {};

template <typename T>
inline constexpr bool may_auto_construct_v = may_auto_construct<T>::value;

template <typename Selection>
struct is_runtime_binding_selection : std::false_type {};

template <typename Binding>
struct is_runtime_binding_selection<runtime_binding_selection<Binding>>
    : std::true_type {};

// A static selection carries its status in its type.
template <typename Selection>
constexpr auto is_bound(const Selection &selection) {
  if constexpr (is_runtime_binding_selection<Selection>::value) {
    return selection.status != binding_status::not_found;
  } else {
    return std::bool_constant<Selection::status != binding_status::not_found>{};
  }
}

template <typename Count> struct is_constant_count : std::false_type {};

template <std::size_t Count>
struct is_constant_count<std::integral_constant<std::size_t, Count>>
    : std::true_type {};

// Whether the default factory makes an unbound request: the class is
// detected, and an array it makes can be initialized. The argument detection
// probes with converts as the one that constructs it does. An annotated value
// is made of the value it annotates, which is copied or moved.
template <typename Request, typename R,
          bool Factory =
              construct_factory_request_v<typename Request::user_type>>
struct default_factory_makes : std::true_type {};

template <typename Request, typename R>
struct default_factory_makes<Request, R, true> {
  using value_type = typename Request::value_type;
  using annotated_type = unannotated_t<value_type>;
  static constexpr bool annotated_is_made =
      std::is_same_v<annotated_type, value_type> ||
      std::is_move_constructible_v<annotated_type>;
  static constexpr bool value = [] {
    if constexpr (annotated_is_made &&
                  constructor<value_type>::kind == constructor_kind::concrete) {
      return handle_makeable_by<
          std::decay_t<R>, constructor_argument<value_type, constructor_shape>,
          std::make_index_sequence<constructor<value_type>::arity>>::value;
    } else {
      return false;
    }
  }();
};

// The request algorithm of the containers. A container is the host of the
// algorithm; it provides the binding table and the fallbacks:
//
// - select_request<Request>(key): a selection of its bindings
// - resolve_selected<Request, R>(selection, scope, context)
// - ambiguous<Request>(context): rejects an ambiguous selection
// - parent_serves<Lookup, LookupKey>() and parent(): the parent that answers
//   requests the host does not bind
// - count_collection<R>(key) and construct_request_collection<R>(scope,
//   context, key), for hosts that construct collections
// - resolve_exact_or_normalized<Request, R>(selection, scope, context): the
//   request itself if its binding publishes it, otherwise resolve_normalized
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
  // A no-key collection that is bound as a type resolves as that binding.
  // Otherwise the collection is constructed from the bindings of its element,
  // and a host without any is answered by the parent or the origin.
  template <typename Request, typename R, typename Host, typename Context,
            typename Origin, typename LookupKey, typename Scope>
  static R resolve(Host &host, Scope scope, Context &context, Origin &origin,
                   LookupKey key) {
    constexpr bool collection = collection_traits<R>::is_collection;
    if constexpr (collection && !is_no_lookup_key_v<LookupKey>) {
      return construct_collection<Request, R>(host, scope, context, origin,
                                              key);
    } else {
      auto selection = host.template select_request<Request>(key);
      return resolve<Request, R, collection>(host, selection, scope, context,
                                             origin, key);
    }
  }

  // A selection resolves its binding. A miss of a collection request
  // constructs the collection when the host constructs collections.
  template <typename Request, typename R, bool Collection = false,
            typename Host, typename Selection, typename Context,
            typename Origin, typename LookupKey, typename Scope>
  static R resolve(Host &host, Selection selection, Scope scope,
                   Context &context, Origin &origin, LookupKey &key) {
    if constexpr (is_runtime_binding_selection<Selection>::value) {
      if (selection.status == binding_status::ambiguous) {
        host.template ambiguous<Request>(context);
      }
      if (selection.status == binding_status::found) {
        return host.template resolve_selected<Request, R>(selection, scope,
                                                          context);
      }
      return resolve_unbound<Request, R, Collection>(host, scope, context,
                                                     origin, key);
    } else if constexpr (Selection::status == binding_status::found) {
      return host.template resolve_selected<Request, R>(selection, scope,
                                                        context);
    } else if constexpr (Selection::status == binding_status::ambiguous) {
      host.template ambiguous<Request>(context);
    } else {
      return resolve_unbound<Request, R, Collection>(host, scope, context,
                                                     origin, key);
    }
  }

  // construct<T>() of a container. A bound type resolves, as its normalized
  // value when its binding does not publish the request itself; an unbound
  // type is constructed by the factory. The paths are separate functions: the
  // one a static selection does not take is never instantiated.
  template <typename Request, typename R, typename Host, typename Context,
            typename Factory, typename Scope>
  DINGO_ALWAYS_INLINE static R construct(Host &host, Scope scope,
                                         Context &context, Factory &factory) {
    using value_type = typename Request::value_type;
    if constexpr (!std::is_same_v<Factory, constructor<value_type>>) {
      return construct_unbound<Request, R>(host, scope, context, factory);
    } else {
      auto key = no_lookup_key();
      auto exact = host.template select_request<Request>(key);
      auto value = host.template select_request<request_type<value_type>>(key);
      if constexpr (is_runtime_binding_selection<decltype(exact)>::value) {
        if (is_bound(exact) || is_bound(value)) {
          return construct_bound<Request, R>(host, exact, scope, context);
        }
        // Whether the request is bound is known only now, so a request the
        // default factory cannot make is not found, rather than rejected
        // while it compiles.
        if constexpr (default_factory_makes<Request, R>::value) {
          return construct_unbound<Request, R>(host, scope, context, factory);
        } else {
          throw_type_not_found(describe_type<typename Request::lookup_type>(),
                               context, lookup_key_index_type(key));
        }
      } else if constexpr (decltype(is_bound(exact))::value ||
                           decltype(is_bound(value))::value) {
        return construct_bound<Request, R>(host, exact, scope, context);
      } else {
        return construct_unbound<Request, R>(host, scope, context, factory);
      }
    }
  }

  // The normalized value of a request, made into the requested wrapper.
  template <typename Request, typename R, typename Host, typename Context,
            typename Scope>
  static R resolve_normalized(Host &host, Scope scope, Context &context) {
    using normalized = request_type<typename Request::value_type>;
    auto &&instance = resolve<normalized, typename normalized::lookup_type>(
        host, scope, context, host, no_lookup_key());
    return detail::convert_type<std::decay_t<typename Request::user_type>,
                                consume>(
        std::forward<decltype(instance)>(instance));
  }

  // A request that no container of the chain binds. A collection is
  // constructed empty if the host declares its lookup; a type is constructed
  // through the root container of the chain, when the request may construct
  // it.
  template <typename Request, bool MayAutoConstruct, typename R, typename Host,
            typename Root, typename Context, typename LookupKey, typename Scope>
  static R unresolved(Host &host, Root &root, Scope scope, Context &context,
                      const LookupKey &key) {
    using value_type = typename Request::value_type;
    if constexpr (collection_traits<R>::is_collection) {
      if constexpr (default_collection_append_v<R>) {
        return host.template construct_request_collection<R>(scope, context,
                                                             key);
      }
    } else if constexpr (MayAutoConstruct &&
                         is_auto_constructible<std::decay_t<
                             typename Request::user_type>>::value) {
      if constexpr (constructor<value_type>::kind ==
                    constructor_kind::concrete) {
        static_assert(is_complete<value_type>::value,
                      "auto-construction requires a complete type");
        return context.template construct<typename Request::interface_type,
                                          constructor_shape>(scope, root);
      }
    }
    throw_type_not_found(describe_type<typename Request::lookup_type>(),
                         context, lookup_key_index_type(key));
  }

private:
  template <typename Request, typename R, typename Host, typename Selection,
            typename Context, typename Scope>
  static R construct_bound(Host &host, Selection &exact, Scope scope,
                           Context &context) {
    using user_type = typename Request::user_type;
    if constexpr (construct_normalized_request_v<user_type>) {
      return host.template resolve_exact_or_normalized<Request, R>(exact, scope,
                                                                   context);
    } else {
      return resolve<Request, R>(host, scope, context, host, no_lookup_key());
    }
  }

  template <typename Request, typename R, typename Host, typename Context,
            typename Factory, typename Scope>
  static R construct_unbound(Host &host, Scope scope, Context &context,
                             Factory &factory) {
    using user_type = typename Request::user_type;
    if constexpr (construct_factory_request_v<user_type>) {
      auto type_guard =
          context.template track_type<typename Request::value_type>();
      return factory.template construct<R>(scope, context, host);
    } else {
      return resolve<Request, R>(host, scope, context, host, no_lookup_key());
    }
  }

  template <typename Request, typename R, bool Collection, typename Host,
            typename Context, typename Origin, typename LookupKey,
            typename Scope>
  static R resolve_unbound(Host &host, Scope scope, Context &context,
                           Origin &origin, LookupKey &key) {
    if constexpr (Collection) {
      return construct_collection<Request, R>(host, scope, context, origin,
                                              key);
    } else {
      return resolve_missing<Request, R,
                             may_auto_construct_v<typename Request::user_type>,
                             typename Request::lookup_type>(
          host, scope, context, origin, key);
    }
  }

  template <typename Request, typename R, typename Host, typename Context,
            typename Origin, typename LookupKey, typename Scope>
  static R construct_collection(Host &host, Scope scope, Context &context,
                                Origin &origin, LookupKey &key) {
    auto count = host.template count_collection<R>(key);
    if constexpr (is_constant_count<decltype(count)>::value) {
      if constexpr (decltype(count)::value == 0) {
        return resolve_missing<Request, R, false,
                               typename collection_traits<R>::resolve_type>(
            host, scope, context, origin, key);
      } else {
        return construct_bound_collection<Request, R>(host, scope, context,
                                                      origin, key);
      }
    } else {
      if (count == 0) {
        return resolve_missing<Request, R, false,
                               typename collection_traits<R>::resolve_type>(
            host, scope, context, origin, key);
      }
      return construct_bound_collection<Request, R>(host, scope, context,
                                                    origin, key);
    }
  }

  template <typename Request, typename R, typename Host, typename Context,
            typename Origin, typename LookupKey, typename Scope>
  static R construct_bound_collection(Host &host, Scope scope, Context &context,
                                      Origin &origin, LookupKey &key) {
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
            typename LookupKey, typename Scope>
  static R resolve_missing(Host &host, Scope scope, Context &context,
                           Origin &origin, LookupKey &key) {
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

} // namespace dingo::detail

#ifdef _MSC_VER
#pragma warning(pop)
#endif
