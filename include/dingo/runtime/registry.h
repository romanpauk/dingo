//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/core/binding_model.h>
#include <dingo/core/binding_resolution_context.h>
#include <dingo/core/binding_selection.h>
#include <dingo/core/container_common.h>
#include <dingo/core/exceptions.h>
#include <dingo/core/keyed.h>
#include <dingo/factory/callable.h>
#include <dingo/factory/invoke.h>
#include <dingo/index/index.h>
#include <dingo/memory/allocator.h>
#include <dingo/registration/annotated.h>
#include <dingo/registration/collection_traits.h>
#include <dingo/registration/requirements.h>
#include <dingo/resolution/runtime_binding.h>
#include <dingo/runtime/context.h>
#include <dingo/runtime/injector.h>
#include <dingo/storage/interface_storage_traits.h>
#include <dingo/type/normalized_type.h>

#include <algorithm>
#include <functional>
#include <map>
#include <memory>
#include <type_traits>
#include <typeindex>
#include <variant>

namespace dingo {

template <typename ContainerTraits, typename Allocator, typename ParentRegistry,
          typename ResolveRoot>
class runtime_registry : public allocator_base<Allocator> {
    friend class runtime_context;
    template <typename, typename> friend class detail::binding_resolution;
    template <typename ContainerTraitsT, typename AllocatorT,
              typename ParentRegistryT, typename ResolveRootT>
    friend class runtime_registry;
    template <typename> friend class runtime_injector;

    template <typename ContainerTraitsT, typename AllocatorT,
              typename ParentRegistryT, typename ResolveRootT>
    using rebind_t = runtime_registry<ContainerTraitsT, AllocatorT,
                                      ParentRegistryT, ResolveRootT>;
    using registry_type = runtime_registry<ContainerTraits, Allocator,
                                           ParentRegistry, ResolveRoot>;
    using resolve_root_type =
        std::conditional_t<std::is_same_v<void, ResolveRoot>, registry_type,
                           ResolveRoot>;
    using container_type = resolve_root_type;
    template <typename Registration>
    using registration_container_type =
        runtime_registry<typename ContainerTraits::template rebind_t<
                             type_list<typename ContainerTraits::tag_type,
                                       typename Registration::interface_type>>,
                         Allocator, resolve_root_type, resolve_root_type>;
    using parent_registry_type =
        std::conditional_t<std::is_same_v<void, ParentRegistry>, registry_type,
                           ParentRegistry>;

    static constexpr bool cache_enabled = ContainerTraits::cache_enabled;

  public:
    using container_traits_type = ContainerTraits;
    using allocator_type = Allocator;
    using rtti_type = typename ContainerTraits::rtti_type;
    using index_definition_type =
        typename ContainerTraits::index_definition_type;
    using runtime_injector_type = runtime_injector<registry_type>;

    runtime_injector_type injector();

    runtime_registry() : allocator_base<allocator_type>(allocator_type()) {}

    runtime_registry(allocator_type alloc)
        : allocator_base<allocator_type>(alloc) {}

    runtime_registry(parent_registry_type* parent,
                     allocator_type alloc = allocator_type())
        : allocator_base<allocator_type>(alloc), parent_(parent) {

        static_assert(
            !is_tagged_container_v<container_traits_type> ||
                !std::is_same_v<typename container_traits_type::tag_type,
                                typename parent_registry_type::
                                    container_traits_type::tag_type>,
            "static typemap based containers require parent and child "
            "container tags to be different");
    }

    allocator_type& get_allocator() {
        return allocator_base<allocator_type>::get_allocator();
    }

  protected:
    bool has_runtime_registrations() const { return runtime_bindings_present_; }
    struct runtime_state;

    runtime_state* runtime_state_if_present() { return runtime_state_.get(); }

    const runtime_state* runtime_state_if_present() const {
        return runtime_state_.get();
    }

    runtime_state& ensure_runtime_state() {
        if (!runtime_state_) {
            runtime_state_ = std::make_unique<runtime_state>(get_allocator());
        }
        return *runtime_state_;
    }

    resolve_root_type* resolve_root() {
        if constexpr (std::is_same_v<void, ResolveRoot> ||
                      std::is_same_v<resolve_root_type, registry_type>) {
            return this;
        } else if constexpr (std::is_base_of_v<registry_type,
                                               resolve_root_type>) {
            return static_cast<resolve_root_type*>(this);
        } else {
            return parent_;
        }
    }

    const resolve_root_type* resolve_root() const {
        if constexpr (std::is_same_v<void, ResolveRoot> ||
                      std::is_same_v<resolve_root_type, registry_type>) {
            return this;
        } else if constexpr (std::is_base_of_v<registry_type,
                                               resolve_root_type>) {
            return static_cast<const resolve_root_type*>(this);
        } else {
            return parent_;
        }
    }

  public:
    template <typename... TypeArgs> auto& register_type() {
        return register_type_impl<TypeArgs...>(none_t(), none_t());
    }

    template <typename... TypeArgs, typename Arg>
    auto& register_type(Arg&& arg) {
        return register_type_impl<TypeArgs...>(std::forward<Arg>(arg),
                                               none_t());
    }

    template <typename... TypeArgs, typename IdType>
    auto& register_indexed_type(IdType&& id) {
        return register_type_impl<TypeArgs...>(none_t(),
                                               std::forward<IdType>(id));
    }

    template <typename... TypeArgs, typename Arg, typename IdType>
    auto& register_indexed_type(Arg&& arg, IdType&& id) {
        return register_type_impl<TypeArgs...>(std::forward<Arg>(arg),
                                               std::forward<IdType>(id));
    }

    template <typename... TypeArgs, typename Fn>
    auto& register_type_collection(Fn&& fn) {
        using registration = type_registration<TypeArgs...>;
        return register_type<TypeArgs...>(callable([&] {
            return this->template construct_collection_runtime_request<
                typename registration::storage_type::type>(fn);
        }));
    }

    template <typename... TypeArgs> auto& register_type_collection() {
        return register_type_collection<TypeArgs...>(
            [](auto& collection, auto&& value) {
                collection_traits<std::decay_t<decltype(collection)>>::add(
                    collection, std::move(value));
            });
    }

  protected:
    template <typename T, typename IdType = none_t,
              typename R = typename annotated_traits<
                  std::conditional_t<std::is_rvalue_reference_v<T>,
                                     std::remove_reference_t<T>, T>>::type>
    R resolve_runtime_request(IdType&& id = IdType()) {
        if constexpr (detail::is_typed_key_v<IdType> &&
                      collection_traits<R>::is_collection) {
            return construct_collection_runtime_request<R>(
                std::decay_t<IdType>{});
        } else {
            if constexpr (cache_enabled) {
                if constexpr (is_none_v<std::decay_t<IdType>>) {
                    if (auto* state = runtime_state_if_present()) {
                        void* cache = state->type_cache.template get<T>();
                        if (cache) {
                            return detail::convert_resolved_binding<
                                typename annotated_traits<T>::type>(cache);
                        }
                    }
                } else {
                    if (auto* state = runtime_state_if_present()) {
                        auto data = state->type_bindings
                                        .template get<normalized_type_t<T>>();
                        if (data) {
                            if constexpr (detail::is_typed_key_v<IdType>) {
                                registered_binding_entry* candidate = nullptr;
                                for (auto&& p : data->bindings) {
                                    auto& entry = p.second;
                                    if (!entry.key_type ||
                                        !(*entry.key_type ==
                                          rtti_type::template get_type_index<
                                              std::decay_t<IdType>>())) {
                                        continue;
                                    }

                                    if (candidate) {
                                        candidate = nullptr;
                                        break;
                                    }
                                    candidate = &entry;
                                }

                                if (candidate && candidate->cache) {
                                    return detail::convert_resolved_binding<
                                        typename annotated_traits<T>::type>(
                                        candidate->cache);
                                }
                            } else {
                                auto indexed = data->template get_index<IdType>(
                                                       get_allocator())
                                                   .find(id);

                                if (indexed) {
                                    if (indexed->cache) {
                                        return detail::convert_resolved_binding<
                                            typename annotated_traits<T>::type>(
                                            indexed->cache);
                                    }
                                }
                            }
                        }
                    }
                }
            }

            runtime_context context;
            return resolve_impl<T, true, false, false>(
                context, std::forward<IdType>(id));
        }
    }

    template <typename T, typename Factory = constructor<normalized_type_t<T>>,
              typename R = typename annotated_traits<
                  std::conditional_t<std::is_rvalue_reference_v<T>,
                                     std::remove_reference_t<T>, T>>::type>
    R construct_runtime_request(Factory factory = Factory()) {
        runtime_context context;
        if constexpr (std::is_same_v<Factory,
                                     constructor<normalized_type_t<T>>>) {
            if (binding_status<T>() !=
                detail::binding_selection_status::not_found) {
                if constexpr (::dingo::
                                  rvalue_request_requires_explicit_conversion_v<
                                      T>) {
                    return resolve<T, false>(context, none_t{});
                } else if constexpr (construct_normalized_request_v<T>) {
                    return ::dingo::construct_request_or_wrap_normalized<T>(
                        [&]() { return resolve<T, false>(context, none_t{}); },
                        [&]() {
                            return resolve<normalized_type_t<T>, false>(
                                context, none_t{});
                        });
                } else {
                    return resolve<T, false>(context, none_t{});
                }
            } else if (binding_status<normalized_type_t<T>>() !=
                       detail::binding_selection_status::not_found) {
                if constexpr (::dingo::
                                  rvalue_request_requires_explicit_conversion_v<
                                      T>) {
                    ::dingo::throw_missing_rvalue_conversion<T>(true, context);
                } else if constexpr (construct_normalized_request_v<T>) {
                    return type_traits<std::decay_t<T>>::make(
                        resolve<normalized_type_t<T>, false>(context,
                                                             none_t{}));
                } else {
                    return resolve<T, false>(context, none_t{});
                }
            }
        }

        if constexpr (construct_factory_request_v<T>) {
            return factory.template construct<R>(context, *resolve_root());
        } else if constexpr (::dingo::
                                 rvalue_request_requires_explicit_conversion_v<
                                     T>) {
            ::dingo::throw_missing_rvalue_conversion<T>(false, context);
        } else {
            return resolve<T, false>(context, none_t{});
        }
    }

    template <typename T> T construct_collection_runtime_request() {
        return construct_collection_runtime_request<T>(
            [](auto& collection, auto&& value) {
                collection_traits<std::decay_t<decltype(collection)>>::add(
                    collection, std::move(value));
            });
    }

    template <typename T, typename Fn>
    T construct_collection_runtime_request(Fn&& fn) {
        return construct_collection_runtime_request<T>(std::forward<Fn>(fn),
                                                       none_t{});
    }

    template <typename T, typename Key>
    T construct_collection_runtime_request(key<Key>) {
        return construct_collection_runtime_request<T>(
            [](auto& collection, auto&& value) {
                collection_traits<std::decay_t<decltype(collection)>>::add(
                    collection, std::move(value));
            },
            key<Key>{});
    }

    template <typename T, typename Fn, typename Key>
    T construct_collection_runtime_request(Fn&& fn, key<Key>) {
        using collection_type = collection_traits<T>;
        using resolve_type = typename collection_type::resolve_type;

        static_assert(collection_type::is_collection,
                      "missing collection_traits specialization for type T");

        T results;
        runtime_context context;
        const std::size_t keyed_count = count_runtime_collection<T>(key<Key>{});
        if (keyed_count == 0) {
            throw detail::make_collection_type_not_found_exception<
                T, resolve_type>();
        }

        collection_type::reserve(results, keyed_count);
        append_runtime_collection(results, context, std::forward<Fn>(fn),
                                  key<Key>{});
        return results;
    }

    template <typename T, typename Fn>
    T construct_collection_runtime_request(Fn&& fn, none_t) {
        using collection_type = collection_traits<T>;
        using resolve_type = typename collection_type::resolve_type;

        static_assert(collection_type::is_collection,
                      "missing collection_traits specialization for type T");

        T results;
        runtime_context context;
        const std::size_t count = count_runtime_collection<T>(none_t{});
        if (count == 0) {
            throw detail::make_collection_type_not_found_exception<
                T, resolve_type>();
        }

        collection_type::reserve(results, count);
        append_runtime_collection(results, context, std::forward<Fn>(fn),
                                  none_t{});
        return results;
    }

    template <typename Signature = void, typename Callable>
    auto invoke_runtime_request(Callable&& callable) {
        using callable_type =
            std::remove_cv_t<std::remove_reference_t<Callable>>;
        using dispatch_signature =
            detail::callable_dispatch_signature_t<Signature, callable_type>;

        runtime_context context;
        auto type_guard = context.template track_type<callable_type>();
        return detail::callable_invoke<dispatch_signature>::construct(
            std::forward<Callable>(callable), context, *resolve_root());
    }

    template <typename Request, typename Key = void>
    detail::binding_selection_status binding_status() {
        using lookup_type = normalized_type_t<Request>;
        auto route = find_runtime_route(
            runtime_state_if_present()
                ? runtime_state_if_present()
                      ->type_bindings.template get<lookup_type>()
                : nullptr,
            std::conditional_t<std::is_void_v<Key>, none_t, key<Key>>{});
        return route.status;
    }

    template <typename T, typename Key = void, typename Fn>
    std::size_t append_collection(T& results, runtime_context& context,
                                  Fn&& fn) {
        if constexpr (std::is_void_v<Key>) {
            return append_runtime_collection(results, context,
                                             std::forward<Fn>(fn), none_t{});
        } else {
            return append_runtime_collection(results, context,
                                             std::forward<Fn>(fn), key<Key>{});
        }
    }

    template <typename T, typename Key = void> std::size_t count_collection() {
        if constexpr (std::is_void_v<Key>) {
            return count_runtime_collection<T>(none_t{});
        } else {
            return count_runtime_collection<T>(key<Key>{});
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
            return resolve<T, RemoveRvalueReferences>(context, none_t{});
        } else {
            return resolve<T, RemoveRvalueReferences>(context, key<Key>{});
        }
    }

    template <typename CachedT>
    static void store_type_cache(void* context, void* ptr) {
        static_cast<registry_type*>(context)
            ->ensure_runtime_state()
            .type_cache.template insert<CachedT>(ptr);
    }

    static void store_index_cache(void* context, void* ptr) {
        static_cast<binding_cache_state*>(context)->cache = ptr;
    }

    template <typename T> static T& invalid_registration_return();

    struct binding_cache_state {
        void* cache = nullptr;
    };

    struct index_data : binding_cache_state {
        runtime_binding_interface<container_type>* binding = nullptr;

        index_data() = default;

        explicit index_data(
            runtime_binding_interface<container_type>* binding_ptr)
            : binding(binding_ptr) {}

        operator bool() const { return binding != nullptr; }
    };

    struct registered_binding_entry : binding_cache_state {
        runtime_binding_ptr<runtime_binding_interface<container_type>> binding;
        std::optional<typename rtti_type::type_index> key_type;

        registered_binding_entry(
            runtime_binding_ptr<runtime_binding_interface<container_type>>&&
                binding_ptr,
            std::optional<typename rtti_type::type_index> resolved_key_type =
                std::nullopt)
            : binding(std::move(binding_ptr)),
              key_type(std::move(resolved_key_type)) {}

        registered_binding_entry(const registered_binding_entry&) = delete;
        registered_binding_entry&
        operator=(const registered_binding_entry&) = delete;

        registered_binding_entry(registered_binding_entry&& other) noexcept
            : binding_cache_state{other.cache},
              binding(std::move(other.binding)),
              key_type(std::move(other.key_type)) {
            other.cache = nullptr;
        }

        registered_binding_entry&
        operator=(registered_binding_entry&& other) noexcept {
            if (this != &other) {
                this->cache = other.cache;
                other.cache = nullptr;
                binding = std::move(other.binding);
                key_type = std::move(other.key_type);
            }
            return *this;
        }
    };

    using index_definition_list_type = to_type_list_t<index_definition_type>;
    using index_type = detail::index_impl<index_definition_list_type,
                                          index_data, allocator_type>;

    struct type_binding_data : index_type {
        type_binding_data(allocator_type& allocator)
            : index_type(allocator), bindings(allocator) {}

        typename ContainerTraits::template type_map_type<
            registered_binding_entry, allocator_type>
            bindings;
    };

    struct runtime_state {
        explicit runtime_state(allocator_type& allocator)
            : type_bindings(allocator), type_cache(allocator) {}

        typename ContainerTraits::template type_map_type<type_binding_data,
                                                         allocator_type>
            type_bindings;
        typename ContainerTraits::template type_cache_type<void*,
                                                           allocator_type>
            type_cache;
    };

    using runtime_binding_interface_type =
        runtime_binding_interface<container_type>;
    using runtime_route =
        detail::runtime_binding_route<runtime_binding_interface_type,
                                      binding_cache_state*>;

  protected:
    template <typename T, typename Key, typename Fn>
    std::size_t append_runtime_collection(T& results, runtime_context& context,
                                          Fn&& fn, key<Key>) {
        using collection_type = collection_traits<T>;
        using resolve_type = typename collection_type::resolve_type;
        using lookup_type = normalized_type_t<resolve_type>;

        auto* state = runtime_state_if_present();
        auto data =
            state ? state->type_bindings.template get<lookup_type>() : nullptr;
        if (!data) {
            return 0;
        }

        std::size_t count = 0;
        for (auto&& p : data->bindings) {
            auto& entry = p.second;
            if (entry.key_type &&
                *entry.key_type ==
                    rtti_type::template get_type_index<key<Key>>()) {
                ++count;
                fn(results, resolve_collection_type<resolve_type>(
                                *entry.binding, context));
            }
        }

        return count;
    }

    template <typename T, typename Fn>
    std::size_t append_runtime_collection(T& results, runtime_context& context,
                                          Fn&& fn, none_t) {
        using collection_type = collection_traits<T>;
        using resolve_type = typename collection_type::resolve_type;
        using lookup_type = normalized_type_t<resolve_type>;

        auto* state = runtime_state_if_present();
        auto data =
            state ? state->type_bindings.template get<lookup_type>() : nullptr;
        if (!data) {
            return 0;
        }

        std::size_t count = 0;
        for (auto&& p : data->bindings) {
            ++count;
            fn(results, resolve_collection_type<resolve_type>(*p.second.binding,
                                                              context));
        }

        return count;
    }

    template <typename T, typename Key>
    std::size_t count_runtime_collection(key<Key>) {
        using collection_type = collection_traits<T>;
        using resolve_type = typename collection_type::resolve_type;
        using lookup_type = normalized_type_t<resolve_type>;

        auto* state = runtime_state_if_present();
        auto data =
            state ? state->type_bindings.template get<lookup_type>() : nullptr;
        if (!data) {
            return 0;
        }

        std::size_t count = 0;
        for (auto&& p : data->bindings) {
            auto& entry = p.second;
            if (entry.key_type &&
                *entry.key_type ==
                    rtti_type::template get_type_index<key<Key>>()) {
                ++count;
            }
        }

        return count;
    }

    template <typename T> std::size_t count_runtime_collection(none_t) {
        using collection_type = collection_traits<T>;
        using resolve_type = typename collection_type::resolve_type;
        using lookup_type = normalized_type_t<resolve_type>;

        auto* state = runtime_state_if_present();
        auto data =
            state ? state->type_bindings.template get<lookup_type>() : nullptr;
        return data ? data->bindings.size() : 0;
    }

  private:
    template <typename IdType>
    runtime_route find_runtime_route(type_binding_data* data, IdType&& id) {
        if constexpr (is_none_v<std::decay_t<IdType>>) {
            return detail::make_runtime_route<runtime_binding_interface_type,
                                              binding_cache_state*>(
                [&](auto&& select) {
                    if (!data) {
                        return;
                    }

                    for (auto&& p : data->bindings) {
                        auto& entry = p.second;
                        select(*entry.binding, &entry);
                    }
                });
        } else if constexpr (detail::is_typed_key_v<IdType>) {
            return detail::make_runtime_route<runtime_binding_interface_type,
                                              binding_cache_state*>(
                [&](auto&& select) {
                    if (!data) {
                        return;
                    }

                    for (auto&& p : data->bindings) {
                        auto& entry = p.second;
                        if (!entry.key_type ||
                            !(*entry.key_type ==
                              rtti_type::template get_type_index<
                                  std::decay_t<IdType>>())) {
                            continue;
                        }

                        select(*entry.binding, &entry);
                    }
                });
        } else {
            using index_key_type = std::decay_t<IdType>;
            auto indexed =
                data ? data->template get_index<index_key_type>(get_allocator())
                           .find(id)
                     : nullptr;
            return detail::make_runtime_route<runtime_binding_interface_type,
                                              binding_cache_state*>(
                indexed ? indexed->binding : nullptr, indexed);
        }
    }

    template <typename T, bool CheckCache, typename IdType>
    auto resolve_runtime_route(runtime_route route, runtime_context& context,
                               IdType&&) -> typename annotated_traits<T>::type {
        if constexpr (is_none_v<std::decay_t<IdType>>) {
            return resolve<T, typename annotated_traits<T>::type>(
                *route.binding, context);
        } else {
            if constexpr (cache_enabled && CheckCache) {
                if (route.state->cache) {
                    return detail::convert_resolved_binding<
                        typename annotated_traits<T>::type>(route.state->cache);
                }
            }

            return resolve<T, typename annotated_traits<T>::type>(
                *route.binding, context, *route.state);
        }
    }

    template <typename T, bool RemoveRvalueReferences, bool MayAutoConstruct,
              typename IdType,
              typename R = std::conditional_t<
                  RemoveRvalueReferences,
                  std::conditional_t<std::is_rvalue_reference_v<T>,
                                     std::remove_reference_t<T>, T>,
                  T>>
    R resolve_missing_binding(runtime_context& context, IdType&& id) {
        using Type = normalized_type_t<T>;

        if constexpr (!std::is_same_v<void*, decltype(parent_)>) {
            if (parent_) {
                if constexpr (!std::is_same_v<void, ResolveRoot> &&
                              !std::is_base_of_v<registry_type,
                                                 resolve_root_type>) {
                    if constexpr (is_none_v<std::decay_t<IdType>>) {
                        return resolve_root()
                            ->template resolve<T, RemoveRvalueReferences, true>(
                                context, none_t{});
                    } else if constexpr (detail::is_typed_key_v<IdType>) {
                        return resolve_root()
                            ->template resolve<T, RemoveRvalueReferences, true>(
                                context, std::decay_t<IdType>{});
                    } else {
                        return parent_->template resolve_impl<
                            T, RemoveRvalueReferences, MayAutoConstruct>(
                            context, std::forward<IdType>(id));
                    }
                } else {
                    return parent_->template resolve_impl<
                        T, RemoveRvalueReferences, MayAutoConstruct>(
                        context, std::forward<IdType>(id));
                }
            }
        }

        if constexpr (MayAutoConstruct && detail::is_typed_key_v<IdType> &&
                      collection_traits<R>::is_collection) {
            return this->template construct_collection_runtime_request<R>(
                [&](auto& collection, auto&& value) {
                    collection_traits<std::decay_t<decltype(collection)>>::add(
                        collection, std::move(value));
                },
                std::decay_t<IdType>{});
        } else if constexpr (MayAutoConstruct &&
                             is_auto_constructible<std::decay_t<T>>::value &&
                             constructor<Type>::kind ==
                                 detail::constructor_kind::concrete) {
            return auto_construct<T>(context);
        } else if constexpr (is_none_v<std::decay_t<IdType>>) {
            throw detail::make_type_not_found_exception<T>(context);
        } else {
            throw detail::make_type_not_found_exception<T,
                                                        std::decay_t<IdType>>(
                context);
        }
    }

    template <typename... TypeArgs, typename Arg, typename IdType>
    auto& register_type_impl(Arg&& arg, IdType&& id) {
        static_assert(!detail::has_explicit_void_interface_v<TypeArgs...>,
                      "interfaces<void> is not a valid registration target");
        using registration =
            std::conditional_t<!is_none_v<std::decay_t<Arg>>,
                               type_registration<TypeArgs..., factory<Arg>>,
                               type_registration<TypeArgs...>>;
        using binding_model = detail::binding_model<registration>;
        using bindings_type = typename binding_model::bindings_type;
        using instance_container_type =
            registration_container_type<registration>;
        using resolution_container_type = std::conditional_t<
            std::is_void_v<bindings_type>, instance_container_type,
            detail::binding_resolution<resolve_root_type, bindings_type>>;
        (void)arg;
        using interface_types = typename binding_model::interface_types;
        static constexpr bool storage_tag_is_complete =
            binding_model::storage_tag_is_complete;
        static_assert(storage_tag_is_complete,
                      "registered storage tag must be complete; include the "
                      "corresponding "
                      "dingo/storage header");
        if constexpr (storage_tag_is_complete) {
            using storage_type = typename binding_model::storage_type;
            using registration_requirements =
                typename binding_model::requirements;
            using key_id_type = std::conditional_t<
                std::is_void_v<typename binding_model::key_type>, none_t,
                key<typename binding_model::key_type>>;

            registration_requirements::assert_valid();

            using runtime_binding_data_type =
                runtime_binding_data<instance_container_type, storage_type,
                                     resolution_container_type>;

            if constexpr (registration_requirements::valid &&
                          type_list_size_v<interface_types> == 1) {
                using interface_type = type_list_head_t<interface_types>;

                using runtime_binding_type = runtime_binding<
                    container_type,
                    typename annotated_traits<interface_type>::type,
                    storage_type, runtime_binding_data_type>;
                using registered_binding_type = std::conditional_t<
                    is_none_v<key_id_type>, runtime_binding_type,
                    detail::keyed_binding_identity<key_id_type,
                                                   runtime_binding_type>>;

                if constexpr (!is_none_v<std::decay_t<Arg>>) {
                    auto&& [binding, binding_container] =
                        allocate_binding<registered_binding_type>(
                            resolve_root(), std::forward<Arg>(arg));
                    register_type_binding<interface_type, storage_type>(
                        std::move(binding), std::move(id), key_id_type{});
                    return *binding_container;
                } else {
                    auto&& [binding, binding_container] =
                        allocate_binding<registered_binding_type>(
                            resolve_root());
                    register_type_binding<interface_type, storage_type>(
                        std::move(binding), std::move(id), key_id_type{});
                    return *binding_container;
                }
            } else {
                if constexpr (registration_requirements::valid) {
                    std::shared_ptr<runtime_binding_data_type> data;
                    if constexpr (!is_none_v<std::decay_t<Arg>>) {
                        data = std::allocate_shared<runtime_binding_data_type>(
                            allocator_traits::rebind<runtime_binding_data_type>(
                                get_allocator()),
                            resolve_root(), std::forward<Arg>(arg));
                    } else {
                        data = std::allocate_shared<runtime_binding_data_type>(
                            allocator_traits::rebind<runtime_binding_data_type>(
                                get_allocator()),
                            resolve_root());
                    }

                    for_each(interface_types{}, [&](auto element) {
                        using interface_type = typename decltype(element)::type;

                        using runtime_binding_type = runtime_binding<
                            container_type,
                            typename annotated_traits<interface_type>::type,
                            storage_type,
                            std::shared_ptr<runtime_binding_data_type>>;
                        using registered_binding_type = std::conditional_t<
                            is_none_v<key_id_type>, runtime_binding_type,
                            detail::keyed_binding_identity<
                                key_id_type, runtime_binding_type>>;

                        register_type_binding<interface_type, storage_type>(
                            allocate_binding<registered_binding_type>(data)
                                .first,
                            id, key_id_type{});
                    });
                    return data->container;
                } else {
                    return invalid_registration_return<
                        instance_container_type>();
                }
            }
        } else {
            return invalid_registration_return<instance_container_type>();
        }
    }

    template <typename TypeInterface, typename TypeStorage, typename Binding,
              typename IdType, typename KeyIdType>
    void register_type_binding(Binding&& binding, IdType&& id, KeyIdType) {
        check_interface_requirements<
            TypeStorage, typename annotated_traits<TypeInterface>::type,
            typename TypeStorage::type>();

        auto pb =
            ensure_runtime_state().type_bindings.template insert<TypeInterface>(
                get_allocator());
        auto& data = pb.first;
        using binding_registration_key = std::conditional_t<
            is_none_v<std::decay_t<KeyIdType>>,
            type_list<TypeInterface, typename TypeStorage::type>,
            type_list<TypeInterface, typename TypeStorage::type,
                      std::decay_t<KeyIdType>>>;

        auto inserted_binding = [&]() {
            if constexpr (is_none_v<std::decay_t<KeyIdType>>) {
                return data.bindings.template insert<binding_registration_key>(
                    std::forward<Binding>(binding), std::nullopt);
            } else {
                return data.bindings.template insert<binding_registration_key>(
                    std::forward<Binding>(binding),
                    rtti_type::template get_type_index<
                        std::decay_t<KeyIdType>>());
            }
        }();
        if (!inserted_binding.second) {
            if constexpr (is_none_v<std::decay_t<KeyIdType>>) {
                throw detail::make_type_already_registered_exception<
                    TypeInterface, typename TypeStorage::type>();
            } else {
                throw detail::make_type_index_already_registered_exception<
                    TypeInterface, typename TypeStorage::type,
                    std::decay_t<KeyIdType>>();
            }
        }
        auto binding_ptr = inserted_binding.first.binding.get();
        runtime_bindings_present_ = true;

        if constexpr (!is_none_v<std::decay_t<IdType>>) {
            if (!data.template get_index<IdType>(get_allocator())
                     .emplace(std::forward<IdType>(id),
                              index_data{binding_ptr})) {
                bool erased =
                    data.bindings.template erase<binding_registration_key>();
                assert(erased);
                (void)erased;
                throw detail::make_type_index_already_registered_exception<
                    TypeInterface, typename TypeStorage::type, IdType>();
            }
        }
    }

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4702)
#endif
    template <typename T, bool RemoveRvalueReferences, bool MayAutoConstruct,
              bool CheckCache = true, typename IdType = none_t,
              typename R = std::conditional_t<
                  RemoveRvalueReferences,
                  std::conditional_t<std::is_rvalue_reference_v<T>,
                                     std::remove_reference_t<T>, T>,
                  T>>
    R resolve_impl(runtime_context& context, IdType&& id = IdType()) {
        using Type = normalized_type_t<T>;
        static_assert(!std::is_const_v<Type>);

        if constexpr (cache_enabled && CheckCache) {
            if (auto* state = runtime_state_if_present()) {
                void* cache = state->type_cache.template get<T>();
                if (cache) {
                    return detail::convert_resolved_binding<
                        typename annotated_traits<T>::type>(cache);
                }
            }
        }

        auto route = find_runtime_route(
            runtime_state_if_present()
                ? runtime_state_if_present()->type_bindings.template get<Type>()
                : nullptr,
            id);
        if (route.found()) {
            return resolve_runtime_route<T, CheckCache>(
                route, context, std::forward<IdType>(id));
        } else if (route.ambiguous()) {
            throw detail::make_type_ambiguous_exception<T>(context);
        }
        return resolve_missing_binding<T, RemoveRvalueReferences,
                                       MayAutoConstruct>(
            context, std::forward<IdType>(id));
    }

    template <typename T>
    decltype(auto) auto_construct(runtime_context& context) {
        using Type = normalized_type_t<T>;

        static_assert(is_complete<Type>::value,
                      "auto-construction requires a complete type");

        using type_detection = detail::automatic;
        return context.template construct_temporary<
            typename annotated_traits<T>::type, type_detection>(
            *resolve_root());
    }

    template <typename T, bool RemoveRvalueReferences, bool CheckCache = true,
              typename IdType = none_t,
              typename R = std::conditional_t<
                  RemoveRvalueReferences,
                  std::conditional_t<std::is_rvalue_reference_v<T>,
                                     std::remove_reference_t<T>, T>,
                  T>>
    R resolve(runtime_context& context, IdType&& id = IdType()) {
        return resolve_impl < T, RemoveRvalueReferences,
               std::is_same_v<normalized_type_t<T>, std::decay_t<T>> &&
                   (!std::is_reference_v<T> ||
                    (std::is_lvalue_reference_v<T> &&
                     std::is_const_v<std::remove_reference_t<T>> &&
                     is_auto_constructible<std::decay_t<T>>::value)),
               CheckCache > (context, std::forward<IdType>(id));
    }
#ifdef _MSC_VER
#pragma warning(pop)
#endif

    template <typename CachedT, typename T, typename Binding, typename Context>
    T resolve(Binding& binding, Context& context) {
        return ::dingo::resolve_binding_request<T, rtti_type>(
            binding, context,
            cache_enabled
                ? instance_cache_sink{this,
                                      &registry_type::store_type_cache<CachedT>}
                : instance_cache_sink{});
    }

    template <typename CachedT, typename T, typename Binding, typename Context>
    T resolve(Binding& binding, Context& context, index_data& data) {
        return ::dingo::resolve_binding_request<T, rtti_type>(
            binding, context,
            cache_enabled
                ? instance_cache_sink{&data, &registry_type::store_index_cache}
                : instance_cache_sink{});
    }

    template <typename CachedT, typename T, typename Binding, typename Context>
    T resolve(Binding& binding, Context& context, binding_cache_state& data) {
        return ::dingo::resolve_binding_request<T, rtti_type>(
            binding, context,
            cache_enabled
                ? instance_cache_sink{&data, &registry_type::store_index_cache}
                : instance_cache_sink{});
    }

    template <typename T, typename Binding, typename Context>
    T resolve_collection_type(Binding& binding, Context& context) {
        return ::dingo::resolve_binding_request<T, rtti_type>(binding, context);
    }

    template <class Storage, class TypeInterface, class Type>
    void check_interface_requirements() {
        detail::interface_registration_requirements<Storage, TypeInterface,
                                                    Type>::assert_valid();
    }

    template <typename U, typename... Args>
    std::pair<runtime_binding_ptr<runtime_binding_interface<container_type>>,
              typename U::container_type*>
    allocate_binding(Args&&... args) {
        auto alloc = allocator_traits::rebind<U>(get_allocator());
        U* instance = allocator_traits::allocate(alloc, 1);
        if (!instance)
            return {nullptr, nullptr};

        allocator_traits::construct(alloc, instance,
                                    std::forward<Args>(args)...);

        return std::make_pair(
            runtime_binding_ptr<runtime_binding_interface<container_type>>(
                instance, &registry_type::template destroy_binding<U>),
            &instance->get_container());
    }

    template <typename U>
    static void
    destroy_binding(runtime_binding_interface<container_type>* ptr) {
        auto* instance = static_cast<U*>(ptr);
        auto alloc = allocator_traits::rebind<U>(
            instance->get_container().get_allocator());
        allocator_traits::destroy(alloc, instance);
        allocator_traits::deallocate(alloc, instance, 1);
    }

    parent_registry_type* parent_ = nullptr;

    std::unique_ptr<runtime_state> runtime_state_;

    bool runtime_bindings_present_ = false;
};

template <typename ContainerTraits, typename Allocator, typename ParentRegistry,
          typename ResolveRoot>
auto runtime_registry<ContainerTraits, Allocator, ParentRegistry,
                      ResolveRoot>::injector() ->
    typename runtime_registry<ContainerTraits, Allocator, ParentRegistry,
                              ResolveRoot>::runtime_injector_type {
    return runtime_injector_type(*this);
}

} // namespace dingo
