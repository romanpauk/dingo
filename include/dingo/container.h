//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/memory/allocator.h>
#include <dingo/core/container_common.h>
#include <dingo/registration/annotated.h>
#include <dingo/core/binding_model.h>
#include <dingo/core/binding_resolution_context.h>
#include <dingo/core/keyed.h>
#include <dingo/core/binding_resolution_status.h>
#include <dingo/core/binding_selection.h>
#include <dingo/registration/requirements.h>
#include <dingo/resolution/runtime_binding.h>
#include <dingo/registration/collection_traits.h>
#include <dingo/type/normalized_type.h>
#include <dingo/core/exceptions.h>
#include <dingo/factory/callable.h>
#include <dingo/factory/invoke.h>
#include <dingo/index/index.h>
#include <dingo/rtti/static_provider.h>
#include <dingo/rtti/typeid_provider.h>
#include <dingo/memory/static_allocator.h>
#include <dingo/resolution/type_cache.h>
#include <dingo/type/complete_type.h>
#include <dingo/type/type_map.h>
#include <dingo/registration/type_registration.h>
#include <dingo/runtime/context.h>
#include <dingo/runtime_container.h>
#include <dingo/storage/interface_storage_traits.h>
#include <dingo/static/injector.h>

#include <algorithm>
#include <functional>
#include <map>
#include <optional>
#include <typeindex>
#include <type_traits>
#include <variant>

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4702)
#endif

namespace dingo {
template <typename... Registrations>
class hybrid_container<static_registry<Registrations...>>
    : public runtime_registry<dynamic_container_traits,
                              typename dynamic_container_traits::allocator_type,
                              void,
                              hybrid_container<static_registry<Registrations...>>>,
      private detail::binding_scope<Registrations...> {
    friend class runtime_context;

    using static_registry_type_ = static_registry<Registrations...>;
    using self_type = hybrid_container<static_registry_type_>;
    using runtime_base = runtime_registry<dynamic_container_traits,
                                          typename dynamic_container_traits::allocator_type,
                                          void, self_type>;
    using static_state = detail::binding_scope<Registrations...>;
    using static_resolution_ref =
        detail::static_binding_scope_ref<static_state, Registrations...>;
    using static_context_type = static_context<static_registry_type_>;

    template <typename T, typename Key>
    using static_selection_t = detail::static_binding_t<
        typename static_registry_type_::template bindings<T, Key>>;

    template <typename T>
    static constexpr bool runtime_auto_constructible_v =
        std::is_same_v<normalized_type_t<T>, std::decay_t<T>> &&
        (!std::is_reference_v<T> ||
         (std::is_lvalue_reference_v<T> &&
          std::is_const_v<std::remove_reference_t<T>> &&
          is_auto_constructible<std::decay_t<T>>::value));

    template <typename Request, typename Key = void>
    bool has_runtime_binding() {
        if (!runtime_base::has_runtime_registrations()) {
            return false;
        }
        return runtime_base::template binding_status<Request, Key>() !=
               detail::binding_selection_status::not_found;
    }

    template <typename T, typename Key = void>
    bool has_runtime_collection() {
        if (!runtime_base::has_runtime_registrations()) {
            return false;
        }
        if constexpr (std::is_void_v<Key>) {
            return this->template count_runtime_collection<T>(none_t{}) != 0;
        } else {
            return this->template count_runtime_collection<T>(key<Key>{}) != 0;
        }
    }

    bool has_runtime_registrations() const {
        return runtime_base::has_runtime_registrations();
    }

    template <typename Request, typename Key = void>
    static constexpr bool has_static_binding_v =
        static_selection_t<Request, Key>::status ==
        detail::binding_selection_status::selected;

    template <typename Request, typename Key = void,
              bool Selected = has_static_binding_v<Request, Key>>
    struct static_binding_satisfies_request : std::false_type {};

    template <typename Request, typename Key>
    struct static_binding_satisfies_request<Request, Key, true>
        : std::bool_constant<
              detail::static_binding_resolvable_v<
                  typename static_selection_t<Request, Key>::binding_type,
                  static_registry_type_> &&
              detail::publication_supports_request_v<
                  Request,
                  typename static_selection_t<Request, Key>::binding_type>> {
    };

    template <typename Request, typename Key = void>
    static constexpr bool static_binding_satisfies_request_v =
        static_binding_satisfies_request<Request, Key>::value;

    template <typename Request, typename Key = void>
    bool select_static_resolve() {
        if constexpr (!static_binding_satisfies_request_v<Request, Key>) {
            return false;
        } else if (!has_runtime_registrations()) {
            return true;
        } else {
            return !has_runtime_binding<Request, Key>();
        }
    }

    template <typename Collection, typename Key = void>
    bool select_static_collection() {
        if constexpr (!has_static_collection_v<Collection, Key>) {
            return false;
        } else if (!has_runtime_registrations()) {
            return true;
        } else {
            return !has_runtime_collection<Collection, Key>();
        }
    }

    template <typename T>
    static constexpr bool has_static_construct_request_v = [] {
        using request_type = typename annotated_traits<T>::type;
        using normalized_request_type = normalized_type_t<T>;
        constexpr bool has_exact_static_binding =
            static_binding_satisfies_request_v<request_type>;
        constexpr bool has_normalized_static_binding =
            static_selection_t<normalized_request_type, void>::status ==
                detail::binding_selection_status::selected &&
            detail::static_binding_resolvable_v<
                typename static_selection_t<normalized_request_type,
                                            void>::binding_type,
                static_registry_type_>;

        return has_exact_static_binding ||
               (has_normalized_static_binding &&
                construct_normalized_request_v<T>);
    }();

    template <typename T>
    bool select_static_construct() {
        using request_type = typename annotated_traits<T>::type;
        using normalized_request_type = normalized_type_t<T>;

        if constexpr (!has_static_construct_request_v<T>) {
            return false;
        } else if (!has_runtime_registrations()) {
            return true;
        } else {
            return !has_runtime_binding<request_type>() &&
                   !has_runtime_binding<normalized_request_type>();
        }
    }

    template <typename T, typename Key = void>
    bool select_static_collection_construct() {
        constexpr bool has_static_collection_bindings =
            detail::static_collection_binding_count<static_registry_type_, T,
                                                   Key>() != 0;
        if constexpr (!has_static_collection_bindings) {
            return false;
        } else if (!has_runtime_registrations()) {
            return true;
        } else {
            return !has_runtime_collection<T, Key>();
        }
    }

    template <typename Collection, typename Key = void>
    static constexpr bool has_static_collection_v =
        detail::static_bindings_resolvable_v<
            typename static_registry_type_::template bindings<
                normalized_type_t<
                    typename collection_traits<Collection>::resolve_type>,
                Key>,
            static_registry_type_>;

    template <typename T, typename Key, typename Fn, typename Context>
    T construct_static_collection(Context& context, Fn&& fn, key<Key>) {
        static_resolution_ref static_state_ref(static_cast<static_state&>(*this));
        return static_state_ref.template construct_static_collection<
            T, Key, static_registry_type_>(*this, context, std::forward<Fn>(fn));
    }

    template <typename T, typename Fn, typename Context>
    T construct_static_collection(Context& context, Fn&& fn, none_t) {
        static_resolution_ref static_state_ref(static_cast<static_state&>(*this));
        return static_state_ref.template construct_static_collection<
            T, void, static_registry_type_>(*this, context,
                                            std::forward<Fn>(fn));
    }

    template <typename T, typename Key, typename Context>
    T construct_static_collection(Context& context, key<Key>) {
        static_resolution_ref static_state_ref(static_cast<static_state&>(*this));
        return static_state_ref.template construct_static_collection<
            T, Key, static_registry_type_>(*this, context);
    }

    template <typename T, typename Context>
    T construct_static_collection(Context& context, none_t) {
        static_resolution_ref static_state_ref(static_cast<static_state&>(*this));
        return static_state_ref.template construct_static_collection<
            T, void, static_registry_type_>(*this, context);
    }

    template <typename T, bool RemoveRvalueReferences, typename Key = void,
              typename R = typename annotated_traits<
                  std::conditional_t<std::is_rvalue_reference_v<T>,
                                     std::remove_reference_t<T>, T>>::type,
              std::enable_if_t<std::is_rvalue_reference_v<T>, int> = 0>
    R resolve_static() {
        static_context_type static_context;
        return resolve_static<T, RemoveRvalueReferences, Key>(
            static_context);
    }

    template <typename T, bool RemoveRvalueReferences, typename Key = void,
              std::enable_if_t<!std::is_rvalue_reference_v<T>, int> = 0>
    decltype(auto) resolve_static() {
        static_context_type static_context;
        return resolve_static<T, RemoveRvalueReferences, Key>(
            static_context);
    }

    template <typename T,
              typename R = typename annotated_traits<std::conditional_t<
                  std::is_rvalue_reference_v<T>, std::remove_reference_t<T>,
                  T>>::type>
    R construct_static() {
        using request_type = typename annotated_traits<T>::type;
        using normalized_request_type = normalized_type_t<T>;
        constexpr bool has_exact_static_binding =
            static_binding_satisfies_request_v<request_type>;
        constexpr bool has_normalized_static_binding =
            static_binding_satisfies_request_v<normalized_request_type>;
        if constexpr (has_exact_static_binding) {
            using interface_binding =
                typename static_selection_t<request_type, void>::binding_type;
            if constexpr (detail::publication_supports_request_v<
                              T, interface_binding>) {
                static_context_type static_context;
                return resolve_static<T, false>(static_context);
            } else if constexpr (has_normalized_static_binding &&
                                 construct_normalized_request_v<T>) {
                using normalized_selection =
                    static_selection_t<normalized_request_type, void>;
                if constexpr (detail::direct_wrapped_factory_selection<
                                  normalized_selection, T>::enabled) {
                    return type_traits<std::decay_t<T>>::make(
                        detail::direct_wrapped_factory_selection<
                            normalized_selection, T>::construct());
                } else {
                    return type_traits<std::decay_t<T>>::make(
                        resolve_static<normalized_type_t<T>, false>());
                }
            } else {
                static_context_type static_context;
                return resolve_static<T, false>(static_context);
            }
        } else {
            if constexpr (has_normalized_static_binding &&
                          construct_normalized_request_v<T>) {
                using normalized_selection =
                    static_selection_t<normalized_request_type, void>;
                if constexpr (detail::direct_wrapped_factory_selection<
                                  normalized_selection, T>::enabled) {
                    return type_traits<std::decay_t<T>>::make(
                        detail::direct_wrapped_factory_selection<
                            normalized_selection, T>::construct());
                } else {
                    return type_traits<std::decay_t<T>>::make(
                        resolve_static<normalized_type_t<T>, false>());
                }
            } else {
                static_context_type static_context;
                return resolve_static<T, false>(static_context);
            }
        }
    }

    template <typename T, typename Fn>
    T construct_static_collection(Fn&& fn, none_t) {
        static_context_type static_context;
        return construct_static_collection<T>(static_context,
                                              std::forward<Fn>(fn), none_t{});
    }

    template <typename T> T construct_static_collection(none_t) {
        static_context_type static_context;
        return construct_static_collection<T>(static_context, none_t{});
    }

    template <typename T, typename Fn, typename Key>
    T construct_static_collection(Fn&& fn, key<Key>) {
        static_context_type static_context;
        return construct_static_collection<T>(static_context,
                                              std::forward<Fn>(fn), key<Key>{});
    }

    template <typename T, typename Key>
    T construct_static_collection(key<Key>) {
        static_context_type static_context;
        return construct_static_collection<T>(static_context, key<Key>{});
    }

    template <typename T, typename Key, typename Fn>
    T construct_collection(runtime_context& context, Fn&& fn, key<Key>) {
        using collection_type = collection_traits<T>;
        using resolve_type = typename collection_type::resolve_type;
        auto& static_state_ref = static_cast<static_state&>(*this);

        constexpr std::size_t static_count =
            type_list_size_v<typename static_registry_type_::template bindings<
                normalized_type_t<resolve_type>, Key>>;
        return detail::construct_binding_collection<T>(
            [&] { return this->template count_runtime_collection<T>(key<Key>{}); },
            [&] { return static_count; },
            [&](auto& results, auto&& append) {
                this->append_runtime_collection(
                    results, context, std::forward<decltype(append)>(append),
                    key<Key>{});
            },
            [&](auto& results, auto&& append) {
                static_state_ref.template append_static_collection<
                    T, Key, static_registry_type_>(
                    results, *this, context,
                    std::forward<decltype(append)>(append));
            },
            std::forward<Fn>(fn));
    }

    template <typename T, typename Fn>
    T construct_collection(runtime_context& context, Fn&& fn, none_t) {
        using collection_type = collection_traits<T>;
        using resolve_type = typename collection_type::resolve_type;
        auto& static_state_ref = static_cast<static_state&>(*this);

        constexpr std::size_t static_count =
            type_list_size_v<typename static_registry_type_::template bindings<
                normalized_type_t<resolve_type>, void>>;
        return detail::construct_binding_collection<T>(
            [&] { return this->template count_runtime_collection<T>(none_t{}); },
            [&] { return static_count; },
            [&](auto& results, auto&& append) {
                this->append_runtime_collection(
                    results, context, std::forward<decltype(append)>(append),
                    none_t{});
            },
            [&](auto& results, auto&& append) {
                static_state_ref.template append_static_collection<
                    T, void, static_registry_type_>(
                    results, *this, context,
                    std::forward<decltype(append)>(append));
            },
            std::forward<Fn>(fn));
    }

    template <typename Request, typename Key, typename Context>
    typename annotated_traits<Request>::type resolve_static_selection(
        Context& context) {
        using selection = static_selection_t<Request, Key>;
        using interface_binding = typename selection::binding_type;
        if constexpr (selection::status !=
                      detail::binding_selection_status::selected) {
            throw detail::make_type_not_found_exception<Request>(context);
        } else {
            static_resolution_ref static_state_ref(
                static_cast<static_state&>(*this));
            auto route =
                static_state_ref.template make_route<interface_binding>(*this);
            return route.template resolve<Request>(context);
        }
    }

    template <typename Request, typename Key, typename Context>
    typename annotated_traits<Request>::type resolve_binding_selection(
        Context& context) {
        using selection = static_selection_t<Request, Key>;
        using interface_binding = typename selection::binding_type;
        if constexpr (selection::status !=
                      detail::binding_selection_status::selected) {
            throw detail::make_type_not_found_exception<Request>(context);
        } else {
            auto& static_state_ref = static_cast<static_state&>(*this);
            auto route =
                static_state_ref.template make_route<interface_binding>(*this);
            return route.template resolve<Request>(context);
        }
    }

    template <typename T, bool RemoveRvalueReferences, typename Key = void,
              typename Context,
              typename R = typename annotated_traits<std::conditional_t<
                  RemoveRvalueReferences,
                  std::conditional_t<std::is_rvalue_reference_v<T>,
                                     std::remove_reference_t<T>, T>,
                  T>>::type>
    R resolve_static(Context& context) {
        if constexpr (collection_traits<R>::is_collection) {
            return construct_static_collection<R>(
                context, std::conditional_t<std::is_void_v<Key>, none_t,
                                            key<Key>>{});
        } else {
            using request_type = R;
            return resolve_static_selection<request_type, Key>(context);
        }
    }

    template <typename T, typename Fn>
#if defined(__GNUC__) && !defined(__clang__)
    __attribute__((noinline))
#endif
    T construct_collection_with_runtime_context(Fn&& fn, none_t) {
        runtime_context context;
        return construct_collection<T>(context, std::forward<Fn>(fn),
                                       none_t{});
    }

    template <typename T, typename Fn, typename Key>
#if defined(__GNUC__) && !defined(__clang__)
    __attribute__((noinline))
#endif
    T construct_collection_with_runtime_context(Fn&& fn, key<Key>) {
        runtime_context context;
        return construct_collection<T>(context, std::forward<Fn>(fn),
                                       key<Key>{});
    }

  public:
    using container_traits_type = typename runtime_base::container_traits_type;
    using allocator_type = typename runtime_base::allocator_type;
    using rtti_type = typename runtime_base::rtti_type;
    using runtime_injector_type = runtime_injector<self_type>;
    using static_source_type = static_registry_type_;

    using runtime_base::get_allocator;
    using runtime_base::register_indexed_type;
    using runtime_base::register_type;
    using runtime_base::register_type_collection;

    static_assert(static_source_type::valid,
                  "container requires a valid compile-time bindings source");
    static_assert(
        detail::graph_analysis<static_source_type, true>::acyclic,
        "container requires an acyclic compile-time binding graph");
    static_assert(
        (detail::binding_factory_is_default_constructible<
             detail::binding_model<Registrations>>::value &&
         ...),
        "container requires default-constructible compile-time factories");
    static_assert(
        (detail::binding_storage_is_default_constructible<
             detail::binding_model<Registrations>>::value &&
         ...),
        "container requires default-constructible compile-time storage objects");

    hybrid_container() = default;

    explicit hybrid_container(allocator_type alloc) : runtime_base(alloc) {}

    runtime_injector_type injector() { return runtime_injector_type(*this); }

    self_type& registry() { return *this; }

    const self_type& registry() const { return *this; }

    self_type& container() { return *this; }

    const self_type& container() const { return *this; }

    template <typename T, typename IdType = none_t,
              typename R = typename annotated_traits<
                  std::conditional_t<std::is_rvalue_reference_v<T>,
                                     std::remove_reference_t<T>, T>>::type>
    R resolve(IdType&& id = IdType()) {
        if constexpr (detail::is_typed_key_v<IdType>) {
            using key_type = typename std::decay_t<IdType>::type;
            if constexpr (collection_traits<R>::is_collection) {
                if (select_static_collection<R, key_type>()) {
                    return resolve_static<T, false, key_type>();
                }
            } else {
                if (select_static_resolve<R, key_type>()) {
                    return resolve_static<T, false, key_type>();
                }
            }
            runtime_context context;
            return resolve<T, false, true, key_type>(context);
        } else if constexpr (!is_none_v<std::decay_t<IdType>>) {
            runtime_context context;
            return this->template resolve_impl<T, false,
                                               runtime_auto_constructible_v<T>,
                                               true>(context,
                                                     std::forward<IdType>(id));
        } else {
            if constexpr (collection_traits<R>::is_collection) {
                if (select_static_collection<R>()) {
                    return resolve_static<T, false>();
                }
            } else {
                if (select_static_resolve<R>()) {
                    return resolve_static<T, false>();
                }
            }
            runtime_context context;
            return resolve<T, false>(context);
        }
    }

    template <typename T,
              typename Factory = constructor<normalized_type_t<T>>,
              typename R = typename annotated_traits<std::conditional_t<
                  std::is_rvalue_reference_v<T>, std::remove_reference_t<T>,
                  T>>::type>
    R construct(Factory factory = Factory()) {
        using request_type = typename annotated_traits<T>::type;
        using normalized_request_type = normalized_type_t<T>;
        if constexpr (std::is_same_v<Factory,
                                     constructor<normalized_type_t<T>>>) {
            if constexpr (has_static_construct_request_v<T>) {
                if (!has_runtime_registrations()) {
                    return construct_static<T>();
                }
            }
            if constexpr (
                ::dingo::rvalue_request_requires_explicit_conversion_v<T>) {
                constexpr bool has_static_normalized_binding =
                    static_selection_t<normalized_request_type, void>::status !=
                        detail::binding_selection_status::not_found &&
                    detail::static_binding_resolvable_v<
                        typename static_selection_t<normalized_request_type,
                                                    void>::binding_type,
                        static_registry_type_>;

                if constexpr (has_static_construct_request_v<T>) {
                    if (binding_status<request_type>() !=
                        detail::binding_selection_status::not_found &&
                        select_static_construct<T>()) {
                        return construct_static<T>();
                    }
                }

                if (has_runtime_binding<request_type>() ||
                    has_runtime_binding<normalized_request_type>()) {
                    return runtime_base::template construct_runtime_request<T>(
                        std::move(factory));
                }

                if constexpr (has_static_normalized_binding) {
                    ::dingo::throw_missing_rvalue_conversion<T>(
                        true,
                        [&]() {
                            return detail::make_type_not_convertible_exception(
                                describe_type<T>(),
                                describe_type<normalized_request_type>());
                        },
                        [&]() {
                            return detail::make_type_not_found_exception<T>();
                        });
                }

                return runtime_base::template construct_runtime_request<T>(
                    std::move(factory));
            } else if constexpr (has_static_construct_request_v<T>) {
                if (binding_status<request_type>() !=
                    detail::binding_selection_status::not_found &&
                    select_static_construct<T>()) {
                    return construct_static<T>();
                }
            } else {
                return runtime_base::template construct_runtime_request<T>(
                    std::move(factory));
            }
        }
        runtime_context context;
        if constexpr (std::is_same_v<Factory,
                                     constructor<normalized_type_t<T>>>) {
            if (binding_status<T>() !=
                detail::binding_selection_status::not_found) {
                if constexpr (construct_normalized_request_v<T>) {
                    return ::dingo::construct_request_or_wrap_normalized<T>(
                        [&]() { return resolve<T, false>(context); },
                        [&]() {
                            return resolve<normalized_type_t<T>, false>(
                                context);
                        });
                } else {
                    return resolve<T, false>(context);
                }
            } else if (binding_status<normalized_type_t<T>>() !=
                       detail::binding_selection_status::not_found) {
                if constexpr (construct_normalized_request_v<T>) {
                    return type_traits<std::decay_t<T>>::make(
                        resolve<normalized_type_t<T>, false>(context));
                } else {
                    return resolve<T, false>(context);
                }
            }
        }

        if constexpr (construct_factory_request_v<T>) {
            auto type_guard =
                context.template track_type<normalized_type_t<T>>();
            return factory.template construct<R>(context, *this);
        } else {
            return resolve<T, false>(context);
        }
    }

    template <typename T> T construct_collection() {
        if (select_static_collection_construct<T>()) {
            return construct_static_collection<T>(none_t{});
        }

        return construct_collection_with_runtime_context<T>(
            [](auto& collection, auto&& value) {
                collection_traits<std::decay_t<decltype(collection)>>::add(
                    collection, std::move(value));
            },
            none_t{});
    }

    template <typename T, typename Fn> T construct_collection(Fn&& fn) {
        if (select_static_collection_construct<T>()) {
            return construct_static_collection<T>(
                std::forward<Fn>(fn), none_t{});
        }

        return construct_collection_with_runtime_context<T>(
            std::forward<Fn>(fn), none_t{});
    }

    template <typename T, typename Key> T construct_collection(key<Key>) {
        if (select_static_collection_construct<T, Key>()) {
            return construct_static_collection<T>(key<Key>{});
        }

        return construct_collection_with_runtime_context<T>(
            [](auto& collection, auto&& value) {
                collection_traits<std::decay_t<decltype(collection)>>::add(
                    collection, std::move(value));
            },
            key<Key>{});
    }

    template <typename T, typename Fn, typename Key>
    T construct_collection(Fn&& fn, key<Key>) {
        if (select_static_collection_construct<T, Key>()) {
            return construct_static_collection<T>(
                std::forward<Fn>(fn), key<Key>{});
        }

        return construct_collection_with_runtime_context<T>(
            std::forward<Fn>(fn), key<Key>{});
    }

    template <typename Signature = void, typename Callable>
    auto invoke(Callable&& callable) {
        using callable_type = std::remove_cv_t<std::remove_reference_t<Callable>>;
        using dispatch_signature =
            detail::callable_dispatch_signature_t<Signature, callable_type>;

        runtime_context context;
        auto type_guard = context.template track_type<callable_type>();
        return detail::callable_invoke<dispatch_signature>::construct(
            std::forward<Callable>(callable), context, *this);
    }

    template <typename T, typename IdType = none_t,
              typename R = typename annotated_traits<
                  std::conditional_t<std::is_rvalue_reference_v<T>,
                                     std::remove_reference_t<T>, T>>::type>
    R resolve_runtime_request(IdType&& id = IdType()) {
        return resolve<T>(std::forward<IdType>(id));
    }

    template <typename T,
              typename Factory = constructor<normalized_type_t<T>>,
              typename R = typename annotated_traits<std::conditional_t<
                  std::is_rvalue_reference_v<T>, std::remove_reference_t<T>,
                  T>>::type>
    R construct_runtime_request(Factory factory = Factory()) {
        return construct<T>(std::move(factory));
    }

    template <typename T> T construct_collection_runtime_request() {
        return construct_collection<T>();
    }

    template <typename T, typename Fn>
    T construct_collection_runtime_request(Fn&& fn) {
        return construct_collection<T>(std::forward<Fn>(fn));
    }

    template <typename T, typename Key>
    T construct_collection_runtime_request(key<Key>) {
        return construct_collection<T>(key<Key>{});
    }

    template <typename T, typename Fn, typename Key>
    T construct_collection_runtime_request(Fn&& fn, key<Key>) {
        return construct_collection<T>(std::forward<Fn>(fn), key<Key>{});
    }

    template <typename Signature = void, typename Callable>
    auto invoke_runtime_request(Callable&& callable) {
        return invoke<Signature>(std::forward<Callable>(callable));
    }

    template <typename Request, typename Key = void>
    detail::binding_selection_status binding_status() {
        using request_type = typename annotated_traits<Request>::type;
        using static_selection = static_selection_t<request_type, Key>;
        const auto runtime_status =
            runtime_base::template binding_status<request_type, Key>();
        return detail::resolve_binding_status<static_selection::status>(
            runtime_status,
            detail::binding_resolution_policy::
                ambiguous_on_conflict);
    }

    template <typename T, typename Key = void, typename Fn>
    std::size_t append_collection(T& results, runtime_context& context,
                                  Fn&& fn) {
        auto& static_state_ref = static_cast<static_state&>(*this);
        if constexpr (std::is_void_v<Key>) {
            return detail::append_binding_collection(
                results,
                [&](auto& collection, auto&& append) {
                    return this->append_runtime_collection(
                        collection, context,
                        std::forward<decltype(append)>(append), none_t{});
                },
                [&](auto& collection, auto&& append) {
                    return static_state_ref.template append_static_collection<
                        T, void, static_registry_type_>(
                        collection, *this, context,
                        std::forward<decltype(append)>(append));
                },
                std::forward<Fn>(fn));
        } else {
            return detail::append_binding_collection(
                results,
                [&](auto& collection, auto&& append) {
                    return this->append_runtime_collection(
                        collection, context,
                        std::forward<decltype(append)>(append), key<Key>{});
                },
                [&](auto& collection, auto&& append) {
                    return static_state_ref.template append_static_collection<
                        T, Key, static_registry_type_>(
                        collection, *this, context,
                        std::forward<decltype(append)>(append));
                },
                std::forward<Fn>(fn));
        }
    }

    template <typename T, typename Key = void>
    std::size_t count_collection() {
        if constexpr (std::is_void_v<Key>) {
            return detail::count_binding_collection<T>(
                [&] { return this->template count_runtime_collection<T>(none_t{}); },
                detail::static_collection_binding_count<static_registry_type_, T,
                                                     void>());
        } else {
            return detail::count_binding_collection<T>(
                [&] { return this->template count_runtime_collection<T>(key<Key>{}); },
                detail::static_collection_binding_count<static_registry_type_, T,
                                                     Key>());
        }
    }

    template <typename T, bool RemoveRvalueReferences, typename Key = void,
              typename R = std::conditional_t<
                  RemoveRvalueReferences,
                  std::conditional_t<std::is_rvalue_reference_v<T>,
                                     std::remove_reference_t<T>, T>,
                  T>>
    R resolve_request(runtime_context& context) {
        if constexpr (std::is_void_v<Key>) {
            return resolve<T, RemoveRvalueReferences>(context);
        } else {
            return resolve<T, RemoveRvalueReferences, true, Key>(context);
        }
    }

    template <typename T, bool RemoveRvalueReferences, bool CheckCache = true,
              typename R = typename annotated_traits<std::conditional_t<
                  RemoveRvalueReferences,
                  std::conditional_t<std::is_rvalue_reference_v<T>,
                                     std::remove_reference_t<T>, T>,
                  T>>::type>
    R resolve(static_context_type& context, none_t) {
        return resolve_static<T, RemoveRvalueReferences>(context);
    }

    template <typename T, bool RemoveRvalueReferences, bool CheckCache = true,
              typename Key = void,
              typename R = typename annotated_traits<std::conditional_t<
                  RemoveRvalueReferences,
                  std::conditional_t<std::is_rvalue_reference_v<T>,
                                     std::remove_reference_t<T>, T>,
                  T>>::type>
    R resolve(static_context_type& context) {
        return resolve_static<T, RemoveRvalueReferences, Key>(context);
    }

    template <typename T, bool RemoveRvalueReferences, bool CheckCache,
              typename Key,
              typename R = std::conditional_t<
                  RemoveRvalueReferences,
                  std::conditional_t<std::is_rvalue_reference_v<T>,
                                     std::remove_reference_t<T>, T>,
                  T>>
    R resolve(static_context_type& context, key<Key>) {
        return resolve_static<T, RemoveRvalueReferences, Key>(context);
    }

    template <typename T, bool RemoveRvalueReferences, bool CheckCache = true,
              typename R = typename annotated_traits<std::conditional_t<
                  RemoveRvalueReferences,
                  std::conditional_t<std::is_rvalue_reference_v<T>,
                                     std::remove_reference_t<T>, T>,
                  T>>::type>
    R resolve(runtime_context& context, none_t) {
        return resolve<T, RemoveRvalueReferences, CheckCache>(context);
    }

    template <typename T, bool RemoveRvalueReferences, bool CheckCache = true,
              typename Key = void,
              typename R = typename annotated_traits<std::conditional_t<
                  RemoveRvalueReferences,
                  std::conditional_t<std::is_rvalue_reference_v<T>,
                                     std::remove_reference_t<T>, T>,
                  T>>::type>
    R resolve(runtime_context& context) {
        if constexpr (collection_traits<R>::is_collection) {
            if constexpr (std::is_void_v<Key>) {
                return construct_collection<R>(
                    context, [](auto& collection, auto&& value) {
                        collection_traits<std::decay_t<decltype(collection)>>::add(
                            collection, std::move(value));
                    },
                    none_t{});
            } else {
                return construct_collection<R>(
                    context, [](auto& collection, auto&& value) {
                        collection_traits<std::decay_t<decltype(collection)>>::add(
                            collection, std::move(value));
                    },
                    key<Key>{});
            }
        } else {
            using request_type = R;
            using static_selection = static_selection_t<request_type, Key>;
            const auto runtime_status =
                runtime_base::template binding_status<request_type, Key>();
            const auto resolution = detail::resolve_binding(
                runtime_status, static_selection::status,
                detail::binding_resolution_policy::
                    ambiguous_on_conflict);

            if (resolution == detail::binding_result::ambiguous) {
                throw detail::make_type_ambiguous_exception<T>(context);
            }

            if (resolution == detail::binding_result::primary) {
                return runtime_base::template resolve_request<
                    T, RemoveRvalueReferences, Key>(context);
            }

            if constexpr (static_selection::status ==
                          detail::binding_selection_status::selected) {
                if (resolution == detail::binding_result::secondary) {
                    return resolve_binding_selection<request_type, Key>(
                        context);
                }
            }

            return runtime_base::template resolve_request<
                T, RemoveRvalueReferences, Key>(context);
        }
    }

    template <typename T, bool RemoveRvalueReferences, bool CheckCache,
              typename Key,
              typename R = std::conditional_t<
                  RemoveRvalueReferences,
                  std::conditional_t<std::is_rvalue_reference_v<T>,
                                     std::remove_reference_t<T>, T>,
                  T>>
    R resolve(runtime_context& context, key<Key>) {
        return resolve<T, RemoveRvalueReferences, CheckCache, Key>(context);
    }
};

namespace detail {

template <typename... Params> struct container_base;
template <typename Param, typename Enable = void> struct container_base_one;
template <typename First, typename Second, typename Enable = void>
struct container_base_two;
template <typename First, typename Second, typename Third, typename Enable = void>
struct container_base_three;

template <>
struct container_base<> {
    using type = runtime_container<dynamic_container_traits>;
};

template <typename Param>
struct container_base<Param> {
    using type = typename container_base_one<Param>::type;
};

template <typename First, typename Second>
struct container_base<First, Second> {
    using type = typename container_base_two<First, Second>::type;
};

template <typename First, typename Second, typename Third>
struct container_base<First, Second, Third> {
    using type = typename container_base_three<First, Second, Third>::type;
};

template <typename First, typename Second, typename Third, typename... Rest>
struct container_base<First, Second, Third, Rest...> {
    static_assert(container_dependent_false_v<First, Second, Third, Rest...>,
                  "container<...> expects runtime traits or a single static bindings source");
};

template <typename... Params>
using container_base_t = typename container_base<Params...>::type;

template <typename Param, typename Enable>
struct container_base_one {
    static_assert(container_dependent_false_v<Param>,
                  "container<T> requires runtime container traits or compile-time bindings<...>");
};

template <typename Param>
struct container_base_one<
    Param, std::enable_if_t<is_runtime_container_traits_v<Param>>> {
    using type = runtime_container<Param, typename Param::allocator_type, void>;
};

template <typename Param>
struct container_base_one<Param, std::enable_if_t<is_static_registry_v<Param>>> {
    using type = hybrid_container<Param>;
};

template <typename Param>
struct container_base_one<
    Param, std::enable_if_t<is_bindings_wrapper_v<Param>>> {
    using static_registry_type = bindings_wrapper_registry_t<Param>;

    static_assert(is_static_registry_v<static_registry_type>,
                  "container<bindings<...>> requires a valid compile-time bindings source");

    using type = hybrid_container<static_registry_type>;
};
template <typename First, typename Second, typename Enable>
struct container_base_two {
    static_assert(container_dependent_false_v<First, Second>,
                  "container<T, U> requires runtime traits + allocator");
};

template <typename First, typename Second>
struct container_base_two<
    First, Second, std::enable_if_t<is_runtime_container_traits_v<First>>> {
    using type = runtime_container<First, Second, void>;
};

template <typename First, typename Second, typename Third, typename Enable>
struct container_base_three {
    static_assert(container_dependent_false_v<First, Second, Third>,
                  "container<T, U, V> requires runtime traits + allocator + parent");
};

template <typename First, typename Second, typename Third>
struct container_base_three<
    First, Second, Third, std::enable_if_t<is_runtime_container_traits_v<First>>> {
    using type = runtime_container<First, Second, Third>;
};

} // namespace detail

template <typename... Params>
class container : public detail::container_base_t<Params...> {
    using base_type = detail::container_base_t<Params...>;

  public:
    using base_type::base_type;
};
} // namespace dingo

#ifdef _MSC_VER
#pragma warning(pop)
#endif

#include <dingo/static_container.h>
