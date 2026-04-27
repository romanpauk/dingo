//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/core/exceptions.h>
#include <dingo/factory/callable.h>
#include <dingo/factory/constructor.h>
#include <dingo/registration/annotated.h>
#include <dingo/registration/type_registration.h>
#include <dingo/core/keyed.h>
#include <dingo/core/static_activation_set.h>
#include <dingo/static/registry.h>
#include <dingo/static/graph.h>
#include <dingo/static/context.h>
#include <type_traits>
#include <utility>

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4702)
#endif

namespace dingo {

template <typename StaticSource, typename = void> class static_injector;

namespace detail {

template <typename StaticRegistry, bool DependenciesResolved>
struct static_injector_graph_type;

template <typename StaticRegistry>
struct static_injector_graph_type<StaticRegistry, true> {
    using type = static_graph<StaticRegistry>;
};

template <typename StaticRegistry>
struct static_injector_graph_type<StaticRegistry, false> {
    struct type {
        static constexpr bool acyclic = true;
    };
};

struct direct_factory_context {
    template <typename T, typename Container>
    T resolve(Container&) = delete;
};

struct direct_factory_container {};

template <typename Factory>
inline constexpr bool direct_factory_without_dependencies_v =
    std::is_same_v<typename factory_traits<Factory>::dependencies, type_list<>>;

template <typename Factory, typename Type>
Type construct_direct_factory_value() {
    direct_factory_context context;
    direct_factory_container container;
    return Factory::template construct<Type>(context, container);
}

template <typename Selection, typename Request,
          bool Enabled = Selection::status ==
                         binding_selection_status::selected>
struct direct_wrapped_factory_selection {
    static constexpr bool enabled = false;
};

template <typename Selection, typename Request>
struct direct_wrapped_factory_selection<Selection, Request, true> {
    using binding_type = typename Selection::binding_type;
    using factory_type = typename binding_type::binding_model_type::factory_type;

    static constexpr bool enabled =
        direct_factory_without_dependencies_v<factory_type>;

    static normalized_type_t<Request> construct() {
        return construct_direct_factory_value<factory_type,
                                              normalized_type_t<Request>>();
    }
};

} // namespace detail

template <typename StaticSource>
class static_injector<
    StaticSource, std::void_t<static_bindings_source_t<StaticSource>>>
    : public static_injector<static_bindings_source_t<StaticSource>> {};

template <typename... Registrations>
class static_injector<static_registry<Registrations...>, void>
    : private detail::static_binding_scope<Registrations...>,
      private detail::static_registry_dependency_diagnostics<
          typename static_registry<Registrations...>::interface_bindings,
          detail::binding_model<Registrations>...> {
    using registry_type_ = static_registry<Registrations...>;
    using graph_type_ = typename detail::static_injector_graph_type<
        registry_type_, registry_type_::dependencies_are_resolved>::type;
    using self_type = static_injector<registry_type_>;
    using state_type = detail::static_binding_scope<Registrations...>;
    using context_type = static_context<registry_type_>;

  public:
    using rtti_type = typename state_type::rtti_type;
    using static_source_type = registry_type_;

    static_assert(static_source_type::valid,
                  "static_injector requires a valid compile-time bindings source");
    static_assert(graph_type_::acyclic,
                  "static_injector requires an acyclic compile-time binding graph");
    static_assert(
        (detail::binding_factory_is_default_constructible<
             detail::binding_model<Registrations>>::value &&
         ...),
        "static_injector requires default-constructible factories");
    static_assert(
        (detail::binding_storage_is_default_constructible<
             detail::binding_model<Registrations>>::value &&
         ...),
        "static_injector requires default-constructible storage objects");

    template <typename T, typename Key = void,
              typename R = typename annotated_traits<std::conditional_t<
                  std::is_rvalue_reference_v<T>, std::remove_reference_t<T>,
                  T>>::type>
    R resolve(key<Key> = {}) {
        if constexpr (!collection_traits<R>::is_collection) {
            using request_type = R;
            using selection =
                detail::static_binding_t<
                    typename static_source_type::template bindings<
                        request_type, Key>>;
            if constexpr (selection::status ==
                          detail::binding_selection_status::selected) {
                using interface_binding = typename selection::binding_type;
                using binding_model_type =
                    typename interface_binding::binding_model_type;
                if constexpr (
                    detail::publication_supports_request_v<
                        request_type, interface_binding> &&
                    (std::is_reference_v<request_type> ||
                     std::is_pointer_v<request_type>) &&
                    detail::direct_stored_request_v<
                        request_type, typename binding_model_type::storage_type> &&
                    detail::factory_without_dependencies_v<
                        typename binding_model_type::factory_type>) {
                    // Keep the no-context shortcut for direct identity requests
                    // only. Value requests still need the regular static
                    // materialization path so copies/moves are derived from the
                    // interface binding semantics.
                    detail::no_dependency_context context;
                    auto route =
                        this->template make_route<interface_binding>(*this);
                    return route.template resolve<request_type>(context);
                }
            }
        }
        context_type context;
        return resolve<T, false, Key>(context);
    }

    template <typename T,
              typename Factory = constructor<normalized_type_t<T>>,
              typename R = typename annotated_traits<std::conditional_t<
                  std::is_rvalue_reference_v<T>, std::remove_reference_t<T>,
                  T>>::type>
    R construct(Factory factory = Factory()) {
        using normalized_request_type = normalized_type_t<T>;
        using request_type = typename annotated_traits<T>::type;
        if constexpr (std::is_same_v<Factory,
                                     constructor<normalized_type_t<T>>>) {
            using selection =
                detail::static_binding_t<
                    typename static_source_type::template bindings<
                        detail::binding_request_interface_t<
                            request_type>,
                        detail::binding_request_key_t<request_type>>>;
            using normalized_selection =
                detail::static_binding_t<
                    typename static_source_type::template bindings<
                        normalized_request_type, void>>;
            if constexpr (selection::status !=
                          detail::binding_selection_status::not_found) {
                using interface_binding = typename selection::binding_type;
                if constexpr (detail::publication_supports_request_v<
                                  T, interface_binding>) {
                    return resolve<T>();
                } else if constexpr (
                    ::dingo::rvalue_request_requires_explicit_conversion_v<T>) {
                    context_type context;
                    ::dingo::throw_missing_rvalue_conversion<T>(
                        normalized_selection::status !=
                            detail::binding_selection_status::not_found,
                        [&]() {
                            return detail::make_type_not_convertible_exception(
                                describe_type<T>(),
                                describe_type<normalized_request_type>(),
                                context);
                        },
                        [&]() {
                            return detail::make_type_not_found_exception<T>(
                                context);
                        });
                } else if constexpr (construct_normalized_request_v<T>) {
                    if constexpr (detail::direct_wrapped_factory_selection<
                                      normalized_selection, T>::enabled) {
                        // Zero-dependency normalized bindings can build their
                        // value directly here, so wrapper construction does not
                        // need the shared/static resolution context machinery.
                        return type_traits<std::decay_t<T>>::make(
                            detail::direct_wrapped_factory_selection<
                                normalized_selection, T>::construct());
                    } else {
                        return type_traits<std::decay_t<T>>::make(
                            resolve<normalized_request_type>());
                    }
                } else {
                    return resolve<T>();
                }
            } else if constexpr (normalized_selection::status !=
                                 detail::binding_selection_status::not_found) {
                if constexpr (
                    ::dingo::rvalue_request_requires_explicit_conversion_v<T>) {
                    context_type context;
                    ::dingo::throw_missing_rvalue_conversion<T>(
                        true,
                        [&]() {
                            return detail::make_type_not_convertible_exception(
                                describe_type<T>(),
                                describe_type<normalized_request_type>(),
                                context);
                        },
                        [&]() {
                            return detail::make_type_not_found_exception<T>(
                                context);
                        });
                } else if constexpr (construct_normalized_request_v<T>) {
                    if constexpr (detail::direct_wrapped_factory_selection<
                                      normalized_selection, T>::enabled) {
                        return type_traits<std::decay_t<T>>::make(
                            detail::direct_wrapped_factory_selection<
                                normalized_selection, T>::construct());
                    } else {
                        return type_traits<std::decay_t<T>>::make(
                            resolve<normalized_request_type>());
                    }
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
        using callable_type = std::remove_cv_t<std::remove_reference_t<Callable>>;
        using dispatch_signature =
            detail::callable_dispatch_signature_t<Signature, callable_type>;

        context_type context;
        auto type_guard = context.template track_type<callable_type>();
        return detail::callable_invoke<dispatch_signature>::construct(
            std::forward<Callable>(callable), context, *this);
    }

    template <typename T> T construct_collection() {
        context_type context;
        return this->template construct_static_collection<T, void,
                                                          static_source_type>(
            *this, context);
    }

    template <typename T, typename Fn> T construct_collection(Fn&& fn) {
        context_type context;
        return this->template construct_static_collection<T, void,
                                                          static_source_type>(
            *this, context, std::forward<Fn>(fn));
    }

    template <typename T, typename Key> T construct_collection(key<Key>) {
        context_type context;
        return this->template construct_static_collection<T, Key,
                                                          static_source_type>(
            *this, context);
    }

    template <typename T, typename Fn, typename Key>
    T construct_collection(Fn&& fn, key<Key>) {
        context_type context;
        return this->template construct_static_collection<T, Key,
                                                          static_source_type>(
            *this, context, std::forward<Fn>(fn));
    }

    template <
        typename T, bool RemoveRvalueReferences, typename Key = void,
        typename R = typename annotated_traits<std::conditional_t<
            RemoveRvalueReferences,
            std::conditional_t<std::is_rvalue_reference_v<T>,
                               std::remove_reference_t<T>, T>,
            T>>::type>
    R resolve(context_type& context) {
        if constexpr (collection_traits<R>::is_collection) {
            return this->template construct_static_collection<
                R, Key, static_source_type>(*this, context);
        } else {
            using request_type = R;
            using selection =
                detail::static_binding_t<
                    typename static_source_type::template bindings<
                        request_type, Key>>;
            static_assert(
                selection::status != detail::binding_selection_status::not_found,
                "static_injector cannot resolve an unbound type");
            static_assert(
                selection::status != detail::binding_selection_status::ambiguous,
                "static_injector cannot resolve an ambiguously bound type");
            using interface_binding = typename selection::binding_type;

            auto route = this->template make_route<interface_binding>(*this);
            return route.template resolve<request_type>(context);
        }
    }

    template <typename T, bool RemoveRvalueReferences, typename Key,
              typename R = std::conditional_t<
                  RemoveRvalueReferences,
                  std::conditional_t<std::is_rvalue_reference_v<T>,
                                     std::remove_reference_t<T>, T>,
                  T>>
    R resolve(context_type& context, key<Key>) {
        return resolve<T, RemoveRvalueReferences, Key>(context);
    }

};

} // namespace dingo

#ifdef _MSC_VER
#pragma warning(pop)
#endif
