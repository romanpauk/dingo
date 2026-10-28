//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

// The static registry: the compile-time binding source and, after the split,
// the registry that owns its storage. The binding types and their dependency
// rules live in bindings.h; the selection types live in selection.h.

#include <dingo/static/binding.h>
#include <dingo/static/bindings.h>

namespace dingo {
namespace detail {

template <typename State, typename... Registrations>
class static_registry<static_bindings<Registrations...>, State> {
  using bindings_type = static_bindings<Registrations...>;

  template <typename Request, typename LookupKey>
  using selection_t =
      typename bindings_type::template selection<Request, LookupKey>;

public:
  using static_bindings_type = bindings_type;
  using interface_bindings = typename static_bindings_type::interface_bindings;
  template <typename Request, typename LookupKey>
  using selection = selection_t<Request, LookupKey>;

  static_registry() = default;

  // The source protocol: the static registry resolves one named request. It
  // makes no collection or precedence decision; the container engine asks the
  // sources in order and owns those rules.
  template <typename Request, typename LookupKey, typename Host,
            typename Context, typename R = typename Request::result_type>
  R resolve_named(construction_scope construction, Context &context,
                  Host &host) {
    static_assert(is_lookup_key_v<LookupKey>);
    using lookup_type = typename Request::lookup_type;
    using result_type = R;
    using selected = selection_t<lookup_type, LookupKey>;
    return resolve_binding<lookup_type, result_type, selected>(construction,
                                                               context, host);
  }

  template <typename T, typename LookupKey, typename Host, typename Context,
            typename Fn>
  std::size_t collect(construction_scope construction, T &results, Host &host,
                      Context &context, Fn &&fn) {
    static_assert(is_lookup_key_v<LookupKey>);
    return state_
        .template append_static_collection<T, LookupKey, bindings_type>(
            construction, results, host, context, std::forward<Fn>(fn));
  }

  template <typename T, typename LookupKey>
  static constexpr std::size_t fixed_count() {
    static_assert(is_lookup_key_v<LookupKey>);
    return static_collection_binding_count<bindings_type, T, LookupKey>();
  }

  template <typename T, typename LookupKey, typename Host, typename Context>
  T construct_collection(construction_scope construction, Host &host,
                         Context &context) {
    static_assert(is_lookup_key_v<LookupKey>);
    return state_
        .template construct_static_collection<T, LookupKey, bindings_type>(
            construction, host, context);
  }

  template <typename T, typename LookupKey, typename Host, typename Context,
            typename Fn>
  T construct_collection(construction_scope construction, Host &host,
                         Context &context, Fn &&fn) {
    static_assert(is_lookup_key_v<LookupKey>);
    return state_
        .template construct_static_collection<T, LookupKey, bindings_type>(
            construction, host, context, std::forward<Fn>(fn));
  }

  template <typename Request, typename ResolveRequest, typename Selection,
            typename Context, typename Host>
  decltype(auto) resolve_binding(construction_scope construction,
                                 Context &context, Host &host) {
    using binding = typename Selection::binding_type;
    auto resolver =
        state_.template make_binding_resolver<binding>(construction, host);
    return resolver.template resolve<ResolveRequest>(context);
  }

private:
  State state_;
};

} // namespace detail
} // namespace dingo
