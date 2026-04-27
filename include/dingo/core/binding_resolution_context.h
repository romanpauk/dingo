//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/core/binding_collection.h>
#include <dingo/core/binding_resolution_status.h>
#include <dingo/core/exceptions.h>
#include <dingo/core/keyed.h>
#include <dingo/core/static_activation_set.h>
#include <dingo/registration/annotated.h>
#include <dingo/runtime/context.h>
#include <dingo/static/graph.h>
#include <dingo/static/registry.h>

#include <type_traits>
#include <utility>

namespace dingo {
namespace detail {

template <typename Host, typename StaticRegistry> class binding_resolution;

template <typename Host, typename... Registrations>
class binding_resolution<Host, static_registry<Registrations...>>
    : private binding_scope<Registrations...> {
    using static_registry_type = static_registry<Registrations...>;
    using self_type = binding_resolution<Host, static_registry_type>;
    using state_type = binding_scope<Registrations...>;

    template <typename T, typename Key>
    using binding_t = static_binding_t<
        typename static_registry_type::template bindings<T, Key>>;

    template <typename Request, typename Key>
    typename annotated_traits<Request>::type
    resolve_binding(runtime_context& context) {
        using binding = binding_t<Request, Key>;
        using interface_binding = typename binding::binding_type;
        auto route = this->template make_route<interface_binding>(*this);
        return route.template resolve<Request>(context);
    }

    template <typename T, typename Key, typename Fn>
    T construct_collection(runtime_context& context, Fn&& fn) {
        using collection_type = collection_traits<T>;
        using resolve_type = typename collection_type::resolve_type;

        constexpr std::size_t static_count =
            type_list_size_v<typename static_registry_type::template bindings<
                normalized_type_t<resolve_type>, Key>>;
        return detail::construct_binding_collection<T>(
            [&] { return host_->template count_collection<T, Key>(); },
            [&] { return static_count; },
            [&](auto& results, auto&& append) {
                host_->template append_collection<T, Key>(
                    results, context, std::forward<decltype(append)>(append));
            },
            [&](auto& results, auto&& append) {
                this->template append_static_collection<T, Key,
                                                        static_registry_type>(
                    results, *this, context,
                    std::forward<decltype(append)>(append));
            },
            std::forward<Fn>(fn));
    }

  public:
    using allocator_type = typename Host::allocator_type;

    static_assert(static_registry_type::valid,
                  "register_type bindings<...> requires a valid compile-time "
                  "bindings source");
    static_assert(detail::graph_analysis<static_registry_type, true>::acyclic,
                  "register_type bindings<...> requires an acyclic "
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
    binding_resolution(Host* host, Allocator&&) : host_(host) {}

    allocator_type& get_allocator() { return host_->get_allocator(); }

    template <typename T, bool RemoveRvalueReferences, typename Key = void,
              typename R = typename annotated_traits<std::conditional_t<
                  RemoveRvalueReferences,
                  std::conditional_t<std::is_rvalue_reference_v<T>,
                                     std::remove_reference_t<T>, T>,
                  T>>::type>
    R resolve(runtime_context& context) {
        if constexpr (collection_traits<R>::is_collection) {
            return construct_collection<R, Key>(
                context, [](auto& collection, auto&& value) {
                    collection_traits<std::decay_t<decltype(collection)>>::add(
                        collection, std::move(value));
                });
        } else {
            using request_type = R;
            using binding = binding_t<request_type, Key>;
            const auto host_status =
                host_->template binding_status<request_type, Key>();
            const auto merged = detail::resolve_binding(
                binding::status, host_status,
                detail::binding_resolution_policy::prefer_primary);
            if (merged == detail::binding_result::ambiguous) {
                throw detail::make_type_ambiguous_exception<T>(context);
            }

            if constexpr (binding::status ==
                          detail::binding_selection_status::found) {
                if (merged == detail::binding_result::primary) {
                    return resolve_binding<request_type, Key>(context);
                }

                if (merged == detail::binding_result::secondary) {
                    return host_->template resolve_request<
                        T, RemoveRvalueReferences, Key>(context);
                }
            }

            return host_
                ->template resolve_request<T, RemoveRvalueReferences, Key>(
                    context);
        }
    }

    template <typename T, bool RemoveRvalueReferences, typename Key,
              typename R = std::conditional_t<
                  RemoveRvalueReferences,
                  std::conditional_t<std::is_rvalue_reference_v<T>,
                                     std::remove_reference_t<T>, T>,
                  T>>
    R resolve(runtime_context& context, key<Key>) {
        return resolve<T, RemoveRvalueReferences, Key>(context);
    }

  private:
    Host* host_;
};

} // namespace detail
} // namespace dingo
