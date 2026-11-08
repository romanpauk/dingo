//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/base/config.h>
#include <dingo/base/construction_scope.h>
#include <dingo/base/none.h>
#include <dingo/binding/binding_collection.h>
#include <dingo/binding/request_resolution.h>
#include <dingo/factory/callable.h>
#include <dingo/factory/constructor.h>
#include <dingo/lookup/lookup_key.h>
#include <dingo/type/collection_traits.h>
#include <dingo/type/request.h>
#include <dingo/type/shape.h>

#include <type_traits>
#include <utility>

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4702)
#endif

namespace dingo::detail {

// Whether a request without a runtime key carries a key in its type.
template <typename T, typename IdType>
inline constexpr bool keyed_by_type_v = [] {
  if constexpr (is_none_v<std::decay_t<IdType>> && is_selected_v<T>) {
    return is_value_selector_v<selected_selector_t<T>>;
  } else {
    return false;
  }
}();

// The requests a container answers: resolve, construct, invoke and the
// collections. A container derives from it and provides
// resolve_entry<Request, R>(lookup_key), construct_collection<T>(fn,
// lookup_key), count_collection<T>(lookup_key) and with_context(fn), which
// runs fn with a fresh resolution context, and is the host of
// request_resolution.
// resolve<T &&>() returns the T it moves out of the object the request refers
// to, which a T that can be neither moved nor copied cannot be. The request
// may be annotated.
template <typename T, typename R>
inline constexpr bool movable_rvalue_request_v =
    !std::is_rvalue_reference_v<::dingo::unannotated_t<T>> ||
    std::is_reference_v<R> || std::is_pointer_v<R> ||
    std::is_move_constructible_v<std::remove_cv_t<R>>;

template <typename Container> class container_requests {
  template <typename Collection, typename Fn>
  using collection_insert_invocable =
      std::is_invocable<Fn &, Collection &,
                        typename collection_traits<Collection>::resolve_type>;

  Container &self() { return static_cast<Container &>(*this); }

public:
  // A request with a key in its type, dependency<T, key_type<Key, Value>>, is
  // looked up under that key, as a dependency of the request is.
  template <typename T, typename IdType = none_t,
            typename R = request_result_t<T, true>,
            std::enable_if_t<!is_lookup_key_v<std::decay_t<IdType>>, int> = 0>
  DINGO_ALWAYS_INLINE R resolve(IdType &&id = IdType()) {
    if constexpr (keyed_by_type_v<T, IdType>) {
      (void)id;
      return resolve<selected_type_t<T>>(
          make_lookup_key(selected_selector_t<T>{}));
    } else if constexpr (!movable_rvalue_request_v<T, R>) {
      static_assert(movable_rvalue_request_v<T, R>,
                    "resolve<T &&>() moves the value out of the object it "
                    "refers to; a value that can be neither copied nor "
                    "moved is requested by reference or pointer, or "
                    "constructed");
    } else {
      return self().template resolve_entry<request_type<T>, R>(
          make_lookup_key(std::forward<IdType>(id)));
    }
  }

  template <typename T, typename LookupKey,
            typename R = request_result_t<T, true>,
            std::enable_if_t<is_lookup_key_v<LookupKey>, int> = 0>
  DINGO_ALWAYS_INLINE R resolve(LookupKey key) {
    if constexpr (!movable_rvalue_request_v<T, R>) {
      static_assert(movable_rvalue_request_v<T, R>,
                    "resolve<T &&>() moves the value out of the object it "
                    "refers to; a value that can be neither copied nor "
                    "moved is requested by reference or pointer, or "
                    "constructed");
    } else {
      return self().template resolve_entry<request_type<T>, R>(std::move(key));
    }
  }

  // A dependency of a factory, resolved in the context of the request that
  // constructs it. Origin is the container the request started at; the host
  // resolves the request through resolve_request.
  template <typename T, bool RemoveRvalueReferences, typename Context,
            typename LookupKey,
            typename R = request_result_t<T, RemoveRvalueReferences>,
            std::enable_if_t<is_lookup_key_v<LookupKey>, int> = 0,
            typename Scope>
  DINGO_ALWAYS_INLINE R resolve(Scope scope, Context &context, LookupKey key) {
    return resolve<T, RemoveRvalueReferences>(scope, context, self(),
                                              std::move(key));
  }

  template <typename T, bool RemoveRvalueReferences, typename Context,
            typename Origin, typename LookupKey,
            typename R = request_result_t<T, RemoveRvalueReferences>,
            std::enable_if_t<is_lookup_key_v<LookupKey>, int> = 0,
            typename Scope>
  DINGO_ALWAYS_INLINE R resolve(Scope scope, Context &context, Origin &origin,
                                LookupKey key) {
    return self()
        .template resolve_request<request_type<T, RemoveRvalueReferences>, R>(
            scope, context, origin, std::move(key));
  }

  // A reference is not constructed: it refers to an instance the container
  // resolves, as resolve<T>() does; an rvalue reference moves out of one.
  // Nor is a request with a key in its type: the key names a binding. A
  // factory makes a value, which an rvalue reference request does not take.
  template <typename T, typename Factory = constructor<normalized_type_t<T>>,
            typename R = request_result_t<T, true>>
  DINGO_ALWAYS_INLINE R construct(Factory factory = Factory()) {
    constexpr bool rvalue =
        std::is_rvalue_reference_v<::dingo::unannotated_t<T>>;
    constexpr bool default_factory =
        std::is_same_v<Factory, constructor<normalized_type_t<T>>>;
    if constexpr ((std::is_lvalue_reference_v<R> || rvalue ||
                   keyed_by_type_v<T, none_t>) &&
                  default_factory) {
      (void)factory;
      return resolve<T>();
    } else if constexpr (rvalue) {
      static_assert(!rvalue, "construct<T &&>(factory) cannot take what the "
                             "factory makes; construct<T>(factory) does");
    } else {
      return self().with_context([&](auto &context) -> R {
        // What construct makes is returned to the caller, as are the
        // references it holds.
        return request_resolution::construct<request_type<T>, R>(
            self(), self().entry_scope(caller_scope), context, factory);
      });
    }
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

  // Returns what the callable returns, a reference included; what the
  // reference refers to is the callable's to keep alive.
  template <typename Signature = void, typename Callable>
  decltype(auto) invoke(Callable &&callable) {
    using callable_type = std::remove_cv_t<std::remove_reference_t<Callable>>;
    using dispatch_signature =
        callable_dispatch_signature_t<Signature, callable_type>;
    return self().with_context([&](auto &context) -> decltype(auto) {
      auto type_guard = context.template track_type<callable_type>();
      return callable_invoke<dispatch_signature>::construct(
          std::forward<Callable>(callable), self().entry_scope(ephemeral_scope),
          context, self());
    });
  }
};

} // namespace dingo::detail

#ifdef _MSC_VER
#pragma warning(pop)
#endif
