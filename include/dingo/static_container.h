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
#include <dingo/resolving_context.h>
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

namespace dingo {
namespace detail {

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

struct unscoped_resolving_context {
    template <typename T, typename Container>
    decltype(auto) resolve(Container& container) {
        return container.template resolve_dependency_unscoped<T>();
    }
};

template <typename Binding>
using binding_primary_interface_t = std::tuple_element_t<
    0, typename Binding::registration_type::interface_type::type_tuple>;

template <typename Request>
using unscoped_request_type_t =
    typename annotated_traits<typename request_traits<Request>::request_type>::type;

template <typename Request>
struct request_consumes_temporary
    : std::bool_constant<!std::is_lvalue_reference_v<unscoped_request_type_t<Request>> &&
                         !std::is_pointer_v<unscoped_request_type_t<Request>>> {
};

template <typename Request>
static constexpr bool request_consumes_temporary_v =
    request_consumes_temporary<Request>::value;

template <typename Binding, typename Request>
struct binding_supports_unscoped_request
    : std::bool_constant<
          std::is_same_v<typename Binding::scope, external> ||
          std::is_same_v<typename Binding::scope, shared> ||
          (std::is_same_v<typename Binding::scope, unique> &&
           request_consumes_temporary_v<Request>)> {};

template <typename Tuple, typename StaticBindings, typename SeenBindings>
struct argument_tuple_unscoped_resolvable;

template <typename StaticBindings, typename SeenBindings>
struct argument_tuple_unscoped_resolvable<std::tuple<>, StaticBindings,
                                          SeenBindings> : std::true_type {};

template <typename StaticBindings, typename SeenBindings, typename... Requests>
struct argument_tuple_unscoped_resolvable<std::tuple<Requests...>, StaticBindings,
                                          SeenBindings>
    : std::conjunction<
          argument_tuple_unscoped_resolvable<std::tuple<Requests>, StaticBindings,
                                             SeenBindings>...> {};

template <typename StaticBindings, typename SeenBindings>
struct argument_tuple_unscoped_resolvable<bindings_no_matching_arguments,
                                          StaticBindings, SeenBindings>
    : std::false_type {};

template <typename Request, typename StaticBindings, typename SeenBindings>
struct request_unscoped_resolvable;

template <typename StaticBindings, typename SeenBindings, typename Request>
struct argument_tuple_unscoped_resolvable<std::tuple<Request>, StaticBindings,
                                          SeenBindings>
    : request_unscoped_resolvable<StaticBindings, Request, SeenBindings> {};

template <typename Binding, typename StaticBindings, typename SeenBindings>
struct binding_unscoped_constructible
    : argument_tuple_unscoped_resolvable<
          binding_factory_adapter_argument_types_t<Binding, StaticBindings,
                                                  SeenBindings>,
          StaticBindings, SeenBindings> {};

template <typename StaticBindings, typename Request, typename SeenBindings,
          typename Binding>
struct request_unscoped_resolvable_binding
    : std::bool_constant<
          binding_supports_request<Binding, request_runtime_type_t<
                                                typename request_traits<Request>::request_type>>::value &&
          binding_supports_unscoped_request<Binding, Request>::value &&
          !type_list_contains_v<SeenBindings, Binding> &&
          binding_unscoped_constructible<
              Binding, StaticBindings,
              type_list_push_unique_t<SeenBindings, Binding>>::value> {};

template <typename StaticBindings, typename Request, typename SeenBindings>
struct request_unscoped_resolvable_binding<StaticBindings, Request,
                                           SeenBindings, no_binding_id>
    : std::false_type {};

template <typename StaticBindings, typename Request, typename SeenBindings>
struct request_unscoped_resolvable_binding<StaticBindings, Request,
                                           SeenBindings, ambiguous_binding>
    : std::false_type {};

template <typename StaticBindings, typename Request, typename SeenBindings>
struct request_unscoped_resolvable
    : request_unscoped_resolvable_binding<
          StaticBindings, Request, SeenBindings,
          find_binding_definition_t<
              binding_definitions_t<StaticBindings>,
              decay_t<typename request_traits<Request>::request_type>,
              typename request_traits<Request>::id_type>> {};

template <typename StaticBindings, typename Request>
static constexpr bool request_unscoped_resolvable_v =
    request_unscoped_resolvable<StaticBindings, Request, type_list<>>::value;

template <typename StaticBindings, typename Bindings, typename... Binds>
struct bind_constructor_validation;

template <typename StaticBindings, typename... Bindings, typename... Binds>
struct bind_constructor_validation<StaticBindings, type_list<Bindings...>,
                                   Binds...>
    : bind_argument_validation<StaticBindings, type_list<Bindings...>,
                               Binds...> {};

template <typename Factory, typename ArgumentTypes> struct direct_factory_adapter;

template <typename Factory, typename... Args>
struct direct_factory_adapter<Factory, std::tuple<Args...>> : Factory {
    using direct_argument_types = std::tuple<Args...>;
};

template <typename Binding, typename StaticBindings,
          typename ArgumentTypes = binding_factory_adapter_argument_types_t<
              Binding, StaticBindings>>
struct binding_factory_adapter {
    using type = typename Binding::factory;
};

template <typename Binding, typename StaticBindings, typename... Args>
struct binding_factory_adapter<Binding, StaticBindings, std::tuple<Args...>> {
    using type = direct_factory_adapter<typename Binding::factory,
                                        std::tuple<Args...>>;
};

template <typename Binding, typename StaticBindings>
using binding_factory_adapter_t =
    typename binding_factory_adapter<Binding, StaticBindings>::type;

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

template <typename Scope, typename Type, typename StoredType, typename Factory,
          typename Conversions, typename = void>
class binding_state;

template <typename Type, typename StoredType, typename Factory,
          typename Conversions>
class binding_state<unique, Type, StoredType, Factory, Conversions>
    : private Factory {
  public:
    template <typename... Args>
    binding_state(Args&&... args) : Factory(std::forward<Args>(args)...) {}

    using conversions = Conversions;
    using type = Type;
    using stored_type = StoredType;
    using tag_type = unique;

    template <typename Context, typename Container>
    Type resolve(Context& context, Container& container) {
        return detail::construct_factory<Type>(
            context, container, static_cast<Factory&>(*this));
    }
};

template <typename Type, typename StoredType, typename Factory,
          typename Conversions, typename = void>
class external_binding_state_holder;

template <typename Type, typename StoredType, typename Factory,
          typename Conversions>
class external_binding_state_holder<Type, StoredType, Factory, Conversions,
                                    std::enable_if_t<!has_type_traits_v<Type>>> {
  public:
    template <typename T>
    explicit external_binding_state_holder(T&& instance)
        : instance_(std::forward<T>(instance)) {}

    Type& get() { return instance_; }

  private:
    Type instance_;
};

template <typename Type, typename StoredType, typename Factory,
          typename Conversions>
class external_binding_state_holder<Type, StoredType, Factory, Conversions,
                                    std::enable_if_t<has_type_traits_v<Type>>> {
  public:
    template <typename T>
    explicit external_binding_state_holder(T&& instance)
        : instance_(type_conversion_traits<StoredType, Type>::convert(
              std::forward<T>(instance))) {}

    StoredType& get() { return instance_; }

  private:
    StoredType instance_;
};

template <typename Type, typename StoredType, typename Factory,
          typename Conversions>
class binding_state<external, Type, StoredType, Factory, Conversions>
    : private external_binding_state_holder<Type, StoredType, Factory, Conversions> {
    using holder_type =
        external_binding_state_holder<Type, StoredType, Factory, Conversions>;

  public:
    template <typename T>
    explicit binding_state(T&& instance) : holder_type(std::forward<T>(instance)) {}

    using conversions = Conversions;
    using type = Type;
    using stored_type = StoredType;
    using tag_type = external;

    template <typename Context, typename Container>
    decltype(auto) resolve(Context&, Container&) {
        return holder_type::get();
    }

    decltype(auto) get_stable() { return holder_type::get(); }

    constexpr bool is_resolved() const { return true; }
};

template <typename Type, typename StoredType, typename Factory,
          typename Conversions>
class binding_state<external, Type&, StoredType, Factory, Conversions> {
  public:
    explicit binding_state(Type& instance) : instance_(instance) {}

    using conversions = Conversions;
    using type = Type&;
    using stored_type = StoredType;
    using tag_type = external;

    template <typename Context, typename Container>
    Type& resolve(Context&, Container&) {
        return instance_;
    }

    Type& get_stable() { return instance_; }

    constexpr bool is_resolved() const { return true; }

  private:
    Type& instance_;
};

template <typename Type, typename StoredType, typename Factory,
          typename Conversions>
class binding_state<external, Type*, StoredType, Factory, Conversions> {
  public:
    explicit binding_state(Type* instance) : instance_(instance) {}

    using conversions = Conversions;
    using type = Type*;
    using stored_type = StoredType;
    using tag_type = external;

    template <typename Context, typename Container>
    Type* resolve(Context&, Container&) {
        return instance_;
    }

    Type* get_stable() { return instance_; }

    constexpr bool is_resolved() const { return true; }

  private:
    Type* instance_;
};

template <typename Type, typename StoredType, typename Factory,
          typename Conversions>
class shared_binding_state_instance_base : private Factory {
  public:
    template <typename... Args>
    shared_binding_state_instance_base(Args&&... args)
        : Factory(std::forward<Args>(args)...) {}

    template <typename Context, typename Container>
    Type* resolve(Context& context, Container& container) {
        if (!initialized_) {
            detail::construct_factory<Type>(&instance_, context, container,
                                            static_cast<Factory&>(*this));
            initialized_ = true;
        }
        return get_stable();
    }

    Type* get_stable() {
        assert(initialized_);
        return reinterpret_cast<Type*>(&instance_);
    }

    bool is_resolved() const { return initialized_; }

  protected:
    Type* instance_ptr() { return reinterpret_cast<Type*>(&instance_); }

    void reset_storage() { initialized_ = false; }

    aligned_storage_t<sizeof(Type), alignof(Type)> instance_;
    bool initialized_ = false;
};

template <typename Type, typename StoredType, typename Factory,
          typename Conversions, bool = std::is_trivially_destructible_v<Type>>
class shared_binding_state_instance;

template <typename Type, typename StoredType, typename Factory,
          typename Conversions>
class shared_binding_state_instance<Type, StoredType, Factory, Conversions, true>
    : public shared_binding_state_instance_base<Type, StoredType, Factory,
                                                Conversions> {
    using base_type = shared_binding_state_instance_base<
        Type, StoredType, Factory, Conversions>;

  public:
    template <typename... Args>
    shared_binding_state_instance(Args&&... args)
        : base_type(std::forward<Args>(args)...) {}
};

template <typename Type, typename StoredType, typename Factory,
          typename Conversions>
class shared_binding_state_instance<Type, StoredType, Factory, Conversions, false>
    : public shared_binding_state_instance_base<Type, StoredType, Factory,
                                                Conversions> {
    using base_type = shared_binding_state_instance_base<
        Type, StoredType, Factory, Conversions>;

  public:
    template <typename... Args>
    shared_binding_state_instance(Args&&... args)
        : base_type(std::forward<Args>(args)...) {}

    ~shared_binding_state_instance() { reset(); }

  private:
    void reset() {
        if (this->initialized_) {
            this->instance_ptr()->~Type();
            this->reset_storage();
        }
    }
};

template <typename Type, typename StoredType, typename Factory,
          typename Conversions>
class binding_state<shared, Type, StoredType, Factory, Conversions,
                    std::enable_if_t<!has_type_traits_v<Type>>>
    : public shared_binding_state_instance<Type, StoredType, Factory,
                                           Conversions> {
    using instance_type =
        shared_binding_state_instance<Type, StoredType, Factory, Conversions>;

  public:
    template <typename... Args>
    binding_state(Args&&... args) : instance_type(std::forward<Args>(args)...) {}

    using conversions = Conversions;
    using type = Type;
    using stored_type = StoredType;
    using tag_type = shared;

    template <typename Context, typename Container>
    Type* resolve(Context& context, Container& container) {
        return instance_type::resolve(context, container);
    }

    Type* get_stable() { return instance_type::get_stable(); }

    bool is_resolved() const { return instance_type::is_resolved(); }
};

template <typename Type, typename StoredType, typename Factory,
          typename Conversions>
class binding_state<shared, Type, StoredType, Factory, Conversions,
                    std::enable_if_t<has_type_traits_v<Type>>> : private Factory {
  public:
    template <typename... Args>
    binding_state(Args&&... args) : Factory(std::forward<Args>(args)...) {}

    ~binding_state() { reset(); }

    using conversions = Conversions;
    using type = Type;
    using stored_type = StoredType;
    using tag_type = shared;

    template <typename Context, typename Container>
    StoredType& resolve(Context& context, Container& container) {
        if (!initialized_) {
            new (&instance_) StoredType(type_conversion_traits<
                                        StoredType, Type>::convert(
                detail::construct_factory<Type>(context, container,
                                                static_cast<Factory&>(*this))));
            initialized_ = true;
        }
        return get_stable();
    }

    StoredType& get_stable() {
        assert(initialized_);
        return *reinterpret_cast<StoredType*>(&instance_);
    }

    bool is_resolved() const { return initialized_; }

  private:
    StoredType* instance_ptr() {
        return reinterpret_cast<StoredType*>(&instance_);
    }

    void reset() {
        if (initialized_) {
            if constexpr (!std::is_trivially_destructible_v<StoredType>) {
                instance_ptr()->~StoredType();
            }
            initialized_ = false;
        }
    }

    aligned_storage_t<sizeof(StoredType), alignof(StoredType)> instance_;
    bool initialized_ = false;
};

template <typename Type, typename StoredType, typename Factory,
          typename Conversions>
class binding_state<shared, Type*, StoredType*, Factory, Conversions>
    : private Factory {
  public:
    template <typename... Args>
    binding_state(Args&&... args) : Factory(std::forward<Args>(args)...) {}

    ~binding_state() { reset(); }

    using conversions = Conversions;
    using type = Type*;
    using stored_type = StoredType*;
    using tag_type = shared;

    template <typename Context, typename Container>
    StoredType* resolve(Context& context, Container& container) {
        if (instance_ == nullptr) {
            instance_ = detail::construct_factory<Type*>(
                context, container, static_cast<Factory&>(*this));
        }
        return get_stable();
    }

    StoredType* get_stable() {
        assert(instance_ != nullptr);
        return type_conversion_traits<StoredType*, Type*>::convert(instance_);
    }

    bool is_resolved() const { return instance_ != nullptr; }

  private:
    void reset() {
        delete instance_;
        instance_ = nullptr;
    }

    Type* instance_ = nullptr;
};

template <typename Binding, typename StaticBindings, typename Scope>
struct binding_storage_selector {
    using type = detail::storage<typename Binding::scope, typename Binding::storage,
                                 binding_stored_type_t<Binding>,
                                 binding_factory_adapter_t<Binding, StaticBindings>,
                                 typename Binding::conversions>;
};

template <typename Binding, typename StaticBindings>
struct binding_storage_selector<Binding, StaticBindings, shared> {
    using type = binding_state<shared, typename Binding::storage,
                               binding_stored_type_t<Binding>,
                               binding_factory_adapter_t<Binding, StaticBindings>,
                               typename Binding::conversions>;
};

template <typename Binding, typename StaticBindings>
struct binding_storage_selector<Binding, StaticBindings, unique> {
    using type = binding_state<unique, typename Binding::storage,
                               binding_stored_type_t<Binding>,
                               binding_factory_adapter_t<Binding, StaticBindings>,
                               typename Binding::conversions>;
};

template <typename Binding, typename StaticBindings>
struct binding_storage_selector<Binding, StaticBindings, external> {
    using type = binding_state<external, typename Binding::storage,
                               binding_stored_type_t<Binding>,
                               binding_factory_adapter_t<Binding, StaticBindings>,
                               typename Binding::conversions>;
};

template <typename Binding, typename StaticBindings>
using binding_storage_t = typename binding_storage_selector<
    Binding, StaticBindings, typename Binding::scope>::type;

template <typename Request>
struct exact_root_fast_request
    : std::bool_constant<
          std::is_same_v<Request, typename annotated_traits<Request>::type> &&
          (std::is_lvalue_reference_v<Request> || std::is_pointer_v<Request>) &&
          !is_unique_ptr_v<std::remove_const_t<std::remove_reference_t<Request>>> &&
          !is_optional_v<std::remove_const_t<std::remove_reference_t<Request>>>> {
};

template <typename Request>
static constexpr bool exact_root_fast_request_v =
    exact_root_fast_request<Request>::value;

template <typename Interface, typename Storage>
struct interface_state {
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
decltype(auto) cached_shared_ptr(Source& source, Conversions& conversions) {
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
        return Request(convert_binding_result<raw_request>(
            std::forward<Source>(source), conversions));
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
                return &cached_shared_ptr<element_type>(source, conversions);
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
            return object_reference<request_base>(std::forward<Source>(source));
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

template <typename Request, typename Source, typename Conversions>
decltype(auto) convert_binding_result_exact(Source&& source,
                                            Conversions& conversions) {
    using raw_request = typename annotated_traits<Request>::type;
    using request_base =
        std::remove_const_t<std::remove_reference_t<raw_request>>;

    static_assert(exact_root_fast_request_v<raw_request>,
                  "request does not support exact root fast resolution");

    if constexpr (std::is_pointer_v<raw_request>) {
        using target = std::remove_const_t<std::remove_pointer_t<raw_request>>;

        if constexpr (is_shared_ptr_v<target>) {
            using element_type = typename target::element_type;
            return &cached_shared_ptr<element_type>(source, conversions);
        } else {
            return object_pointer<target>(std::forward<Source>(source));
        }
    } else {
        if constexpr (is_shared_ptr_v<request_base>) {
            using target = typename request_base::element_type;
            return cached_shared_ptr<target>(source, conversions);
        } else {
            return object_reference<request_base>(std::forward<Source>(source));
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
    static constexpr bool supports_stable_resolution =
        std::is_same_v<typename Binding::scope, external> ||
        std::is_same_v<typename Binding::scope, shared> ||
        std::is_same_v<typename Binding::scope, shared_cyclical>;

    template <typename Request>
    static constexpr bool supports_exact_root_resolution =
        supports_stable_resolution &&
        exact_root_fast_request_v<
            typename request_traits<Request>::request_type>;

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
        return convert_binding_result<resolved_type>(
            storage_.resolve(context, owner), state.conversions);
    }

    template <typename Request, typename OwnerArg>
    decltype(auto) resolve_unscoped(OwnerArg& owner) {
        using resolved_type = typename request_traits<Request>::request_type;
        using interface_key = decay_t<resolved_type>;
        unscoped_resolving_context context;

        auto& state = static_cast<interface_state_type<interface_key>&>(*this);
        return convert_binding_result<resolved_type>(
            storage_.resolve(context, owner), state.conversions);
    }

    bool is_stable() const {
        if constexpr (std::is_same_v<typename Binding::scope, external>) {
            return true;
        } else if constexpr (supports_stable_resolution) {
            return storage_.is_resolved();
        } else {
            return false;
        }
    }

    template <typename Request> decltype(auto) resolve_stable() {
        using resolved_type = typename request_traits<Request>::request_type;
        using interface_key = decay_t<resolved_type>;

        auto& state = static_cast<interface_state_type<interface_key>&>(*this);
        return convert_binding_result<resolved_type>(storage_.get_stable(),
                                                     state.conversions);
    }

    template <typename Request> decltype(auto) resolve_stable_exact() {
        using resolved_type = typename request_traits<Request>::request_type;
        using interface_key = decay_t<resolved_type>;
        auto& state = static_cast<interface_state_type<interface_key>&>(*this);
        return convert_binding_result_exact<resolved_type>(storage_.get_stable(),
                                                           state.conversions);
    }

    template <typename Request, typename OwnerArg>
    decltype(auto) resolve_unscoped_exact(OwnerArg& owner) {
        using resolved_type = typename request_traits<Request>::request_type;
        using interface_key = decay_t<resolved_type>;
        unscoped_resolving_context context;

        auto& state = static_cast<interface_state_type<interface_key>&>(*this);
        return convert_binding_result_exact<resolved_type>(
            storage_.resolve(context, owner), state.conversions);
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

template <typename StaticBindings, typename Bindings>
class static_container_impl;

template <typename StaticBindings, typename... Bindings>
class static_container_impl<StaticBindings, type_list<Bindings...>> {
  public:
    using bindings_type = StaticBindings;

  private:
    friend struct unscoped_resolving_context;

    using self_type =
        static_container_impl<StaticBindings, type_list<Bindings...>>;
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

    template <typename Request>
    using direct_binding_t =
        find_binding_definition_t<type_list<Bindings...>,
                                  decay_t<resolved_type_t<Request>>,
                                  typename request_traits<Request>::id_type>;

    template <typename Request>
    static constexpr bool has_direct_binding_v =
        !std::is_same_v<direct_binding_t<Request>, no_binding_id> &&
        !std::is_same_v<direct_binding_t<Request>, ambiguous_binding>;

    template <typename Request>
    static constexpr bool has_ambiguous_binding_v =
        std::is_same_v<direct_binding_t<Request>, ambiguous_binding>;

    template <typename Request, typename... Binds>
    using provided_bindings_t = type_list<std::decay_t<Binds>...>;

    template <typename Request, typename... Binds>
    static constexpr bool root_binding_resolvable_v =
        bindings_resolvable_impl<StaticBindings, resolved_type_t<Request>,
                                 type_list<>,
                                 provided_bindings_t<Request, Binds...>,
                                 typename request_traits<Request>::id_type>::value;

    template <typename Request, typename... Binds>
    using root_open_argument_types_t =
        typename open_root_constructible<
            StaticBindings, Request,
            provided_bindings_t<Request, Binds...>>::type;

  public:
    template <typename Dummy = void,
              typename = std::enable_if_t<external_binding_count == 0, Dummy>>
    static_container_impl() {}

    template <typename... Binds,
              typename = std::enable_if_t<(sizeof...(Binds) != 0)>,
              typename = std::enable_if_t<
                  (is_bound_value_v<std::decay_t<Binds>> && ...)>,
              typename = std::enable_if_t<
                  bind_constructor_validation<StaticBindings,
                                              type_list<Bindings...>,
                                              std::decay_t<Binds>...>::value>>
    explicit static_container_impl(Binds&&... binds)
        : states_(make_state<Bindings>(std::forward<Binds>(binds)...)...) {}

    template <typename Request, typename... Binds>
    top_level_resolved_type_t<Request> resolve(Binds&&... binds) {
        static_assert(
            detail::root_constructible<
                StaticBindings, Request, provided_bindings_t<Request, Binds...>>::value,
            "bindings cannot resolve the requested type");

        if constexpr (has_direct_binding_v<Request>) {
            using binding = direct_binding_t<Request>;
            if constexpr (binding_state_type<binding>::supports_stable_resolution) {
                if (state<binding>().is_stable()) {
                    if constexpr (binding_state_type<binding>::template supports_exact_root_resolution<
                                      Request>) {
                        return state<binding>().template resolve_stable_exact<Request>();
                    } else {
                        return state<binding>().template resolve_stable<Request>();
                    }
                }
            }

            if constexpr (sizeof...(Binds) == 0 &&
                          request_unscoped_resolvable_v<StaticBindings, Request> &&
                          binding_state_type<binding>::template supports_exact_root_resolution<
                              Request>) {
                return state<binding>().template resolve_unscoped_exact<Request>(
                    *this);
            }
        }

        return resolve_with_runtime_context<Request>(
            std::bool_constant<sizeof...(Binds) == 0 &&
                               request_unscoped_resolvable_v<StaticBindings,
                                                             Request>>{},
            std::forward<Binds>(binds)...);
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
    R resolve(resolving_context& context) {
        static_assert(bindings_resolvable_v<StaticBindings, resolved_type_t<Request>,
                                            typename request_traits<Request>::id_type>,
                      "bindings cannot resolve the requested type");
        return resolve_dependency<Request>(context);
    }

    template <typename Request, bool RemoveRvalueReferences,
              bool = true,
              typename R = std::conditional_t<
                  RemoveRvalueReferences &&
                      std::is_rvalue_reference_v<resolved_type_t<Request>>,
                  std::remove_reference_t<resolved_type_t<Request>>,
                  resolved_type_t<Request>>>
    R resolve(resolving_context& context) {
        if constexpr (!RemoveRvalueReferences &&
                      std::is_rvalue_reference_v<resolved_type_t<Request>>) {
            using temporary_type =
                std::remove_reference_t<resolved_type_t<Request>>;
            auto& instance =
                context.template construct<temporary_type>(resolve<Request>(context));
            return std::move(instance);
        } else {
            return resolve<Request>(context);
        }
    }

  private:
    template <typename Request, typename... Binds>
    class bound_root_container {
      public:
        using bindings_type = StaticBindings;

        explicit bound_root_container(self_type& owner, Binds&... binds)
            : owner_(owner), binds_(binds...) {}

        template <typename T>
        decltype(auto) resolve(resolving_context& context) {
            return resolve_dependency<T>(context);
        }

        template <typename T, bool RemoveRvalueReferences, bool = true,
                  typename R = std::conditional_t<
                      RemoveRvalueReferences &&
                          std::is_rvalue_reference_v<resolved_type_t<T>>,
                      std::remove_reference_t<resolved_type_t<T>>,
                      resolved_type_t<T>>>
        R resolve(resolving_context& context) {
            if constexpr (!RemoveRvalueReferences &&
                          std::is_rvalue_reference_v<resolved_type_t<T>>) {
                using temporary_type =
                    std::remove_reference_t<resolved_type_t<T>>;
                auto& instance =
                    context.template construct<temporary_type>(resolve<T>(context));
                return std::move(instance);
            } else {
                return resolve<T>(context);
            }
        }

        template <typename RequestT>
        top_level_resolved_type_t<RequestT> resolve_dependency(
            resolving_context& context) {
            using interface_key = decay_t<resolved_type_t<RequestT>>;
            using id_type = typename request_traits<RequestT>::id_type;
            using binding =
                find_binding_definition_t<type_list<Bindings...>, interface_key, id_type>;

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
                        return bound_value<RequestT>(find_bind<bind_type>(binds...));
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
        resolving_context& context) {
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

    template <typename Request>
    top_level_resolved_type_t<Request> resolve_dependency_unscoped() {
        using interface_key = decay_t<resolved_type_t<Request>>;
        using id_type = typename request_traits<Request>::id_type;
        using binding =
            find_binding_definition_t<type_list<Bindings...>, interface_key, id_type>;

        static_assert(!std::is_same_v<binding, no_binding_id>,
                      "binding not found");
        static_assert(!std::is_same_v<binding, ambiguous_binding>,
                      "binding is ambiguous");
        static_assert(request_unscoped_resolvable_v<StaticBindings, Request>,
                      "binding requires resolving_context");
        return state<binding>().template resolve_unscoped<Request>(*this);
    }

    template <typename Request, typename... Binds>
    top_level_resolved_type_t<Request> resolve_root(resolving_context& context,
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

    template <typename Request, typename... Binds>
    top_level_resolved_type_t<Request> resolve_with_runtime_context(
        std::true_type, Binds&&...) {
        return resolve_dependency_unscoped<Request>();
    }

    template <typename Request, typename... Binds>
    top_level_resolved_type_t<Request> resolve_with_runtime_context(
        std::false_type, Binds&&... binds) {
        resolving_context context;
        return resolve_root<Request>(context, std::forward<Binds>(binds)...);
    }

    template <typename Binding, typename FirstBind, typename... RestBinds>
    static decltype(auto) external_bind(FirstBind&& first_bind,
                                        RestBinds&&... rest_binds) {
        if constexpr (bound_matches_binding<Binding, StaticBindings,
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

template <typename StaticBindings, typename ParentContainer, typename Bindings>
class runtime_bindings_container_impl;

template <typename StaticBindings, typename ParentContainer, typename... Bindings>
class runtime_bindings_container_impl<StaticBindings, ParentContainer,
                                      type_list<Bindings...>> {
  public:
    using bindings_type = StaticBindings;

  private:
    using self_type = runtime_bindings_container_impl<
        StaticBindings, ParentContainer, type_list<Bindings...>>;

    template <typename Binding>
    using binding_state_type = binding_state_t<Binding, self_type>;

    template <typename Request>
    using resolved_type_t = typename request_traits<Request>::request_type;

    template <typename Request>
    using top_level_resolved_type_t =
        std::conditional_t<std::is_rvalue_reference_v<resolved_type_t<Request>>,
                           std::remove_reference_t<resolved_type_t<Request>>,
                           resolved_type_t<Request>>;

    template <typename Request>
    using direct_binding_t =
        find_binding_definition_t<type_list<Bindings...>,
                                  decay_t<resolved_type_t<Request>>,
                                  typename request_traits<Request>::id_type>;

    template <typename Request>
    static constexpr bool has_direct_binding_v =
        !std::is_same_v<direct_binding_t<Request>, no_binding_id> &&
        !std::is_same_v<direct_binding_t<Request>, ambiguous_binding>;

    template <typename Request>
    static constexpr bool has_ambiguous_binding_v =
        std::is_same_v<direct_binding_t<Request>, ambiguous_binding>;

  public:
    template <typename... Binds,
              typename = std::enable_if_t<
                  (is_bound_value_v<std::decay_t<Binds>> && ...)>>
    explicit runtime_bindings_container_impl(ParentContainer* parent,
                                             Binds&&... binds)
        : parent_(parent),
          states_(make_state<Bindings>(std::forward<Binds>(binds)...)...) {
        using validation = bind_argument_validation<
            StaticBindings, type_list<Bindings...>, std::decay_t<Binds>...>;
        static_assert(validation::value,
                      "invalid bind arguments for runtime_bindings_container");
    }

    template <typename Request>
    top_level_resolved_type_t<Request> resolve() {
        resolving_context context;
        return resolve<Request, true>(context);
    }

    template <typename Request,
              typename R = std::conditional_t<
                  std::is_rvalue_reference_v<resolved_type_t<Request>>,
                  std::remove_reference_t<resolved_type_t<Request>>,
                  resolved_type_t<Request>>>
    R resolve(resolving_context& context) {
        return resolve_dependency<Request>(context);
    }

    template <typename Request, bool RemoveRvalueReferences, bool = true,
              typename R = std::conditional_t<
                  RemoveRvalueReferences &&
                      std::is_rvalue_reference_v<resolved_type_t<Request>>,
                  std::remove_reference_t<resolved_type_t<Request>>,
                  resolved_type_t<Request>>>
    R resolve(resolving_context& context) {
        if constexpr (!RemoveRvalueReferences &&
                      std::is_rvalue_reference_v<resolved_type_t<Request>>) {
            using temporary_type =
                std::remove_reference_t<resolved_type_t<Request>>;
            auto& instance =
                context.template construct<temporary_type>(resolve<Request>(context));
            return std::move(instance);
        } else {
            return resolve<Request>(context);
        }
    }

    template <typename T,
              typename Factory = ::dingo::constructor_detection<decay_t<T>>>
    T construct(Factory factory = Factory()) {
        resolving_context context;
        return construct<T>(context, factory);
    }

    template <typename T,
              typename Factory = ::dingo::constructor_detection<decay_t<T>>>
    T construct(resolving_context& context, Factory factory = Factory()) {
        return factory.template construct<T>(context, *this);
    }

  private:
    template <typename Request>
    top_level_resolved_type_t<Request> resolve_parent(
        resolving_context& context) {
        static_assert(!std::is_same_v<ParentContainer, void>,
                      "missing parent container");
        assert(parent_ != nullptr);

        using id_type = typename request_traits<Request>::id_type;
        if constexpr (std::is_same_v<id_type, no_binding_id>) {
            return parent_->template resolve<resolved_type_t<Request>, false>(
                context);
        } else {
            return parent_->template resolve<resolved_type_t<Request>, false>(
                context, id_type::get());
        }
    }

    template <typename Request>
    top_level_resolved_type_t<Request> resolve_dependency(
        resolving_context& context) {
        if constexpr (has_ambiguous_binding_v<Request>) {
            static_assert(!has_ambiguous_binding_v<Request>,
                          "binding is ambiguous");
        } else if constexpr (has_direct_binding_v<Request>) {
            using binding = direct_binding_t<Request>;

            if constexpr (binding_state_type<binding>::supports_stable_resolution) {
                if (state<binding>().is_stable()) {
                    if constexpr (binding_state_type<binding>::template supports_exact_root_resolution<
                                      Request>) {
                        return state<binding>().template resolve_stable_exact<Request>();
                    } else {
                        return state<binding>().template resolve_stable<Request>();
                    }
                }
            }

            return state<binding>().template resolve<Request>(context, *this);
        } else {
            return resolve_parent<Request>(context);
        }
    }

    template <typename Binding> binding_state_type<Binding>& state() {
        return std::get<binding_state_type<Binding>>(states_);
    }

    template <typename Binding, typename FirstBind, typename... RestBinds>
    static decltype(auto) external_bind(FirstBind&& first_bind,
                                        RestBinds&&... rest_binds) {
        if constexpr (bound_matches_binding<Binding, StaticBindings,
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

    ParentContainer* parent_;
    std::tuple<binding_state_type<Bindings>...> states_;
};

} // namespace detail

template <typename StaticBindings>
class static_container
    : public detail::static_container_impl<StaticBindings,
                                           detail::binding_definitions_t<StaticBindings>> {
    using base_type = detail::static_container_impl<
        StaticBindings, detail::binding_definitions_t<StaticBindings>>;

  public:
    using base_type::base_type;
    using base_type::construct;
    using base_type::resolve;
};

template <typename StaticBindings, typename ParentContainer>
class runtime_bindings_container
    : public detail::runtime_bindings_container_impl<
          StaticBindings, ParentContainer,
          detail::binding_definitions_t<StaticBindings>> {
    using base_type = detail::runtime_bindings_container_impl<
        StaticBindings, ParentContainer,
        detail::binding_definitions_t<StaticBindings>>;

  public:
    using base_type::base_type;
    using base_type::construct;
    using base_type::resolve;
};

} // namespace dingo
