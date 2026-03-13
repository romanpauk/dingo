//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/bindings.h>

#include <dingo/class_instance_conversions.h>
#include <dingo/exceptions.h>
#include <dingo/factory/constructor.h>
#include <dingo/interface_storage_traits.h>
#include <dingo/storage/external.h>
#include <dingo/storage/shared.h>
#include <dingo/storage/shared_cyclical.h>
#include <dingo/storage/unique.h>

#include <functional>
#include <memory>
#include <optional>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace dingo {
namespace detail {

class bindings_resolving_context {
  public:
    ~bindings_resolving_context() {
        for (auto it = destructibles_.rbegin(); it != destructibles_.rend(); ++it)
            it->destroy(it->instance);
    }

    template <typename T, typename Container>
    decltype(auto) resolve(Container& container) {
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

template <typename T> struct is_reference_wrapper : std::false_type {};
template <typename T>
struct is_reference_wrapper<std::reference_wrapper<T>> : std::true_type {};

template <typename T>
static constexpr bool is_reference_wrapper_v = is_reference_wrapper<T>::value;

template <typename T> struct is_annotated_request : std::false_type {};
template <typename T, typename Tag>
struct is_annotated_request<annotated<T, Tag>> : std::true_type {};

template <typename T>
static constexpr bool is_annotated_request_v = is_annotated_request<T>::value;

template <typename Binding>
using binding_primary_interface_t = std::tuple_element_t<
    0, typename Binding::registration_type::interface_type::type_tuple>;

template <typename Component, typename Bind>
using bound_binding_t = find_binding_definition_t<
    binding_definitions_t<Component>, decay_t<typename Bind::request_type>,
    typename Bind::id_type>;

template <typename Binding, typename = void>
struct bound_targets_external : std::false_type {};

template <typename Binding>
struct bound_targets_external<
    Binding, std::void_t<typename Binding::scope>>
    : std::bool_constant<std::is_same_v<typename Binding::scope, external>> {};

template <typename Component, typename Bind>
struct bound_valid
    : std::bool_constant<
          bound_targets_external<
              bound_binding_t<Component, Bind>>::value> {};

template <typename Binding, typename Component, typename Bind>
struct bound_matches_binding
    : std::bool_constant<
          bound_valid<Component, Bind>::value &&
          std::is_same_v<Binding, bound_binding_t<Component, Bind>>> {};

template <typename Binding, typename Component, typename... Binds>
struct binding_bound_count
    : std::integral_constant<size_t,
                             (0u + ... +
                              (bound_matches_binding<
                                   Binding, Component, Binds>::value
                                   ? 1u
                                   : 0u))> {};

template <typename Bindings> struct external_binding_count_for_bindings;

template <typename... Bindings>
struct external_binding_count_for_bindings<type_list<Bindings...>>
    : std::integral_constant<
          size_t,
          (0u + ... +
           (std::is_same_v<typename Bindings::scope, external> ? 1u : 0u))> {};

template <typename Component, typename Bindings, typename... Binds>
struct external_bindings_satisfied;

template <typename Component, typename... Bindings, typename... Binds>
struct external_bindings_satisfied<Component, type_list<Bindings...>,
                                             Binds...>
    : std::bool_constant<
          (((!std::is_same_v<typename Bindings::scope, external>) ||
           binding_bound_count<Bindings, Component, Binds...>::value ==
               1) && ...)> {};

template <typename Component, typename Bindings, typename... Binds>
struct bind_constructor_validation;

template <typename Component, typename... Bindings, typename... Binds>
struct bind_constructor_validation<Component, type_list<Bindings...>,
                                             Binds...> : std::true_type {
    static_assert((is_bound_value_v<Binds> && ...),
                  "bindings_container constructor expects dingo::bind(...) arguments");
    static_assert((bound_valid<Component, Binds>::value && ...),
                  "bind does not match an external bindings entry");
    static_assert(
        external_bindings_satisfied<Component, type_list<Bindings...>,
                                             Binds...>::value,
        "external bindings entrys must be supplied exactly once");
};

template <typename Factory, typename ArgumentTypes> struct direct_factory_adapter;

template <typename Factory, typename... Args>
struct direct_factory_adapter<Factory, std::tuple<Args...>> : Factory {
    using direct_argument_types = std::tuple<Args...>;
};

template <typename Binding, typename Component,
          typename ArgumentTypes = binding_factory_adapter_argument_types_t<
              Binding, Component>>
struct binding_factory_adapter {
    using type = typename Binding::factory;
};

template <typename Binding, typename Component, typename... Args>
struct binding_factory_adapter<Binding, Component, std::tuple<Args...>> {
    using type =
        direct_factory_adapter<typename Binding::factory, std::tuple<Args...>>;
};

template <typename Binding, typename Component>
using binding_factory_adapter_t =
    typename binding_factory_adapter<Binding, Component>::type;

template <typename Binding>
using binding_stored_type_t = rebind_type_t<
    typename Binding::storage,
    std::conditional_t<
        std::tuple_size_v<typename Binding::registration_type::interface_type::type_tuple> ==
                1 &&
            std::has_virtual_destructor_v<
                binding_primary_interface_t<Binding>> &&
            is_interface_storage_rebindable_v<typename Binding::storage>,
        binding_primary_interface_t<Binding>,
        decay_t<typename Binding::storage>>>;

template <typename Binding, typename Component>
using binding_storage_t = detail::storage<
    typename Binding::scope, typename Binding::storage,
    binding_stored_type_t<Binding>,
    binding_factory_adapter_t<Binding, Component>,
    typename Binding::conversions>;

template <typename Interface, typename Storage> struct interface_state {
    class_instance_conversions<
        rebind_type_t<typename Storage::conversions::conversion_types, Interface>>
        conversions;
};

template <typename Target, typename Source>
decltype(auto) object_reference(Source&& source) {
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
Target* object_pointer(Source&& source) {
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
decltype(auto) cached_shared_ptr(Source& source,
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
decltype(auto) convert_binding_result(Source&& source, Conversions& conversions) {
    if constexpr (is_annotated_request_v<Request>) {
        using raw_request = typename annotated_traits<Request>::type;
        return Request(
            convert_binding_result<raw_request>(std::forward<Source>(source),
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
                        cached_shared_ptr<target>(source, conversions);
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
                return &cached_shared_ptr<element_type>(source,
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
                return object_pointer<target>(std::forward<Source>(source));
            }
        } else if constexpr (std::is_lvalue_reference_v<raw_request>) {
            return object_reference<request_base>(
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
                return object_reference<request_base>(
                    std::forward<Source>(source));
            } else {
                return request_base(std::forward<Source>(source));
            }
        }
    }
}

template <typename Binding, typename Owner, typename... Interfaces>
class binding_state_impl
    : public interface_state<
          typename annotated_traits<Interfaces>::type,
          binding_storage_t<Binding, typename Owner::bindings_type>>... {
    using storage_type =
        binding_storage_t<Binding, typename Owner::bindings_type>;

    template <typename Interface>
    using interface_state_type = interface_state<
        typename annotated_traits<Interface>::type, storage_type>;

  public:
    binding_state_impl() = default;

    template <typename Bind,
              typename = std::enable_if_t<
                  std::is_same_v<typename Binding::scope, external> &&
                  is_bound_value_v<std::decay_t<Bind>>>>
    explicit binding_state_impl(Bind&& bind)
        : storage_(std::forward<Bind>(bind).get()) {}

    template <typename Request, typename Context, typename OwnerArg>
    decltype(auto) resolve(Context& context, OwnerArg& owner) {
        using resolved_type = typename request_traits<Request>::request_type;
        using interface_key = decay_t<resolved_type>;

        auto& state = static_cast<interface_state_type<interface_key>&>(*this);
        return convert_binding_result<resolved_type>(storage_.resolve(context, owner),
                                               state.conversions);
    }

  private:
    storage_type storage_;
};

template <typename Binding, typename Owner, typename Interfaces>
struct binding_state_builder;

template <typename Binding, typename Owner, typename... Interfaces>
struct binding_state_builder<Binding, Owner, type_list<Interfaces...>> {
    using type = binding_state_impl<Binding, Owner, Interfaces...>;
};

template <typename Binding, typename Owner>
using binding_state_t =
    typename binding_state_builder<Binding, Owner,
                                             typename Binding::interface_list>::type;

template <typename Component, typename Bindings> class bindings_container_impl;

template <typename Component, typename... Bindings>
class bindings_container_impl<Component, type_list<Bindings...>> {
  public:
    using bindings_type = Component;

  private:
    using self_type = bindings_container_impl<Component, type_list<Bindings...>>;
    static constexpr size_t external_binding_count =
        external_binding_count_for_bindings<type_list<Bindings...>>::value;

    template <typename Binding>
    using binding_state_type = binding_state_t<Binding, self_type>;

    template <typename Request>
    using resolved_type_t = typename request_traits<Request>::request_type;

    template <typename Request>
    using top_level_resolved_type_t =
        std::conditional_t<std::is_rvalue_reference_v<resolved_type_t<Request>>,
                           std::remove_reference_t<resolved_type_t<Request>>,
                           resolved_type_t<Request>>;

    template <typename Request, typename... Binds>
    using provided_bindings_t = type_list<std::decay_t<Binds>...>;

    template <typename Request, typename... Binds>
    static constexpr bool root_binding_resolvable_v =
        bindings_resolvable_impl<Component, resolved_type_t<Request>, type_list<>,
                                  provided_bindings_t<Request, Binds...>,
                                  typename request_traits<Request>::id_type>::value;

    template <typename Request, typename... Binds>
    using root_open_argument_types_t =
        typename open_root_constructible<
            Component, Request, provided_bindings_t<Request, Binds...>>::type;

  public:
    template <typename Dummy = void,
              typename = std::enable_if_t<external_binding_count == 0, Dummy>>
    bindings_container_impl() {}

    template <typename... Binds,
              typename = std::enable_if_t<(sizeof...(Binds) != 0)>,
              typename = std::enable_if_t<
                  bind_constructor_validation<Component,
                                                        type_list<Bindings...>,
                                                        std::decay_t<Binds>...>::value>>
    explicit bindings_container_impl(Binds&&... binds)
        : states_(make_state<Bindings>(std::forward<Binds>(binds)...)...) {}

    template <typename Request, typename... Binds>
    top_level_resolved_type_t<Request> resolve(Binds&&... binds) {
        static_assert(
            detail::root_constructible<
                Component, Request, provided_bindings_t<Request, Binds...>>::value,
            "bindings cannot resolve the requested type");
        bindings_resolving_context context;
        return resolve_root<Request>(context, std::forward<Binds>(binds)...);
    }

    template <typename T, typename... Binds>
    T construct(Binds&&... binds) {
        return resolve<T>(std::forward<Binds>(binds)...);
    }

    template <typename Request,
              typename R = std::conditional_t<
                  std::is_rvalue_reference_v<resolved_type_t<Request>>,
                  std::remove_reference_t<resolved_type_t<Request>>,
                  resolved_type_t<Request>>>
    R resolve(bindings_resolving_context& context) {
        static_assert(bindings_resolvable_v<Component, resolved_type_t<Request>,
                                             typename request_traits<
                                                 Request>::id_type>,
                      "bindings cannot resolve the requested type");
        return resolve_dependency<Request>(context);
    }

  private:
    template <typename Request, typename... Binds>
    class bound_root_container {
      public:
        using bindings_type = Component;

        explicit bound_root_container(self_type& owner, Binds&... binds)
            : owner_(owner), binds_(binds...) {}

        template <typename T>
        decltype(auto) resolve(bindings_resolving_context& context) {
            return resolve_dependency<T>(context);
        }

        template <typename RequestT>
        top_level_resolved_type_t<RequestT> resolve_dependency(
            bindings_resolving_context& context) {
            using interface_key = decay_t<resolved_type_t<RequestT>>;
            using id_type = typename request_traits<RequestT>::id_type;
            using binding = find_binding_definition_t<type_list<Bindings...>,
                                                     interface_key, id_type>;

            if constexpr (!std::is_same_v<binding, no_binding_id> &&
                          !std::is_same_v<binding, ambiguous_binding>) {
                return owner_.template state<binding>().template resolve<RequestT>(
                    context, *this);
            } else {
                return std::apply(
                    [&](auto&... binds) -> decltype(auto) {
                        using bind_type = find_bound_value_t<
                            type_list<std::decay_t<Binds>...>, interface_key,
                            id_type>;
                        static_assert(!std::is_same_v<bind_type, no_binding_id>,
                                      "binding not found");
                        return bound_value<RequestT>(
                            find_bind<bind_type>(binds...));
                    },
                    binds_);
            }
        }

        template <typename Binding>
        binding_state_type<Binding>& state() {
            return owner_.template state<Binding>();
        }

      private:
        self_type& owner_;
        std::tuple<Binds&...> binds_;
    };

    template <typename Request, typename Bind>
    static top_level_resolved_type_t<Request> bound_value(Bind& bind) {
        if constexpr (is_annotated_request_v<resolved_type_t<Request>>) {
            return resolved_type_t<Request>(bind.get());
        } else {
            return static_cast<top_level_resolved_type_t<Request>>(bind.get());
        }
    }

    template <typename BindType, typename FirstBind, typename... RestBinds>
    static BindType& find_bind(FirstBind& first_bind, RestBinds&... rest_binds) {
        if constexpr (std::is_same_v<BindType, std::decay_t<FirstBind>>) {
            return first_bind;
        } else {
            static_assert(sizeof...(RestBinds) != 0, "missing bound value");
            return find_bind<BindType>(rest_binds...);
        }
    }

    template <typename Request>
    top_level_resolved_type_t<Request> resolve_dependency(
        bindings_resolving_context& context) {
        using interface_key = decay_t<resolved_type_t<Request>>;
        using id_type = typename request_traits<Request>::id_type;
        using binding =
            find_binding_definition_t<type_list<Bindings...>, interface_key, id_type>;

        static_assert(!std::is_same_v<binding, no_binding_id>,
                      "binding not found");
        static_assert(!std::is_same_v<binding, ambiguous_binding>,
                      "binding is ambiguous");
        return state<binding>().template resolve<Request>(context, *this);
    }

    template <typename Request, typename... Binds>
    top_level_resolved_type_t<Request> resolve_root(bindings_resolving_context& context,
                                                    Binds&&... binds) {
        bound_root_container<Request, std::decay_t<Binds>...> root_container(
            *this, binds...);

        if constexpr (root_binding_resolvable_v<Request, Binds...>) {
            return root_container.template resolve_dependency<Request>(context);
        } else {
            using request_type = resolved_type_t<Request>;
            using factory_type = direct_factory_adapter<
                ::dingo::constructor_detection<decay_t<request_type>>,
                root_open_argument_types_t<Request, Binds...>>;

            factory_type factory;
            return detail::construct_factory<request_type>(context, root_container,
                                                           factory);
        }
    }

    template <typename Binding, typename FirstBind, typename... RestBinds>
    static decltype(auto) external_bind(FirstBind&& first_bind,
                                        RestBinds&&... rest_binds) {
        if constexpr (bound_matches_binding<
                          Binding, Component,
                          std::decay_t<FirstBind>>::value) {
            return std::forward<FirstBind>(first_bind);
        } else {
            static_assert(sizeof...(RestBinds) != 0,
                          "missing external bindings entry");
            return external_bind<Binding>(std::forward<RestBinds>(rest_binds)...);
        }
    }

    template <typename Binding, typename... Binds>
    static binding_state_type<Binding> make_state(Binds&&... binds) {
        if constexpr (std::is_same_v<typename Binding::scope, external>) {
            return binding_state_type<Binding>(
                external_bind<Binding>(std::forward<Binds>(binds)...));
        } else {
            return binding_state_type<Binding>();
        }
    }

    template <typename Binding> binding_state_type<Binding>& state() {
        return std::get<binding_state_type<Binding>>(states_);
    }

    std::tuple<binding_state_type<Bindings>...> states_;
};

} // namespace detail

template <typename Component>
class bindings_container
    : public detail::bindings_container_impl<Component,
                                              detail::binding_definitions_t<Component>> {
    using base_type = detail::bindings_container_impl<
        Component, detail::binding_definitions_t<Component>>;

  public:
    using base_type::base_type;
    using base_type::construct;
    using base_type::resolve;
};

namespace detail {
template <typename Bindings> struct bindings_has_external_bindings;

template <typename... Entries>
struct bindings_has_external_bindings<bindings<Entries...>>
    : std::bool_constant<
          external_binding_count_for_bindings<binding_definitions_t<
              bindings<Entries...>>>::value != 0> {};
} // namespace detail

template <typename... Entries, typename Root>
struct factory<bindings<Entries...>, Root> {
    using bindings_type = bindings<Entries...>;
    using root_type = Root;
    using self_type = factory<bindings_type, root_type>;

    template <typename... Binds>
    auto operator()(Binds&&... binds) const {
        if constexpr (detail::bindings_has_external_bindings<bindings_type>::value) {
            bindings_container<bindings_type> container(
                std::forward<Binds>(binds)...);
            return container.template resolve<root_type>();
        } else {
            bindings_container<bindings_type> container;
            return container.template resolve<root_type>(
                std::forward<Binds>(binds)...);
        }
    }

    template <typename Type, typename Context, typename Container>
    Type construct(Context&, Container& container) const {
        static_assert(
            !detail::bindings_has_external_bindings<bindings_type>::value,
            "register_factory does not support external bindings; bind them directly with factory::operator()");

        using child_container_type =
            typename Container::template child_container_type<self_type>;
        child_container_type child(&container, container.get_allocator());
        install<bindings_type>(child);
        return child.template resolve<Type>();
    }
};

} // namespace dingo
