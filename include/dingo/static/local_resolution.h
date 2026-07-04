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

  template <typename T, typename Key>
  using binding_t = static_binding_t<
      typename static_registry_type::template bindings<T, Key>>;

  template <typename T, bool RemoveRvalueReferences, typename Key,
            typename Request>
  struct local_binding_source {
    using binding = binding_t<Request, Key>;
    static constexpr bool can_resolve =
        binding::status == binding_status::found;

    self_type &self;

    constexpr binding_status status() const { return binding::status; }

    template <typename ResolveRequest>
    decltype(auto) resolve(runtime_context &context) {
      return self.template resolve_binding<Request, Key>(context);
    }
  };

  template <typename T, bool RemoveRvalueReferences, typename Key,
            typename Request>
  struct host_binding_source {
    static constexpr bool can_resolve = true;

    Host &host;

    binding_status status() const {
      return host.template binding_status<Request, Key>();
    }

    template <typename ResolveRequest>
    decltype(auto) resolve(runtime_context &context) {
      if constexpr (std::is_void_v<Key>) {
        return host.template resolve<T, RemoveRvalueReferences>(context);
      } else {
        return host.template resolve<T, RemoveRvalueReferences>(
            context, key_type<Key>{});
      }
    }
  };

  template <typename Request, typename Key>
  typename annotated_traits<Request>::type
  resolve_binding(runtime_context &context) {
    using selection = binding_t<Request, Key>;
    using binding = typename selection::binding_type;
    auto resolver = this->template make_binding_resolver<binding>(*this);
    return resolver.template resolve<Request>(context);
  }

  template <typename T, typename Key, typename Fn>
  T construct_collection(runtime_context &context, Fn &&fn) {
    using collection_type = collection_traits<T>;
    using resolve_type = typename collection_type::resolve_type;

    constexpr std::size_t static_count =
        type_list_size_v<typename static_registry_type::template bindings<
            normalized_type_t<resolve_type>, Key>>;
    return detail::construct_binding_collection<T>(
        [&] { return host_->template count_collection<T, Key>(); },
        [&] { return static_count; },
        [&](auto &results, auto &&append) {
          host_->template append_collection<T, Key>(
              results, context, std::forward<decltype(append)>(append));
        },
        [&](auto &results, auto &&append) {
          this->template append_static_collection<T, Key, static_registry_type>(
              results, *this, context, std::forward<decltype(append)>(append));
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
  binding_resolution(Host *host, Allocator &&, context_closure_base &closure)
      : state_type(closure), host_(host) {}

  allocator_type &get_allocator() { return host_->get_allocator(); }

  template <typename T, bool RemoveRvalueReferences, typename Key = void,
            typename R = resolve_dependency_result_t<T, RemoveRvalueReferences>>
  R resolve(runtime_context &context) {
    if constexpr (collection_traits<R>::is_collection) {
      return construct_collection<R, Key>(context,
                                          detail::binding_collection_append{});
    } else {
      using request_type = R;
      local_binding_source<T, RemoveRvalueReferences, Key, request_type> local{
          *this};
      host_binding_source<T, RemoveRvalueReferences, Key, request_type> host{
          *host_};
      auto sources = detail::make_two_binding_sources(
          local, host, host, detail::binding_resolution_policy::prefer_primary);
      return detail::resolve_from_binding_sources<T, request_type>(context,
                                                                   sources);
    }
  }

  template <typename T, bool RemoveRvalueReferences, typename Key,
            typename R = resolve_dependency_t<T, RemoveRvalueReferences>>
  R resolve(runtime_context &context, key_type<Key>) {
    return resolve<T, RemoveRvalueReferences, Key>(context);
  }

private:
  Host *host_;
};

} // namespace detail
} // namespace dingo
