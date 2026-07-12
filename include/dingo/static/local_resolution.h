//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/core/binding_collection.h>
#include <dingo/core/binding_resolution_policy.h>
#include <dingo/core/exceptions.h>
#include <dingo/core/key.h>
#include <dingo/registration/annotated.h>
#include <dingo/runtime/context.h>
#include <dingo/static/activation_set.h>
#include <dingo/static/graph.h>
#include <dingo/static/registry.h>
#include <dingo/type/dependency_traits.h>

#include <type_traits>
#include <utility>

namespace dingo {
namespace detail {

template <typename Host, typename StaticRegistry> class binding_resolution;

template <typename Host, typename... Registrations>
class binding_resolution<Host, static_bindings<Registrations...>>
    : private borrowed_binding_scope<Registrations...> {
  using static_registry_type = static_bindings<Registrations...>;
  using self_type = binding_resolution<Host, static_registry_type>;
  using state_type = borrowed_binding_scope<Registrations...>;
  using runtime_context_type = runtime_context<typename Host::allocator_type>;

  template <typename T, typename LookupKey>
  using binding_t = static_binding_t<
      typename static_registry_type::template bindings<T, LookupKey>>;

  template <typename Request, typename LookupKey, typename Result>
  struct local_binding_source {
    using binding = binding_t<Result, LookupKey>;
    static constexpr bool can_resolve =
        binding::status == binding_status::found;

    self_type &self;
    construction_scope scope;

    constexpr binding_status status() const { return binding::status; }

    template <typename ResolveRequest, typename Context>
    decltype(auto) resolve(Context &context) {
      return self.template resolve_binding<Result, LookupKey>(scope, context);
    }
  };

  template <typename Request, typename LookupKey, typename Result>
  struct host_binding_source {
    static constexpr bool can_resolve = true;

    Host &host;
    construction_scope scope;
    LookupKey key;

    binding_status status() const {
      return host.template binding_status<Result>(key);
    }

    template <typename ResolveRequest>
    decltype(auto) resolve(runtime_context_type &context) {
      return host.template resolve<typename Request::user_type,
                                   Request::removes_rvalue_references>(
          scope, context, std::move(key));
    }
  };

  template <typename Request, typename LookupKey, typename Context>
  typename annotated_traits<Request>::type
  resolve_binding(construction_scope scope, Context &context) {
    using selection = binding_t<Request, LookupKey>;
    using binding = typename selection::binding_type;
    auto resolver = this->template make_binding_resolver<binding>(scope, *this);
    return resolver.template resolve<Request>(context);
  }

  template <typename T, typename LookupKey, typename Fn>
  T construct_collection(construction_scope scope,
                         runtime_context_type &context, Fn &&fn) {
    using collection_type = collection_traits<T>;
    using resolve_type = typename collection_type::resolve_type;

    constexpr std::size_t static_count =
        type_list_size_v<typename static_registry_type::template bindings<
            normalized_type_t<resolve_type>, LookupKey>>;
    return detail::construct_binding_collection<T>(
        [&] { return host_->template count_collection<T>(LookupKey{}); },
        [&] { return static_count; },
        [&](auto &results, auto &&append) {
          host_->template append_collection<T>(
              results, scope, context, std::forward<decltype(append)>(append),
              LookupKey{});
        },
        [&](auto &results, auto &&append) {
          this->template append_static_collection<T, LookupKey,
                                                  static_registry_type>(
              scope, results, *this, context,
              std::forward<decltype(append)>(append));
        },
        std::forward<Fn>(fn));
  }

public:
  using allocator_type = typename Host::allocator_type;

  static_assert(static_registry_type::valid,
                "register_type bindings<...> requires a valid compile-time "
                "bindings source");
  static_assert(detail::graph_analysis<static_registry_type, true>::resolvable,
                "register_type bindings<...> requires a resolvable "
                "compile-time binding graph");
  static_assert((detail::binding_factory_is_default_constructible<
                     detail::binding_model<Registrations>>::value &&
                 ...),
                "register_type bindings<...> requires default-constructible "
                "local factories");
  static_assert((detail::binding_storage_is_default_constructible<
                     detail::binding_model<Registrations>>::value &&
                 ...),
                "register_type bindings<...> requires default-constructible "
                "local storage objects");

  template <typename Allocator>
  binding_resolution(Host *host, Allocator &&) : state_type(), host_(host) {}

  template <typename Allocator>
  binding_resolution(Host *host, Allocator &&allocator,
                     static_context_frame_base &)
      : binding_resolution(host, std::forward<Allocator>(allocator)) {}

  allocator_type &get_allocator() { return host_->get_allocator(); }

  template <typename Request, typename LookupKey,
            typename R = typename Request::result_type,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  R resolve(construction_scope scope, runtime_context_type &context,
            LookupKey key) {
    static_assert(detail::is_lookup_key_v<LookupKey>);
    (void)key;
    if constexpr (collection_traits<R>::is_collection) {
      return construct_collection<R, LookupKey>(
          scope, context, detail::binding_collection_append{});
    } else {
      using result_type = R;
      local_binding_source<Request, LookupKey, result_type> local{*this, scope};
      host_binding_source<Request, LookupKey, result_type> host{*host_, scope,
                                                                std::move(key)};
      auto sources = detail::make_two_binding_sources(
          local, host, host, detail::binding_resolution_policy::prefer_primary);
      return detail::resolve_from_binding_sources<typename Request::lookup_type,
                                                  result_type>(context,
                                                               sources);
    }
  }

  template <typename T, bool RemoveRvalueReferences, typename LookupKey,
            typename R =
                typename request_type<T, RemoveRvalueReferences>::lookup_type,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  R resolve(construction_scope scope, runtime_context_type &context,
            LookupKey key) {
    using request = request_type<T, RemoveRvalueReferences>;
    return resolve<request, LookupKey, R>(scope, context, std::move(key));
  }

private:
  Host *host_;
};

} // namespace detail
} // namespace dingo
