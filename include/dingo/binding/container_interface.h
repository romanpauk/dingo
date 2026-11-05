//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/base/complete_type.h>
#include <dingo/base/config.h>
#include <dingo/base/construction_scope.h>
#include <dingo/base/exceptions.h>
#include <dingo/base/none.h>
#include <dingo/binding/binding_collection.h>
#include <dingo/binding/binding_resolution.h>
#include <dingo/binding/binding_selection.h>
#include <dingo/factory/callable.h>
#include <dingo/factory/constructor.h>
#include <dingo/type/auto_constructible.h>
#include <dingo/type/collection_traits.h>
#include <dingo/type/dependency_traits.h>
#include <dingo/type/key.h>
#include <dingo/type/normalized_type.h>
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
            typename Origin, typename LookupKey>
  static R resolve(Host &host, construction_scope scope, Context &context,
                   Origin &origin, LookupKey key) {
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
            typename Origin, typename LookupKey>
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
            typename Factory>
  DINGO_ALWAYS_INLINE static R construct(Host &host, construction_scope scope,
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
          return construct_bound<Request, R>(host, exact, is_bound(value),
                                             scope, context);
        }
        return construct_unbound<Request, R>(host, scope, context, factory);
      } else if constexpr (decltype(is_bound(exact))::value ||
                           decltype(is_bound(value))::value) {
        return construct_bound<Request, R>(host, exact, is_bound(value), scope,
                                           context);
      } else {
        return construct_unbound<Request, R>(host, scope, context, factory);
      }
    }
  }

  // The normalized value of a request, made into the requested wrapper.
  template <typename Request, typename R, typename Host, typename Context>
  static R resolve_normalized(Host &host, construction_scope scope,
                              Context &context) {
    using normalized = request_type<typename Request::value_type>;
    auto &&instance = resolve<normalized, typename normalized::lookup_type>(
        host, scope, context, host, no_lookup_key());
    return type_traits<std::decay_t<typename Request::user_type>>::make(
        std::forward<decltype(instance)>(instance));
  }

  // A request that no container of the chain binds. A collection is
  // constructed empty if the host declares its lookup; a type is constructed
  // through the root container of the chain, when the request may construct
  // it.
  template <typename Request, bool MayAutoConstruct, typename R, typename Host,
            typename Root, typename Context, typename LookupKey>
  static R unresolved(Host &host, Root &root, construction_scope scope,
                      Context &context, const LookupKey &key) {
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
    throw make_type_not_found_exception<typename Request::lookup_type>(context,
                                                                       key);
  }

private:
  template <typename Request, typename R, typename Host, typename Selection,
            typename Context>
  static R construct_bound(Host &host, Selection &exact, bool value_bound,
                           construction_scope scope, Context &context) {
    using user_type = typename Request::user_type;
    if constexpr (rvalue_request_requires_explicit_conversion_v<user_type>) {
      throw_missing_rvalue_conversion<user_type>(value_bound, context);
    } else if constexpr (construct_normalized_request_v<user_type>) {
      return host.template resolve_exact_or_normalized<Request, R>(exact, scope,
                                                                   context);
    } else {
      return resolve<Request, R>(host, scope, context, host, no_lookup_key());
    }
  }

  template <typename Request, typename R, typename Host, typename Context,
            typename Factory>
  static R construct_unbound(Host &host, construction_scope scope,
                             Context &context, Factory &factory) {
    using user_type = typename Request::user_type;
    if constexpr (construct_factory_request_v<user_type>) {
      auto type_guard =
          context.template track_type<typename Request::value_type>();
      return factory.template construct<R>(scope, context, host);
    } else if constexpr (rvalue_request_requires_explicit_conversion_v<
                             user_type>) {
      throw_missing_rvalue_conversion<user_type>(false, context);
    } else {
      return resolve<Request, R>(host, scope, context, host, no_lookup_key());
    }
  }

  template <typename Request, typename R, bool Collection, typename Host,
            typename Context, typename Origin, typename LookupKey>
  static R resolve_unbound(Host &host, construction_scope scope,
                           Context &context, Origin &origin, LookupKey &key) {
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

// The request entry points of both containers. A container provides
// resolve_entry<Request, R>(lookup_key), construct_collection<T>(fn,
// lookup_key), count_collection<T>(lookup_key) and with_context(fn), which
// runs fn with a fresh resolution context, and is the host of
// request_resolution.
template <typename Container> class container_interface {
  template <typename Collection, typename Fn>
  using collection_insert_invocable =
      std::is_invocable<Fn &, Collection &,
                        typename collection_traits<Collection>::resolve_type>;

  Container &self() { return static_cast<Container &>(*this); }

public:
  template <typename T, typename IdType = none_t,
            typename R = request_result_t<T, true>,
            std::enable_if_t<!is_lookup_key_v<std::decay_t<IdType>>, int> = 0>
  DINGO_ALWAYS_INLINE R resolve(IdType &&id = IdType()) {
    return self().template resolve_entry<request_type<T>, R>(
        make_lookup_key(std::forward<IdType>(id)));
  }

  template <typename T, typename LookupKey,
            typename R = request_result_t<T, true>,
            std::enable_if_t<is_lookup_key_v<LookupKey>, int> = 0>
  DINGO_ALWAYS_INLINE R resolve(LookupKey key) {
    return self().template resolve_entry<request_type<T>, R>(std::move(key));
  }

  template <typename T, typename Factory = constructor<normalized_type_t<T>>,
            typename R = request_result_t<T, true>>
  DINGO_ALWAYS_INLINE R construct(Factory factory = Factory()) {
    return self().with_context([&](auto &context) -> R {
      return request_resolution::construct<request_type<T>, R>(
          self(), ephemeral_scope, context, factory);
    });
  }

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

  template <typename T, typename IdType = none_t,
            std::enable_if_t<!is_lookup_key_v<std::decay_t<IdType>>, int> = 0>
  std::size_t count_collection(IdType &&id = IdType()) {
    return self().template count_collection<T>(
        make_lookup_key(std::forward<IdType>(id)));
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

} // namespace dingo::detail

#ifdef _MSC_VER
#pragma warning(pop)
#endif
