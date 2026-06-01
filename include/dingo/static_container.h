//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/container.h>
#include <dingo/core/binding_resolution_policy.h>
#include <dingo/static/activation_set.h>
#include <dingo/static/container_traits.h>
#include <dingo/factory/invoke.h>
#include <dingo/static/context.h>
#include <dingo/static/resolution.h>
#include <dingo/type/request_traits.h>

#include <type_traits>
#include <utility>

namespace dingo {

namespace detail {

template <typename StaticRegistry> class static_container_impl;

template <typename... Registrations>
class static_container_impl<static_registry<Registrations...>>
    : private static_registry_dependency_diagnostics<
          typename static_registry<Registrations...>::interface_bindings,
          binding_model<Registrations>...> {
    using registry_type_ = static_registry<Registrations...>;
    using graph_type_ = typename static_container_graph_type<
        registry_type_, registry_type_::dependencies_are_resolved>::type;
    using self_type = static_container_impl<registry_type_>;
    using state_type = static_storage_state<Registrations...>;
    using scope_ref = static_binding_scope_ref<state_type, Registrations...>;
    using context_type = static_context<registry_type_>;

    template <typename Request, typename LookupRequest, typename Key>
    struct diagnostic_static_binding_source {
        using selection = static_binding_t<
            typename registry_type_::template bindings<LookupRequest, Key>>;
        static constexpr bool can_resolve =
            selection::status == binding_selection_status::found;

        self_type& host;

        constexpr binding_selection_status status() const {
            static_assert(selection::status !=
                              binding_selection_status::not_found,
                          "static_container cannot resolve an unbound type");
            static_assert(selection::status != binding_selection_status::ambiguous,
                          "static_container cannot resolve an ambiguously "
                          "bound type");
            return selection::status;
        }

        template <typename ResolveRequest>
        decltype(auto) resolve(context_type& context) {
            using binding = typename selection::binding_type;
            auto state = host.state_ref();
            auto resolver =
                state.template make_binding_resolver<binding>(host);
            return resolver.template resolve<Request>(context);
        }
    };

    scope_ref state_ref() { return scope_ref(static_state_); }

  public:
    using source_type = registry_type_;
    using static_source_type = registry_type_;
    using rtti_type = typename scope_ref::rtti_type;

    static_assert(static_source_type::valid,
                  "static_container requires a valid compile-time bindings "
                  "source");
    static_assert(graph_type_::acyclic,
                  "static_container requires an acyclic compile-time binding "
                  "graph");
    static_assert((binding_factory_is_default_constructible<
                       binding_model<Registrations>>::value &&
                   ...),
                  "static_container requires default-constructible factories");
    static_assert((binding_storage_is_default_constructible<
                       binding_model<Registrations>>::value &&
                   ...),
                  "static_container requires default-constructible storage "
                  "objects");

    static_source_type& registry() { return static_registry_; }

    const static_source_type& registry() const { return static_registry_; }

    template <typename T, typename Key = void,
              typename R = request_result_t<T>>
    R resolve(key<Key> = {}) {
        if constexpr (!collection_traits<R>::is_collection) {
            using lookup_request_type = resolve_request_t<T, true>;
            using request_type = R;
            using selection = static_binding_t<
                typename static_source_type::template bindings<
                    lookup_request_type, Key>>;
            if constexpr (selection::status == binding_selection_status::found) {
                using binding = typename selection::binding_type;
                using binding_model_type =
                    typename binding::binding_model_type;
                if constexpr (binding_supports_request_v<request_type,
                                                          binding> &&
                              (std::is_reference_v<request_type> ||
                               std::is_pointer_v<request_type>) &&
                              stored_request_identity_v<
                                  request_type,
                                  typename binding_model_type::storage_type> &&
                              factory_without_dependencies_v<
                                  typename binding_model_type::factory_type>) {
                    no_dependency_context context;
                    auto state = state_ref();
                    auto resolver =
                        state.template make_binding_resolver<binding>(*this);
                    return resolver.template resolve<request_type>(context);
                }
            }
        }
        context_type context;
        return resolve<T, false, Key>(context);
    }

    template <typename T, typename Factory = constructor<normalized_type_t<T>>,
              typename R = request_result_t<T>>
    R construct(Factory factory = Factory()) {
        using normalized_request_type = request_value_t<T>;
        using request_type = request_interface_t<T>;
        if constexpr (std::is_same_v<Factory,
                                     constructor<normalized_type_t<T>>>) {
            using selection = static_binding_t<
                typename static_source_type::template bindings<
                    binding_request_interface_t<request_type>,
                    binding_request_key_t<request_type>>>;
            using normalized_selection = static_binding_t<
                typename static_source_type::template bindings<
                    normalized_request_type, void>>;
            constexpr bool has_exact_binding =
                selection::status != binding_selection_status::not_found;
            constexpr bool has_normalized_binding =
                normalized_selection::status !=
                binding_selection_status::not_found;
            if constexpr (has_exact_binding) {
                using binding = typename selection::binding_type;
                if constexpr (binding_supports_request_v<T, binding>) {
                    return resolve<T>();
                }
            }

            if constexpr (has_exact_binding || has_normalized_binding) {
                if constexpr (::dingo::
                                  rvalue_request_requires_explicit_conversion_v<
                                      T>) {
                    context_type context;
                    ::dingo::throw_missing_rvalue_conversion<T>(
                        has_normalized_binding, context);
                } else if constexpr (construct_normalized_request_v<T>) {
                    return construct_static_binding_value<
                        T, normalized_selection>(
                        [&]() { return resolve<normalized_request_type>(); });
                } else {
                    return resolve<T>();
                }
            } else {
                context_type context;
                auto type_guard =
                    context.template track_type<normalized_request_type>();
                return factory.template construct<R>(context, *this);
            }
        } else {
            context_type context;
            auto type_guard =
                context.template track_type<normalized_request_type>();
            return factory.template construct<R>(context, *this);
        }
    }

    template <typename Signature = void, typename Callable>
    auto invoke(Callable&& callable) {
        using callable_type =
            std::remove_cv_t<std::remove_reference_t<Callable>>;
        using dispatch_signature =
            callable_dispatch_signature_t<Signature, callable_type>;

        context_type context;
        auto type_guard = context.template track_type<callable_type>();
        return callable_invoke<dispatch_signature>::construct(
            std::forward<Callable>(callable), context, *this);
    }

    template <typename T> T construct_collection() {
        context_type context;
        auto state = state_ref();
        return state.template construct_static_collection<
            T, void, static_source_type>(*this, context);
    }

    template <typename T, typename Fn> T construct_collection(Fn&& fn) {
        context_type context;
        auto state = state_ref();
        return state.template construct_static_collection<
            T, void, static_source_type>(*this, context, std::forward<Fn>(fn));
    }

    template <typename T, typename Key> T construct_collection(key<Key>) {
        context_type context;
        auto state = state_ref();
        return state.template construct_static_collection<
            T, Key, static_source_type>(*this, context);
    }

    template <typename T, typename Fn, typename Key>
    T construct_collection(Fn&& fn, key<Key>) {
        context_type context;
        auto state = state_ref();
        return state.template construct_static_collection<
            T, Key, static_source_type>(*this, context, std::forward<Fn>(fn));
    }

    template <typename T, typename Key = void, typename Fn, typename Context>
    std::size_t append_collection(T& results, Context& context, Fn&& fn) {
        auto state = state_ref();
        return state.template append_static_collection<T, Key,
                                                       static_source_type>(
            results, *this, context, std::forward<Fn>(fn));
    }

    template <typename T, typename Key = void> std::size_t count_collection() {
        using collection_type = collection_traits<T>;
        return type_list_size_v<typename static_source_type::template bindings<
            normalized_type_t<typename collection_type::resolve_type>, Key>>;
    }

    template <typename T, bool RemoveRvalueReferences, typename Key = void,
              typename R = resolve_result_t<T, RemoveRvalueReferences>>
    R resolve(context_type& context) {
        if constexpr (collection_traits<R>::is_collection) {
            auto state = state_ref();
            return state.template construct_static_collection<
                R, Key, static_source_type>(*this, context);
        } else {
            using lookup_request_type =
                resolve_request_t<T, RemoveRvalueReferences>;
            using request_type = R;
            diagnostic_static_binding_source<request_type, lookup_request_type,
                                             Key>
                source{*this};
            missing_binding_source<lookup_request_type> missing;
            auto sources = make_one_binding_source(source, missing);
            return resolve_from_binding_sources<T, request_type>(context,
                                                                 sources);
        }
    }

    template <typename T, bool RemoveRvalueReferences, typename Key,
              typename R = resolve_request_t<T, RemoveRvalueReferences>>
    R resolve(context_type& context, key<Key>) {
        return resolve<T, RemoveRvalueReferences, Key>(context);
    }

    template <typename T, bool RemoveRvalueReferences, bool CheckCache,
              typename Key,
              typename R = resolve_request_t<T, RemoveRvalueReferences>>
    R resolve(context_type& context, key<Key>) {
        (void)CheckCache;
        return resolve<T, RemoveRvalueReferences, Key>(context);
    }

  private:
    static_source_type static_registry_;
    state_type static_state_;
};

} // namespace detail

template <typename StaticSource>
class static_container
    : public detail::static_container_impl<
          static_bindings_source_t<StaticSource>> {};

} // namespace dingo
