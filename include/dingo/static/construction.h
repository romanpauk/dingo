//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

// The construct rule over a static source: the exact binding, the normalized
// binding wrapped into the request, or the caller's factory. Only the static
// container enters it. Keeping the choice here makes the static-binding half of
// `construct` one rule.

#include <dingo/base/config.h>
#include <dingo/base/construction_scope.h>
#include <dingo/binding/binding_resolution.h>
#include <dingo/binding/binding_selection.h>
#include <dingo/factory/factory_traits.h>
#include <dingo/static/binding.h>
#include <dingo/type/dependency_traits.h>
#include <dingo/type/key.h>
#include <dingo/type/normalized_type.h>

#include <type_traits>
#include <utility>

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

template <typename Request, typename R, typename Context, bool DiagnoseRvalue,
          typename ExactSelection, typename NormalizedSelection,
          typename ExactResolve, typename NormalizedResolve, typename Fallback>
DINGO_ALWAYS_INLINE R construct_from_static_source(
    construction_scope scope, Context &context, ExactResolve &&exact_resolve,
    NormalizedResolve &&normalized_resolve, Fallback &&fallback) {
  (void)scope;
  using user_type = typename Request::user_type;

  constexpr bool has_exact =
      ExactSelection::status != binding_status::not_found;
  constexpr bool has_normalized =
      NormalizedSelection::status != binding_status::not_found;

  if constexpr (has_exact) {
    using binding = typename ExactSelection::binding_type;
    if constexpr (binding_supports_request_v<user_type, binding>) {
      return std::forward<ExactResolve>(exact_resolve)(0);
    }
  }

  if constexpr (has_exact || has_normalized) {
    if constexpr (DiagnoseRvalue &&
                  rvalue_request_requires_explicit_conversion_v<user_type>) {
      throw_missing_rvalue_conversion<user_type>(has_normalized, context);
    } else if constexpr (construct_normalized_request_v<user_type>) {
      return construct_static_binding_value<user_type, NormalizedSelection>(
          [&]() -> decltype(auto) {
            return std::forward<NormalizedResolve>(normalized_resolve)(0);
          });
    } else {
      return std::forward<ExactResolve>(exact_resolve)(0);
    }
  } else {
    return std::forward<Fallback>(fallback)(0);
  }
}

} // namespace detail
} // namespace dingo
