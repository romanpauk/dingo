//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

// What a container does when no source answers a request: the one rule for
// auto-construction, a default collection, and the diagnostics. The containers
// supply their capabilities as a policy so the rule is written once.

#include <dingo/base/complete_type.h>
#include <dingo/base/config.h>
#include <dingo/base/construction_scope.h>
#include <dingo/base/exceptions.h>
#include <dingo/binding/binding_collection.h>
#include <dingo/factory/constructor.h>
#include <dingo/type/auto_constructible.h>
#include <dingo/type/collection_traits.h>

#include <type_traits>
#include <utility>

namespace dingo {
namespace detail {

template <typename...> inline constexpr bool miss_dependent_false_v = false;

// A runtime container, or a mixed container, answers a miss by constructing an
// unregistered type from its detected constructor. A runtime-only container
// reports a missing collection as a missing type.
template <bool RuntimeOnly> struct runtime_miss_policy {
  static constexpr bool runtime_only = RuntimeOnly;
  static constexpr bool auto_constructs = true;
  static constexpr bool constructs_default_collections = true;
  static constexpr bool diagnostics_carry_context = true;
  static constexpr bool missing_is_compile_error = false;
};

// A static container neither auto-constructs nor constructs a default
// collection on a miss, and reports an unbound type when it is compiled.
struct static_miss_policy {
  static constexpr bool runtime_only = false;
  static constexpr bool auto_constructs = false;
  static constexpr bool constructs_default_collections = false;
  static constexpr bool diagnostics_carry_context = false;
  static constexpr bool missing_is_compile_error = true;
};

// Resolves a request no source named. `StaticError` reports an unbound type at
// compile time for a container that cannot ask a parent.
template <typename Policy, bool MayAutoConstruct, bool StaticError,
          typename Request, typename LookupKey, typename R, typename Container,
          typename Context>
DINGO_ALWAYS_INLINE R resolve_missing_request(Container &container,
                                              construction_scope scope,
                                              Context &context, LookupKey key) {
  using Type = typename Request::value_type;
  using user_type = typename Request::user_type;
  (void)container;
  (void)scope;
  (void)context;
  (void)key;

  if constexpr (collection_traits<R>::is_collection) {
    if constexpr (Policy::constructs_default_collections &&
                  default_collection_append_v<R>) {
      return container.template construct_collection<R>(
          scope, context, binding_collection_append{}, std::move(key));
    } else if constexpr (Policy::runtime_only) {
      throw make_type_not_found_exception<typename Request::lookup_type>(
          context, key);
    } else {
      using resolve_type = typename collection_traits<R>::resolve_type;
      throw make_collection_type_not_found_exception<R, resolve_type>();
    }
  } else if constexpr (Policy::missing_is_compile_error && StaticError) {
    static_assert(miss_dependent_false_v<Request, LookupKey>,
                  "static_container cannot resolve an unbound type");
  } else if constexpr (Policy::auto_constructs && MayAutoConstruct &&
                       is_auto_constructible<std::decay_t<user_type>>::value) {
    if constexpr (constructor<Type>::kind == constructor_kind::concrete) {
      static_assert(is_complete<Type>::value,
                    "auto-construction requires a complete type");
      using type_detection = constructor_shape;
      return context
          .template construct<typename Request::interface_type, type_detection>(
              scope, container);
    } else {
      throw make_type_not_found_exception<typename Request::lookup_type>(
          context, key);
    }
  } else {
    if constexpr (Policy::diagnostics_carry_context) {
      throw make_type_not_found_exception<typename Request::lookup_type>(
          context, key);
    } else {
      throw make_type_not_found_exception<typename Request::lookup_type>();
    }
  }
}

} // namespace detail
} // namespace dingo
