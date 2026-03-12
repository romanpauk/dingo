//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/component.h>

#include <dingo/class_instance_conversions.h>
#include <dingo/exceptions.h>
#include <dingo/factory/constructor.h>
#include <dingo/interface_storage_traits.h>
#include <dingo/storage/shared.h>
#include <dingo/storage/shared_cyclical.h>
#include <dingo/storage/unique.h>

#include <memory>
#include <optional>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace dingo {
namespace detail {

class component_resolving_context {
  public:
    ~component_resolving_context() {
        for (auto it = destructibles_.rbegin(); it != destructibles_.rend(); ++it)
            it->destroy(it->instance);
    }

    template <typename T, typename Container> T resolve(Container& container) {
        return container.template resolve<T>(*this);
    }

    template <typename T, typename... Args> T& construct(Args&&... args) {
        T* instance = new T(std::forward<Args>(args)...);
        destructibles_.push_back({instance, &destroy<T>});
        return *instance;
    }

  private:
    struct destructible {
        void* instance;
        void (*destroy)(void*);
    };

    template <typename T> static void destroy(void* ptr) {
        delete reinterpret_cast<T*>(ptr);
    }

    std::vector<destructible> destructibles_;
};

template <typename T> struct is_shared_ptr : std::false_type {};
template <typename T>
struct is_shared_ptr<std::shared_ptr<T>> : std::true_type {};

template <typename T>
static constexpr bool is_shared_ptr_v = is_shared_ptr<T>::value;

template <typename T> struct is_unique_ptr : std::false_type {};
template <typename T, typename Deleter>
struct is_unique_ptr<std::unique_ptr<T, Deleter>> : std::true_type {};

template <typename T>
static constexpr bool is_unique_ptr_v = is_unique_ptr<T>::value;

template <typename T> struct is_optional : std::false_type {};
template <typename T>
struct is_optional<std::optional<T>> : std::true_type {};

template <typename T>
static constexpr bool is_optional_v = is_optional<T>::value;

template <typename T> struct is_annotated_request : std::false_type {};
template <typename T, typename Tag>
struct is_annotated_request<annotated<T, Tag>> : std::true_type {};

template <typename T>
static constexpr bool is_annotated_request_v = is_annotated_request<T>::value;

template <typename Binding>
using component_binding_primary_interface_t = std::tuple_element_t<
    0, typename Binding::registration_type::interface_type::type_tuple>;

template <typename Factory, typename ArgumentTypes> struct component_direct_factory;

template <typename Factory, typename... Args>
struct component_direct_factory<Factory, std::tuple<Args...>> : Factory {
    using direct_argument_types = std::tuple<Args...>;
};

template <typename Binding, typename Component,
          typename ArgumentTypes = component_binding_factory_argument_types_t<
              Binding, Component>>
struct component_binding_factory {
    using type = typename Binding::factory;
};

template <typename Binding, typename Component, typename... Args>
struct component_binding_factory<Binding, Component, std::tuple<Args...>> {
    using type =
        component_direct_factory<typename Binding::factory, std::tuple<Args...>>;
};

template <typename Binding, typename Component>
using component_binding_factory_t =
    typename component_binding_factory<Binding, Component>::type;

template <typename Binding>
using component_binding_stored_type_t = rebind_type_t<
    typename Binding::storage,
    std::conditional_t<
        std::tuple_size_v<typename Binding::registration_type::interface_type::type_tuple> ==
                1 &&
            std::has_virtual_destructor_v<
                component_binding_primary_interface_t<Binding>> &&
            is_interface_storage_rebindable_v<typename Binding::storage>,
        component_binding_primary_interface_t<Binding>,
        decay_t<typename Binding::storage>>>;

template <typename Binding, typename Component>
using component_binding_storage_t = detail::storage<
    typename Binding::scope, typename Binding::storage,
    component_binding_stored_type_t<Binding>,
    component_binding_factory_t<Binding, Component>,
    typename Binding::conversions>;

template <typename Interface, typename Storage> struct component_interface_state {
    class_instance_conversions<
        rebind_type_t<typename Storage::conversions::conversion_types, Interface>>
        conversions;
};

template <typename Target, typename Source>
decltype(auto) component_object_reference(Source&& source) {
    using source_type = std::remove_reference_t<Source>;

    if constexpr (std::is_pointer_v<source_type>) {
        return static_cast<Target&>(*source);
    } else if constexpr (is_shared_ptr_v<source_type> ||
                         is_unique_ptr_v<source_type>) {
        return static_cast<Target&>(*source);
    } else if constexpr (is_optional_v<source_type>) {
        return static_cast<Target&>(source.value());
    } else {
        return static_cast<Target&>(source);
    }
}

template <typename Target, typename Source>
Target* component_object_pointer(Source&& source) {
    using source_type = std::remove_reference_t<Source>;

    if constexpr (std::is_pointer_v<source_type>) {
        return static_cast<Target*>(source);
    } else if constexpr (is_shared_ptr_v<source_type> ||
                         is_unique_ptr_v<source_type>) {
        return static_cast<Target*>(source.get());
    } else if constexpr (is_optional_v<source_type>) {
        return &static_cast<Target&>(source.value());
    } else {
        return &static_cast<Target&>(source);
    }
}

template <typename Target, typename Source, typename Conversions>
decltype(auto) component_cached_shared_ptr(Source& source,
                                           Conversions& conversions) {
    using source_type = std::remove_reference_t<Source>;
    using source_target = typename source_type::element_type;

    if constexpr (std::is_same_v<Target, source_target>) {
        return source;
    } else {
        return conversions.template construct<std::shared_ptr<Target>>(source);
    }
}

template <typename Request, typename Source, typename Conversions>
decltype(auto) component_convert(Source&& source, Conversions& conversions) {
    if constexpr (is_annotated_request_v<Request>) {
        using raw_request = typename annotated_traits<Request>::type;
        return Request(
            component_convert<raw_request>(std::forward<Source>(source),
                                           conversions));
    } else {
        using raw_request = typename annotated_traits<Request>::type;
        using request_base =
            std::remove_const_t<std::remove_reference_t<raw_request>>;
        using source_type = std::remove_reference_t<Source>;

        if constexpr (is_shared_ptr_v<request_base>) {
            using target = typename request_base::element_type;

            if constexpr (is_shared_ptr_v<source_type>) {
                if constexpr (std::is_lvalue_reference_v<Source>) {
                    auto& converted =
                        component_cached_shared_ptr<target>(source, conversions);
                    if constexpr (std::is_lvalue_reference_v<raw_request>) {
                        return converted;
                    } else {
                        return std::shared_ptr<target>(converted);
                    }
                } else {
                    return std::shared_ptr<target>(std::forward<Source>(source));
                }
            } else if constexpr (std::is_pointer_v<source_type>) {
                return std::shared_ptr<target>(static_cast<target*>(source));
            } else {
                static_assert(dependent_false<Request, Source>::value,
                              "unsupported shared_ptr conversion");
            }
        } else if constexpr (is_unique_ptr_v<request_base>) {
            using target = typename request_base::element_type;

            if constexpr (is_unique_ptr_v<source_type>) {
                using source_target = typename source_type::element_type;
                if constexpr (std::is_lvalue_reference_v<raw_request>) {
                    if constexpr (std::is_same_v<target, source_target>) {
                        return source;
                    } else {
                        throw type_not_convertible_exception();
                    }
                } else {
                    return std::unique_ptr<target>(std::forward<Source>(source));
                }
            } else if constexpr (std::is_pointer_v<source_type>) {
                return std::unique_ptr<target>(static_cast<target*>(source));
            } else {
                static_assert(dependent_false<Request, Source>::value,
                              "unsupported unique_ptr conversion");
            }
        } else if constexpr (is_optional_v<request_base>) {
            using target = typename request_base::value_type;

            if constexpr (is_optional_v<source_type>) {
                using source_target = typename source_type::value_type;
                if constexpr (std::is_lvalue_reference_v<raw_request>) {
                    if constexpr (std::is_same_v<target, source_target>) {
                        return source;
                    } else {
                        throw type_not_convertible_exception();
                    }
                } else {
                    if constexpr (std::is_same_v<target, source_target>) {
                        return std::optional<target>(source);
                    } else {
                        throw type_not_convertible_exception();
                    }
                }
            } else {
                return std::optional<target>(std::forward<Source>(source));
            }
        } else if constexpr (std::is_pointer_v<raw_request>) {
            using target =
                std::remove_const_t<std::remove_pointer_t<raw_request>>;

            if constexpr (is_shared_ptr_v<target>) {
                using element_type = typename target::element_type;
                return &component_cached_shared_ptr<element_type>(source,
                                                                 conversions);
            } else if constexpr (is_unique_ptr_v<target> ||
                                 is_optional_v<target>) {
                using source_target = std::remove_reference_t<Source>;
                if constexpr (std::is_same_v<target, source_target>) {
                    return &source;
                } else {
                    throw type_not_convertible_exception();
                }
            } else {
                return component_object_pointer<target>(std::forward<Source>(source));
            }
        } else if constexpr (std::is_lvalue_reference_v<raw_request>) {
            return component_object_reference<request_base>(
                std::forward<Source>(source));
        } else if constexpr (std::is_rvalue_reference_v<raw_request>) {
            if constexpr (std::is_reference_v<Source>) {
                return std::move(source);
            } else {
                return request_base(std::forward<Source>(source));
            }
        } else {
            if constexpr (std::is_pointer_v<source_type> ||
                          is_shared_ptr_v<source_type> ||
                          is_unique_ptr_v<source_type> ||
                          is_optional_v<source_type>) {
                return component_object_reference<request_base>(
                    std::forward<Source>(source));
            } else {
                return request_base(std::forward<Source>(source));
            }
        }
    }
}

template <typename Binding, typename Owner, typename... Interfaces>
class component_binding_state_impl
    : public component_interface_state<
          typename annotated_traits<Interfaces>::type,
          component_binding_storage_t<Binding, typename Owner::component_type>>... {
    using storage_type =
        component_binding_storage_t<Binding, typename Owner::component_type>;

    template <typename Interface>
    using interface_state_type = component_interface_state<
        typename annotated_traits<Interface>::type, storage_type>;

  public:
    component_binding_state_impl() {
        static_assert(!std::is_same_v<typename Binding::scope, external>,
                      "component_container does not support external scope");
    }

    template <typename Request, typename Context>
    decltype(auto) resolve(Context& context, Owner& owner) {
        using resolved_type = typename component_request_traits<Request>::request_type;
        using interface_key = decay_t<resolved_type>;

        auto& state = static_cast<interface_state_type<interface_key>&>(*this);
        return component_convert<resolved_type>(storage_.resolve(context, owner),
                                               state.conversions);
    }

  private:
    storage_type storage_;
};

template <typename Binding, typename Owner, typename Interfaces>
struct component_binding_state_builder;

template <typename Binding, typename Owner, typename... Interfaces>
struct component_binding_state_builder<Binding, Owner, type_list<Interfaces...>> {
    using type = component_binding_state_impl<Binding, Owner, Interfaces...>;
};

template <typename Binding, typename Owner>
using component_binding_state_t =
    typename component_binding_state_builder<Binding, Owner,
                                             typename Binding::interface_list>::type;

template <typename Component, typename Bindings> class component_container_impl;

template <typename Component, typename... Bindings>
class component_container_impl<Component, type_list<Bindings...>> {
  public:
    using component_type = Component;

  private:
    using self_type = component_container_impl<Component, type_list<Bindings...>>;

    template <typename Binding>
    using binding_state_type = component_binding_state_t<Binding, self_type>;

    template <typename Request>
    using resolved_type_t = typename component_request_traits<Request>::request_type;

    template <typename Request>
    using top_level_resolved_type_t =
        std::conditional_t<std::is_rvalue_reference_v<resolved_type_t<Request>>,
                           std::remove_reference_t<resolved_type_t<Request>>,
                           resolved_type_t<Request>>;

  public:
    template <typename Request> top_level_resolved_type_t<Request> resolve() {
        static_assert(component_constructible_v<Component, Request>,
                      "component cannot resolve the requested type");
        component_resolving_context context;
        return resolve<Request>(context);
    }

    template <typename T, typename Factory = ::dingo::constructor_detection<decay_t<T>>>
    T construct(Factory factory = Factory()) {
        static_assert(factory.valid, "construction not detected");
        component_resolving_context context;
        return detail::construct_factory<T>(context, *this, factory);
    }

    template <typename Request,
              typename R = std::conditional_t<
                  std::is_rvalue_reference_v<resolved_type_t<Request>>,
                  std::remove_reference_t<resolved_type_t<Request>>,
                  resolved_type_t<Request>>>
    R resolve(component_resolving_context& context) {
        static_assert(component_constructible_v<Component, Request>,
                      "component cannot resolve the requested type");
        using interface_key = decay_t<resolved_type_t<Request>>;
        using id_type = typename component_request_traits<Request>::id_type;
        using binding =
            find_component_binding_t<type_list<Bindings...>, interface_key, id_type>;
        static_assert(!std::is_same_v<binding, component_no_id>,
                      "binding not found");
        static_assert(!std::is_same_v<binding, component_ambiguous>,
                      "binding is ambiguous");
        return state<binding>().template resolve<Request>(context, *this);
    }

  private:
    template <typename Binding> binding_state_type<Binding>& state() {
        return std::get<binding_state_type<Binding>>(states_);
    }

    std::tuple<binding_state_type<Bindings>...> states_;
};

} // namespace detail

template <typename Component>
class component_container
    : public detail::component_container_impl<Component,
                                              detail::component_bindings_t<Component>> {
    using base_type = detail::component_container_impl<
        Component, detail::component_bindings_t<Component>>;

  public:
    using base_type::base_type;
    using base_type::construct;
    using base_type::resolve;
};

} // namespace dingo
