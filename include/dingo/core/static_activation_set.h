//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/core/binding_model.h>
#include <dingo/core/binding_resolution.h>
#include <dingo/core/context_base.h>
#include <dingo/core/exceptions.h>
#include <dingo/core/factory_traits.h>
#include <dingo/registration/collection_traits.h>
#include <dingo/rtti/static_provider.h>
#include <dingo/static/graph.h>
#include <dingo/type/type_descriptor.h>

#include <cstddef>
#include <type_traits>
#include <utility>

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4702)
#endif

namespace dingo {
namespace detail {

template <typename Registration> struct binding_storage_slot {
    using binding_model = detail::binding_model<Registration>;
    using storage_type = typename binding_model::storage_type;

    storage_type storage;
};

template <typename BindingModel>
struct binding_factory_is_default_constructible
    : std::bool_constant<std::is_default_constructible_v<
          typename BindingModel::factory_type>> {};

template <typename BindingModel>
struct binding_storage_is_default_constructible
    : std::bool_constant<std::is_default_constructible_v<
          typename BindingModel::storage_type>> {};

struct no_dependency_context {
    template <typename T, typename Container> T resolve(Container&) = delete;
};

template <typename Factory>
inline constexpr bool factory_without_dependencies_v =
    std::is_same_v<typename factory_traits<Factory>::dependencies, type_list<>>;

template <typename Request, typename StorageType>
inline constexpr bool stored_request_identity_v = [] {
    using stored_type = std::remove_cv_t<
        std::remove_reference_t<typename StorageType::stored_type>>;
    return std::is_same_v<Request, stored_type> ||
           std::is_same_v<Request, stored_type&> ||
           std::is_same_v<Request, const stored_type&> ||
           std::is_same_v<Request, stored_type*> ||
           std::is_same_v<Request, const stored_type*>;
}();

template <typename BindingModel>
inline constexpr bool binding_has_conversion_cache_v = [] {
    using conversions_type = typename BindingModel::storage_type::conversions;
    return conversions_type::is_stable &&
           type_list_size_v<typename conversions_type::conversion_types> != 0;
}();

template <typename Closure, typename Registration> struct binding_closure_slot {
    Closure closure;
};

template <typename Registration, bool Enabled = binding_has_conversion_cache_v<
                                     detail::binding_model<Registration>>>
struct binding_conversion_cache_slot;

template <typename Registration, bool Enabled>
struct registration_conversion_types {
    using binding_model = detail::binding_model<Registration>;
    using storage_type = typename binding_model::storage_type;
    using conversions_type = typename storage_type::conversions;
    using type = typename conversions_type::conversion_types;
};

template <typename Registration, bool Enabled>
using registration_conversion_cache_base = binding_conversion_cache_base<
    Enabled,
    typename registration_conversion_types<Registration, Enabled>::type>;

template <typename Registration, bool Enabled>
struct binding_conversion_cache_slot
    : registration_conversion_cache_base<Registration, Enabled> {
    using binding_model = detail::binding_model<Registration>;
    using conversions_type = typename binding_model::storage_type::conversions;
    using base_type = registration_conversion_cache_base<Registration, Enabled>;

    using base_type::construct_conversion;
};

template <bool RuntimeDependencies, typename... Registrations>
struct basic_static_activation_closure;

template <typename... Registrations>
struct basic_static_activation_closure<false, Registrations...> {
    using type = detail::static_context_closure<
        detail::basic_static_execution_traits<static_registry<Registrations...>,
                                              false>::max_destructible_slots,
        detail::basic_static_execution_traits<static_registry<Registrations...>,
                                              false>::max_temporary_slots,
        detail::basic_static_execution_traits<static_registry<Registrations...>,
                                              false>::max_temporary_size == 0
            ? 1
            : detail::basic_static_execution_traits<
                  static_registry<Registrations...>, false>::max_temporary_size,
        detail::basic_static_execution_traits<static_registry<Registrations...>,
                                              false>::max_temporary_align == 0
            ? alignof(std::max_align_t)
            : detail::basic_static_execution_traits<
                  static_registry<Registrations...>,
                  false>::max_temporary_align>;
};

template <typename... Registrations>
struct basic_static_activation_closure<true, Registrations...> {
    using type = detail::fixed_context_closure<
        detail::basic_static_execution_traits<static_registry<Registrations...>,
                                              true>::max_destructible_slots,
        detail::basic_static_execution_traits<static_registry<Registrations...>,
                                              true>::max_temporary_slots,
        detail::basic_static_execution_traits<static_registry<Registrations...>,
                                              true>::max_temporary_size == 0
            ? 1
            : detail::basic_static_execution_traits<
                  static_registry<Registrations...>, true>::max_temporary_size,
        detail::basic_static_execution_traits<static_registry<Registrations...>,
                                              true>::max_temporary_align == 0
            ? alignof(std::max_align_t)
            : detail::basic_static_execution_traits<
                  static_registry<Registrations...>,
                  true>::max_temporary_align>;
};

template <bool RuntimeDependencies, typename... Registrations>
using static_activation_closure_t =
    typename basic_static_activation_closure<RuntimeDependencies,
                                             Registrations...>::type;

template <typename... Registrations>
class static_binding_storage
    : private binding_conversion_cache_slot<Registrations>...,
      private binding_storage_slot<Registrations>... {
    template <typename Registration>
    using storage_slot = binding_storage_slot<Registration>;

    template <typename Registration>
    using conversion_cache_slot = binding_conversion_cache_slot<Registration>;

  public:
    template <typename Registration> auto& get_storage() {
        return static_cast<storage_slot<Registration>&>(*this).storage;
    }

    template <typename BindingModel> auto& get_storage_for_model() {
        return get_storage<typename BindingModel::registration_type>();
    }

    template <typename BindingModel> auto& get_conversion_cache_for_model() {
        return static_cast<
            conversion_cache_slot<typename BindingModel::registration_type>&>(
            *this);
    }
};

template <typename State, typename Host, typename BindingModel>
struct binding_activation {
    State& state;
    Host& host;

    template <typename T, typename Context>
    decltype(auto) resolve(Context& context) {
        return state.template resolve_binding_type<T, BindingModel>(host,
                                                                    context);
    }

    template <typename T, typename Context, typename Source>
    decltype(auto) resolve_conversion(Context& context, Source&& source) {
        return state.template resolve_conversion<T, BindingModel>(
            context, std::forward<Source>(source));
    }
};

template <typename Request, typename StorageType> struct request_capabilities {
    using conversions = typename StorageType::conversions;
    using request_leaf_type =
        leaf_type_t<std::remove_cv_t<std::remove_reference_t<Request>>>;
    using base_types = std::conditional_t<
        std::is_pointer_v<Request>, typename conversions::pointer_types,
        std::conditional_t<
            std::is_lvalue_reference_v<Request>,
            typename conversions::lvalue_reference_types,
            std::conditional_t<std::is_rvalue_reference_v<Request>,
                               typename conversions::rvalue_reference_types,
                               typename conversions::value_types>>>;

    template <typename Type, typename Leaf,
              bool NeedsResolution =
                  std::is_same_v<leaf_type_t<Type>, runtime_type>>
    struct resolved_request_conversion_type {
        using type = Type;
    };

    template <typename Type, typename Leaf>
    struct resolved_request_conversion_type<Type, Leaf, true> {
        using type = resolved_type_t<Type, Leaf>;
    };

    template <typename Types, typename Leaf>
    struct resolved_request_conversion_list;

    template <typename Leaf, typename... Types>
    struct resolved_request_conversion_list<type_list<Types...>, Leaf> {
        using type = type_list<
            typename resolved_request_conversion_type<Types, Leaf>::type...>;
    };

    using type =
        typename resolved_request_conversion_list<base_types,
                                                  request_leaf_type>::type;
};

template <typename Request, typename CapabilityTypes>
struct request_capability_match;

template <typename Request>
struct request_capability_match<Request, type_list<>> {
    using type = void;
};

template <typename Request, typename Head, typename... Tail>
struct request_capability_match<Request, type_list<Head, Tail...>> {
    using type = std::conditional_t<
        std::is_same_v<lookup_type_t<Head>, request_lookup_type_t<Request>>,
        Head,
        typename request_capability_match<Request, type_list<Tail...>>::type>;
};

template <typename Request, typename InterfaceBinding>
struct binding_supports_request {
    using storage_type =
        typename InterfaceBinding::binding_model_type::storage_type;
    using capability_types =
        typename request_capabilities<Request, storage_type>::type;
    using type =
        typename request_capability_match<Request, capability_types>::type;

    static constexpr bool value = !std::is_void_v<type>;
};

template <typename Request, typename InterfaceBinding>
inline constexpr bool binding_supports_request_v =
    binding_supports_request<Request, InterfaceBinding>::value;

template <typename State, typename Host, typename InterfaceBinding>
struct static_route {
    using binding_model_type = typename InterfaceBinding::binding_model_type;
    using interface_type = typename InterfaceBinding::interface_type;
    using storage_type = typename binding_model_type::storage_type;

    State& state;
    Host& host;

    static constexpr type_descriptor registered_type() {
        return describe_type<typename storage_type::type>();
    }

    template <typename Request, typename Context>
    Request resolve(Context& context, instance_cache_sink cache = {}) {
        static_assert(State::runtime_dependencies ||
                          binding_supports_request_v<Request, InterfaceBinding>,
                      "static resolution cannot satisfy a request the storage "
                      "does not publish");
        using capability_types =
            typename request_capabilities<Request, storage_type>::type;
        using capability =
            typename request_capability_match<Request, capability_types>::type;
        void* ptr = nullptr;
        if constexpr (!std::is_void_v<capability>) {
            // Stay on the normal materialization path even for identity
            // pointer/reference requests. Some storages publish the stored
            // instance through pointer-like source capabilities, so taking the
            // address of `storage.resolve(...)` would capture the address of a
            // stack-local pointer variable instead of the bound object.
            ptr = resolve_request_address<Request, capability>(context);
        } else {
            throw detail::make_type_not_convertible_exception(
                describe_type<Request>(), registered_type(), context);
        }
        if constexpr (storage_type::conversions::is_stable) {
            cache(ptr);
        }
        return detail::convert_resolved_binding<Request>(ptr);
    }

    template <typename Request, typename Context, typename Fn>
    decltype(auto) consume(Context& context, Fn&& fn) {
        static_assert(State::runtime_dependencies ||
                          binding_supports_request_v<Request, InterfaceBinding>,
                      "static resolution cannot satisfy a request the storage "
                      "does not publish");
        using capability_types =
            typename request_capabilities<Request, storage_type>::type;
        using capability =
            typename request_capability_match<Request, capability_types>::type;

        if constexpr (!std::is_void_v<capability>) {
            return consume_request<Request, capability>(context,
                                                        std::forward<Fn>(fn));
        } else {
            throw detail::make_type_not_convertible_exception(
                describe_type<Request>(), registered_type(), context);
        }
    }

    template <typename Request, typename T, typename Context>
    void* resolve_request_address(Context& context) {
        using conversion_request_type =
            std::remove_reference_t<resolved_type_t<T, interface_type>>;
        using target_type =
            std::remove_reference_t<resolved_type_t<T, interface_type>>;
        constexpr bool uses_stored_request_identity =
            stored_request_identity_v<Request, storage_type>;

        binding_activation<State, Host, binding_model_type> activation{state,
                                                                       host};
        if constexpr (uses_stored_request_identity) {
            if constexpr (!State::runtime_dependencies) {
                return detail::materialize_binding_source(
                    context,
                    state.template get_storage_for_model<binding_model_type>(),
                    host, [&](auto&& source) -> void* {
                        auto&& instance =
                            detail::resolve_binding_request<Request,
                                                            storage_type>(
                                activation, context,
                                std::forward<decltype(source)>(source));
                        return detail::get_address_as<target_type>(
                            context,
                            std::forward<decltype(instance)>(instance));
                    });
            } else {
                return detail::forward_binding_request<Request>(
                    context,
                    state.template get_storage_for_model<binding_model_type>(),
                    host, activation, [&](auto&& instance) -> void* {
                        return detail::get_address_as<target_type>(
                            context,
                            std::forward<decltype(instance)>(instance));
                    });
            }
        } else {
            return detail::forward_binding_resolution_request<
                conversion_request_type>(
                context,
                state.template get_storage_for_model<binding_model_type>(),
                host,
                state.template get_closure<
                    typename binding_model_type::registration_type>(),
                activation, [&](auto&& instance) -> void* {
                    return detail::get_address_as<target_type>(
                        context, std::forward<decltype(instance)>(instance));
                });
        }
    }

    template <typename Request, typename T, typename Context, typename Fn>
    decltype(auto) consume_request(Context& context, Fn&& fn) {
        constexpr bool uses_stored_request_identity =
            stored_request_identity_v<Request, storage_type>;

        if constexpr (uses_stored_request_identity) {
            binding_activation<State, Host, binding_model_type> activation{
                state, host};
            if constexpr (!State::runtime_dependencies) {
                return detail::materialize_binding_source(
                    context,
                    state.template get_storage_for_model<binding_model_type>(),
                    host, [&](auto&& source) -> decltype(auto) {
                        auto&& instance =
                            detail::resolve_binding_request<Request,
                                                            storage_type>(
                                activation, context,
                                std::forward<decltype(source)>(source));
                        return detail::consume_resolved_binding<Request>(
                            std::forward<decltype(instance)>(instance),
                            std::forward<Fn>(fn));
                    });
            } else {
                return detail::consume_binding_request<Request>(
                    context,
                    state.template get_storage_for_model<binding_model_type>(),
                    host, activation, std::forward<Fn>(fn));
            }
        } else {
            binding_activation<State, Host, binding_model_type> activation{
                state, host};
            return detail::consume_binding_resolution_request<Request>(
                context,
                state.template get_storage_for_model<binding_model_type>(),
                host,
                state.template get_closure<
                    typename binding_model_type::registration_type>(),
                activation, std::forward<Fn>(fn));
        }
    }
};

template <typename T, typename BindingModel, typename State, typename Context,
          typename Source>
decltype(auto) evaluate_static_conversion(State& state, Context& context,
                                          Source&& source) {
    return detail::resolve_binding_conversion<T>(
        state.template get_storage_for_model<BindingModel>(),
        state.template get_conversion_cache_for_model<BindingModel>(), context,
        std::forward<Source>(source));
}

template <typename T, typename BindingModel, typename State, typename Host,
          typename Context>
decltype(auto) evaluate_static_binding(State& state, Host& host,
                                       Context& context) {
    binding_activation<State, Host, BindingModel> activation{state, host};
    return detail::materialize_tracked_binding_source(
        context, state.template get_storage_for_model<BindingModel>(), host,
        [&](auto&& source) -> decltype(auto) {
            return detail::resolve_binding_value<T>(
                activation, context, std::forward<decltype(source)>(source));
        });
}

template <typename InterfaceBinding, typename State, typename Host>
auto make_static_route(State& state, Host& host) {
    return static_route<State, Host, InterfaceBinding>{state, host};
}

template <typename T, typename Key, typename StaticRegistryType, typename State,
          typename Host, typename Fn, typename Context>
std::size_t append_static_collection_impl(State& state, T& results, Host& host,
                                          Context& context, Fn&& fn) {
    using collection_type = collection_traits<T>;
    using resolve_type = typename collection_type::resolve_type;
    using interface_bindings = typename StaticRegistryType::template bindings<
        normalized_type_t<resolve_type>, Key>;

    static_assert(collection_type::is_collection,
                  "missing collection_traits specialization for type T");

    constexpr std::size_t count = type_list_size_v<interface_bindings>;
    if constexpr (count != 0) {
        dingo::for_each(interface_bindings{}, [&](auto binding_iterator) {
            using interface_binding = typename decltype(binding_iterator)::type;
            auto route = make_static_route<interface_binding>(state, host);
            if constexpr (copy_on_resolve_v<resolve_type> &&
                          !std::is_reference_v<resolve_type>) {
                route.template consume<resolve_type>(
                    context, [&](auto&& value) {
                        fn(results, std::forward<decltype(value)>(value));
                    });
            } else {
                fn(results, route.template resolve<resolve_type>(context));
            }
        });
    }

    return count;
}

template <typename T, typename Key, typename StaticRegistryType, typename State,
          typename Host, typename Fn, typename Context>
T construct_static_collection_impl(State& state, Host& host, Context& context,
                                   Fn&& fn) {
    using collection_type = collection_traits<T>;
    using resolve_type = typename collection_type::resolve_type;
    using interface_bindings = typename StaticRegistryType::template bindings<
        normalized_type_t<resolve_type>, Key>;

    static_assert(collection_type::is_collection,
                  "missing collection_traits specialization for type T");
    static_assert(
        type_list_size_v<interface_bindings> != 0,
        "static_injector cannot construct a collection for an unbound type");

    T results;
    collection_type::reserve(results, type_list_size_v<interface_bindings>);
    append_static_collection_impl<T, Key, StaticRegistryType>(
        state, results, host, context, std::forward<Fn>(fn));
    return results;
}

template <typename T, typename Key, typename StaticRegistryType, typename State,
          typename Host, typename Context>
T construct_static_collection_default_impl(State& state, Host& host,
                                           Context& context) {
    using collection_type = collection_traits<T>;
    using resolve_type = typename collection_type::resolve_type;
    using interface_bindings = typename StaticRegistryType::template bindings<
        normalized_type_t<resolve_type>, Key>;

    static_assert(collection_type::is_collection,
                  "missing collection_traits specialization for type T");
    static_assert(
        type_list_size_v<interface_bindings> != 0,
        "static_injector cannot construct a collection for an unbound type");

    if constexpr (collection_type::has_fixed_size_construct) {
        T results = collection_type::make_fixed_size(
            type_list_size_v<interface_bindings>);
        std::size_t index = 0;
        append_static_collection_impl<T, Key, StaticRegistryType>(
            state, results, host, context, [&](auto&, auto&& value) {
                collection_type::set(results, index,
                                     std::forward<decltype(value)>(value));
                ++index;
            });
        return results;
    } else {
        return construct_static_collection_impl<T, Key, StaticRegistryType>(
            state, host, context, [](auto& collection, auto&& value) {
                collection_type::add(collection,
                                     std::forward<decltype(value)>(value));
            });
    }
}

template <typename Derived, bool RuntimeDependencies, typename... Registrations>
class basic_static_activation_set_base
    : private binding_closure_slot<
          static_activation_closure_t<RuntimeDependencies, Registrations...>,
          Registrations>... {
    template <typename Registration>
    using closure_holder = binding_closure_slot<
        static_activation_closure_t<RuntimeDependencies, Registrations...>,
        Registration>;

  public:
    static constexpr bool runtime_dependencies = RuntimeDependencies;

    using rtti_type = rtti<static_provider>;

    template <typename Registration> auto& get_closure() {
        return static_cast<closure_holder<Registration>&>(*this).closure;
    }

    template <typename T, typename BindingModel, typename Context,
              typename Source>
    decltype(auto) resolve_conversion(Context& context, Source&& source) {
        return detail::evaluate_static_conversion<T, BindingModel>(
            derived(), context, std::forward<Source>(source));
    }

    template <typename T, typename BindingModel, typename Host,
              typename Context>
    decltype(auto) resolve_binding_type(Host& host, Context& context) {
        return detail::evaluate_static_binding<T, BindingModel>(derived(), host,
                                                                context);
    }

    template <typename InterfaceBinding, typename Host>
    auto make_route(Host& host) {
        return detail::make_static_route<InterfaceBinding>(derived(), host);
    }

    template <typename T, typename Key, typename StaticRegistryType,
              typename Host, typename Fn, typename Context>
    std::size_t append_static_collection(T& results, Host& host,
                                         Context& context, Fn&& fn) {
        return detail::append_static_collection_impl<T, Key,
                                                     StaticRegistryType>(
            derived(), results, host, context, std::forward<Fn>(fn));
    }

    template <typename T, typename Key, typename StaticRegistryType,
              typename Host, typename Fn, typename Context>
    T construct_static_collection(Host& host, Context& context, Fn&& fn) {
        return detail::construct_static_collection_impl<T, Key,
                                                        StaticRegistryType>(
            derived(), host, context, std::forward<Fn>(fn));
    }

    template <typename T, typename Key, typename StaticRegistryType,
              typename Host, typename Context>
    T construct_static_collection(Host& host, Context& context) {
        return detail::construct_static_collection_default_impl<
            T, Key, StaticRegistryType>(derived(), host, context);
    }

  private:
    Derived& derived() { return static_cast<Derived&>(*this); }
};

template <bool RuntimeDependencies, typename... Registrations>
class basic_static_activation_set
    : public basic_static_activation_set_base<
          basic_static_activation_set<RuntimeDependencies, Registrations...>,
          RuntimeDependencies, Registrations...>,
      private static_binding_storage<Registrations...> {
  public:
    using static_binding_storage<
        Registrations...>::get_conversion_cache_for_model;
    using static_binding_storage<Registrations...>::get_storage;
    using static_binding_storage<Registrations...>::get_storage_for_model;
};

template <typename... Registrations>
using static_binding_scope =
    basic_static_activation_set<false, Registrations...>;

template <typename... Registrations>
using binding_scope = basic_static_activation_set<true, Registrations...>;

template <typename... Registrations>
using static_storage_state = static_binding_storage<Registrations...>;

template <bool RuntimeDependencies, typename StorageState,
          typename... Registrations>
class basic_static_activation_set_ref
    : public basic_static_activation_set_base<
          basic_static_activation_set_ref<RuntimeDependencies, StorageState,
                                          Registrations...>,
          RuntimeDependencies, Registrations...> {
  public:
    explicit basic_static_activation_set_ref(StorageState& state)
        : state_(&state) {}

    template <typename Registration> auto& get_storage() {
        return state_->template get_storage<Registration>();
    }

    template <typename BindingModel> auto& get_storage_for_model() {
        return state_->template get_storage_for_model<BindingModel>();
    }

    template <typename BindingModel> auto& get_conversion_cache_for_model() {
        return state_->template get_conversion_cache_for_model<BindingModel>();
    }

  private:
    StorageState* state_;
};

template <typename StorageState, typename... Registrations>
using static_binding_scope_ref =
    basic_static_activation_set_ref<false, StorageState, Registrations...>;

template <typename StorageState, typename... Registrations>
using binding_scope_ref =
    basic_static_activation_set_ref<true, StorageState, Registrations...>;

} // namespace detail
} // namespace dingo

#ifdef _MSC_VER
#pragma warning(pop)
#endif
