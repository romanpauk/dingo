//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/base/complete_type.h>
#include <dingo/base/construction_scope.h>
#include <dingo/base/exceptions.h>
#include <dingo/base/none.h>
#include <dingo/binding/binding_collection.h>
#include <dingo/binding/binding_selection.h>
#include <dingo/factory/callable.h>
#include <dingo/factory/constructor.h>
#include <dingo/type/auto_constructible.h>
#include <dingo/type/collection_traits.h>
#include <dingo/type/key.h>

#include <cstddef>
#include <type_traits>
#include <utility>

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4702)
#endif

namespace dingo::detail {

// The request entry points both containers spell the same way. A container
// provides construct_collection<T>(fn, lookup_key), count_collection<T>(lookup_
// key) and with_context(fn), which runs fn with a fresh resolution context.
template <typename Container> class container_interface {
  template <typename Collection, typename Fn>
  using collection_insert_invocable =
      std::is_invocable<Fn &, Collection &,
                        typename collection_traits<Collection>::resolve_type>;

  Container &self() { return static_cast<Container &>(*this); }

public:
  template <typename T> T construct_collection() {
    return self().template construct_collection<T>(binding_collection_append{},
                                                   no_lookup_key());
  }

  template <typename T, typename Arg,
            std::enable_if_t<!is_lookup_key_v<std::decay_t<Arg>>, int> = 0>
  T construct_collection(Arg &&arg) {
    if constexpr (collection_insert_invocable<T, std::decay_t<Arg>>::value) {
      return self().template construct_collection<T>(std::forward<Arg>(arg),
                                                     no_lookup_key());
    } else {
      return self().template construct_collection<T>(
          binding_collection_append{}, make_lookup_key(std::forward<Arg>(arg)));
    }
  }

  template <typename T, typename Fn, typename IdType,
            std::enable_if_t<!is_lookup_key_v<std::decay_t<IdType>>, int> = 0>
  T construct_collection(Fn &&fn, IdType &&id) {
    return self().template construct_collection<T>(
        std::forward<Fn>(fn), make_lookup_key(std::forward<IdType>(id)));
  }

  template <typename T> std::size_t count_collection() {
    return self().template count_collection<T>(no_lookup_key());
  }

  template <typename T, typename Key> std::size_t count_collection() {
    return self().template count_collection<T>(
        make_lookup_key(type_selector<Key>{}));
  }

  template <typename Signature = void, typename Callable>
  auto invoke(Callable &&callable) {
    using callable_type = std::remove_cv_t<std::remove_reference_t<Callable>>;
    using dispatch_signature =
        callable_dispatch_signature_t<Signature, callable_type>;
    return self().with_context([&](auto &context) {
      auto type_guard = context.template track_type<callable_type>();
      return callable_invoke<dispatch_signature>::construct(
          std::forward<Callable>(callable), ephemeral_scope, context, self());
    });
  }
};

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
// - count_collection<R>(key) and construct_request_collection<R>(scope,
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
  // A no-key collection that is bound as a type resolves as that binding.
  // Otherwise the collection is constructed from the bindings of its element,
  // and a host without any is answered by the parent or the origin.
  template <typename Request, typename R, bool MayAutoConstruct, typename Host,
            typename Context, typename Origin, typename LookupKey>
  static R resolve(Host &host, construction_scope scope, Context &context,
                   Origin &origin, LookupKey key) {
    constexpr bool collection = collection_traits<R>::is_collection;
    if constexpr (collection && !is_no_lookup_key_v<LookupKey>) {
      return construct_collection<Request, R>(host, scope, context, origin,
                                              key);
    } else {
      auto selection = host.template select_request<Request>(key);
      return resolve < Request, R, MayAutoConstruct && !collection,
             collection > (host, selection, scope, context, origin, key);
    }
  }

  // A selection resolves its binding. A miss of a collection request
  // constructs the collection when the host constructs collections.
  template <typename Request, typename R, bool MayAutoConstruct,
            bool Collection = false, typename Host, typename Selection,
            typename Context, typename Origin, typename LookupKey>
  static R resolve(Host &host, Selection selection, construction_scope scope,
                   Context &context, Origin &origin, LookupKey &key) {
    if constexpr (is_runtime_binding_selection<Selection>::value) {
      if (selection.status == binding_status::ambiguous) {
        host.template ambiguous<Request>(context);
      }
      if (selection.status == binding_status::found) {
        return host.template resolve_selected<Request, R>(selection, scope,
                                                          context);
      }
      return resolve_unbound<Request, R, MayAutoConstruct, Collection>(
          host, scope, context, origin, key);
    } else if constexpr (Selection::status == binding_status::found) {
      return host.template resolve_selected<Request, R>(selection, scope,
                                                        context);
    } else if constexpr (Selection::status == binding_status::ambiguous) {
      host.template ambiguous<Request>(context);
    } else {
      return resolve_unbound<Request, R, MayAutoConstruct, Collection>(
          host, scope, context, origin, key);
    }
  }

  // Constructs a type that no container of the chain binds, when the request
  // may construct it, through the root container of the chain.
  template <typename Request, bool MayAutoConstruct, typename R,
            typename Context, typename Root, typename LookupKey>
  static R construct_unbound(construction_scope scope, Context &context,
                             Root &root, const LookupKey &key) {
    using value_type = typename Request::value_type;
    if constexpr (MayAutoConstruct &&
                  is_auto_constructible<
                      std::decay_t<typename Request::user_type>>::value) {
      if constexpr (constructor<value_type>::kind ==
                    constructor_kind::concrete) {
        static_assert(is_complete<value_type>::value,
                      "auto-construction requires a complete type");
        return context.template construct<typename Request::interface_type,
                                          constructor_shape>(scope, root);
      }
    }
    throw make_type_not_found_exception<typename Request::lookup_type>(context,
                                                                       key);
  }

private:
  template <typename Request, typename R, bool MayAutoConstruct,
            bool Collection, typename Host, typename Context, typename Origin,
            typename LookupKey>
  static R resolve_unbound(Host &host, construction_scope scope,
                           Context &context, Origin &origin, LookupKey &key) {
    if constexpr (Collection) {
      return construct_collection<Request, R>(host, scope, context, origin,
                                              key);
    } else {
      return resolve_missing<Request, R, MayAutoConstruct,
                             typename Request::lookup_type>(
          host, scope, context, origin, key);
    }
  }

  template <typename Request, typename R, typename Host, typename Context,
            typename Origin, typename LookupKey>
  static R construct_collection(Host &host, construction_scope scope,
                                Context &context, Origin &origin,
                                LookupKey &key) {
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
            typename Origin, typename LookupKey>
  static R construct_bound_collection(Host &host, construction_scope scope,
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

} // namespace dingo::detail

#ifdef _MSC_VER
#pragma warning(pop)
#endif
