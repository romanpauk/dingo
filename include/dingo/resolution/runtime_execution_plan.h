//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/config.h>

#include <dingo/resolution/ir.h>
#include <dingo/resolution/type_conversion.h>
#include <dingo/type/type_conversion_traits.h>
#include <dingo/type/type_descriptor.h>

#include <cstddef>
#include <string>
#include <string_view>

namespace dingo {
struct unique;
struct shared;
struct external;
struct shared_cyclical;
class resolving_context;

namespace detail {
enum class runtime_request_kind {
    value,
    lvalue_reference,
    rvalue_reference,
    pointer
};

enum class runtime_access_kind {
    value,
    lvalue_reference,
    rvalue_reference,
    pointer
};

enum class runtime_conversion_route {
    request_dispatch,
    source_fetch,
    source_access
};

using runtime_conversion_family = ir::conversion_family;
using runtime_conversion_kind = ir::conversion_kind;

enum class runtime_binding_shape_kind {
    plain,
    array,
    alternative,
    wrapper
};

enum class runtime_storage_kind {
    unique,
    shared,
    external,
    shared_cyclical,
    unknown
};

template <typename TypeIndex> struct runtime_selected_conversion {
    bool available = false;
    runtime_conversion_family family = runtime_conversion_family::native;
    runtime_conversion_kind kind = runtime_conversion_kind::value_copy_or_move;
    type_descriptor target_type = {};
};

template <typename StorageTag> struct runtime_storage_kind_of {
    static constexpr auto value = runtime_storage_kind::unknown;
};

template <> struct runtime_storage_kind_of<unique> {
    static constexpr auto value = runtime_storage_kind::unique;
};

template <> struct runtime_storage_kind_of<shared> {
    static constexpr auto value = runtime_storage_kind::shared;
};

template <> struct runtime_storage_kind_of<external> {
    static constexpr auto value = runtime_storage_kind::external;
};

template <> struct runtime_storage_kind_of<shared_cyclical> {
    static constexpr auto value = runtime_storage_kind::shared_cyclical;
};

template <typename FactoryPtr, typename TypeIndex> struct runtime_binding_record {
    using request_executor_type = void* (*)(
        FactoryPtr, resolving_context&, runtime_request_kind,
        const TypeIndex&, type_descriptor);
    using source_fetcher_type = void* (*)(FactoryPtr, resolving_context&);
    using source_converter_type = void* (*)(
        void*, resolving_context&, runtime_request_kind, const TypeIndex&,
        type_descriptor, type_descriptor);
    using conversion_selector_type =
        runtime_selected_conversion<TypeIndex> (*)(
            runtime_request_kind, const TypeIndex&);

    std::size_t id;
    runtime_storage_kind storage_kind;
    type_descriptor interface_type;
    type_descriptor registered_type;
    type_descriptor stored_type;
    type_descriptor source_type;
    runtime_access_kind access_kind;
    runtime_binding_shape_kind shape_kind;
    bool wrapper_enabled;
    bool value_borrowable;
    bool pointer_like;
    TypeIndex lookup_type;
    bool supports_source_access;
    request_executor_type execute_request;
    source_fetcher_type fetch_source;
    source_converter_type convert_source;
    conversion_selector_type select_conversion;
    FactoryPtr factory;
    bool cacheable;
};

template <typename SourceType> struct runtime_access_kind_of {
    static constexpr auto value = runtime_access_kind::value;
};

template <typename SourceType>
struct runtime_access_kind_of<SourceType&> {
    static constexpr auto value = runtime_access_kind::lvalue_reference;
};

template <typename SourceType>
struct runtime_access_kind_of<SourceType&&> {
    static constexpr auto value = runtime_access_kind::rvalue_reference;
};

template <typename SourceType>
struct runtime_access_kind_of<SourceType*> {
    static constexpr auto value = runtime_access_kind::pointer;
};

template <typename T>
using runtime_plain_shape_value_t =
    std::remove_cv_t<std::remove_pointer_t<std::remove_reference_t<T>>>;

template <typename StorageType, typename = void>
struct runtime_binding_shape_kind_of {
    static constexpr auto value = runtime_binding_shape_kind::plain;
};

template <typename StorageType>
struct runtime_binding_shape_kind_of<
    StorageType,
    std::enable_if_t<std::is_array_v<typename StorageType::type>>> {
    static constexpr auto value = runtime_binding_shape_kind::array;
};

template <typename StorageType>
struct runtime_binding_shape_kind_of<
    StorageType,
    std::enable_if_t<!std::is_array_v<typename StorageType::type> &&
                     is_alternative_type_v<
                         std::remove_cv_t<typename StorageType::type>>>> {
    static constexpr auto value = runtime_binding_shape_kind::alternative;
};

template <typename StorageType>
struct runtime_binding_shape_kind_of<
    StorageType,
    std::enable_if_t<!std::is_array_v<typename StorageType::type> &&
                     !is_alternative_type_v<
                         std::remove_cv_t<typename StorageType::type>> &&
                     type_traits<std::remove_cv_t<typename StorageType::type>>::enabled>> {
    static constexpr auto value = runtime_binding_shape_kind::wrapper;
};

template <typename StorageType, typename T>
struct runtime_supports_source_access
    : std::bool_constant<
          !std::is_array_v<typename StorageType::type> &&
          !std::is_array_v<std::remove_reference_t<T>> &&
          !type_traits<runtime_plain_shape_value_t<T>>::enabled &&
          !is_alternative_type_v<runtime_plain_shape_value_t<T>>> {};

template <typename T>
struct runtime_request_supports_source_access
    : std::bool_constant<!std::is_array_v<std::remove_reference_t<T>> &&
                         !type_traits<runtime_plain_shape_value_t<T>>::enabled &&
                         !is_alternative_type_v<runtime_plain_shape_value_t<T>>> {};

template <typename RTTI, typename ConcreteFactory, typename FactoryPtr>
void* execute_runtime_request(FactoryPtr factory, resolving_context& context,
                              runtime_request_kind request_kind,
                              const typename RTTI::type_index& lookup_type,
                              type_descriptor requested_type) {
    auto& concrete_factory = *static_cast<ConcreteFactory*>(factory);

    switch (request_kind) {
    case runtime_request_kind::value:
        return concrete_factory.get_value(context, lookup_type, requested_type);
    case runtime_request_kind::lvalue_reference:
        return concrete_factory.get_lvalue_reference(context, lookup_type,
                                                     requested_type);
    case runtime_request_kind::rvalue_reference:
        return concrete_factory.get_rvalue_reference(context, lookup_type,
                                                     requested_type);
    case runtime_request_kind::pointer:
        return concrete_factory.get_pointer(context, lookup_type,
                                            requested_type);
    }

    return nullptr;
}

template <typename RTTI, typename ConcreteFactory, typename FactoryPtr>
constexpr auto runtime_request_executor_for()
    -> typename runtime_binding_record<
        FactoryPtr, typename RTTI::type_index>::request_executor_type {
    if constexpr (std::is_void_v<ConcreteFactory>) {
        return nullptr;
    } else {
        return &execute_runtime_request<RTTI, ConcreteFactory, FactoryPtr>;
    }
}

template <typename Context, typename T>
void* get_runtime_conversion_address(Context& context, T&& instance);

template <typename ConcreteFactory, typename Context, typename = void>
struct runtime_has_source_fetch : std::false_type {};

template <typename ConcreteFactory, typename Context>
struct runtime_has_source_fetch<
    ConcreteFactory, Context,
    std::void_t<decltype(std::declval<ConcreteFactory&>().resolve(
        std::declval<Context&>()))>> : std::true_type {};

template <typename ConcreteFactory, typename FactoryPtr>
void* fetch_runtime_source(FactoryPtr factory, resolving_context& context) {
    auto& concrete_factory = *static_cast<ConcreteFactory*>(factory);
    auto&& source = concrete_factory.resolve(context);
    return get_runtime_conversion_address(context,
                                          std::forward<decltype(source)>(source));
}

template <typename RTTI, typename ConcreteFactory, typename FactoryPtr>
constexpr auto runtime_source_fetcher_for()
    -> typename runtime_binding_record<
        FactoryPtr, typename RTTI::type_index>::source_fetcher_type {
    if constexpr (std::is_void_v<ConcreteFactory> ||
                  !runtime_has_source_fetch<ConcreteFactory,
                                            resolving_context>::value) {
        return nullptr;
    } else {
        return &fetch_runtime_source<ConcreteFactory, FactoryPtr>;
    }
}

template <typename T>
using runtime_direct_target_type_t = std::remove_reference_t<T>;

template <typename Target>
using runtime_direct_target_value_t =
    std::remove_cv_t<runtime_direct_target_type_t<Target>>;

template <typename Source>
class runtime_source_adapter {
  public:
    explicit runtime_source_adapter(void* source_ptr) : source_ptr_(source_ptr) {}

    template <typename Context> decltype(auto) resolve(Context& context) {
        return get(context);
    }

    template <typename Target, typename Context> Target& resolve(Context& context) {
        using source_value_type = std::remove_cv_t<std::remove_reference_t<Source>>;

        auto&& source = get(context);
        if constexpr (std::is_same_v<Target, source_value_type> &&
                      std::is_lvalue_reference_v<decltype(source)>) {
            return source;
        } else {
            return context.template construct<Target>(
                type_conversion_traits<Target, source_value_type>::convert(
                    std::forward<decltype(source)>(source)));
        }
    }

  private:
    template <typename Context> decltype(auto) get(Context&) {
        if constexpr (std::is_lvalue_reference_v<Source>) {
            return *static_cast<std::remove_reference_t<Source>*>(source_ptr_);
        } else if constexpr (std::is_rvalue_reference_v<Source>) {
            return std::move(
                *static_cast<std::remove_reference_t<Source>*>(source_ptr_));
        } else if constexpr (std::is_pointer_v<Source>) {
            return static_cast<Source>(source_ptr_);
        } else {
            return std::move(*static_cast<Source*>(source_ptr_));
        }
    }
    void* source_ptr_;
};

template <typename Context, typename T>
void* get_runtime_conversion_address(Context& context, T&& instance) {
    if constexpr (std::is_reference_v<T>) {
        return std::addressof(instance);
    } else if constexpr (std::is_pointer_v<T>) {
        return instance;
    } else {
        return std::addressof(
            context.template construct<std::remove_cv_t<std::remove_reference_t<T>>>(
                std::forward<T>(instance)));
    }
}

template <typename RTTI, typename InterfaceType, typename StorageType,
          typename SourceType, typename Lookup>
void* apply_runtime_source_conversion_if_supported(
    void* source_ptr, resolving_context& context, type_descriptor requested_type,
    type_descriptor registered_type) {
    using request_kind = std::conditional_t<
        std::is_pointer_v<Lookup>, ir::pointer_request,
        std::conditional_t<
            std::is_rvalue_reference_v<Lookup>, ir::rvalue_reference_request,
            std::conditional_t<std::is_lvalue_reference_v<Lookup>,
                               ir::lvalue_reference_request,
                               ir::value_request>>>;
    using conversion = selected_conversion_for_kind_t<
        request_kind, InterfaceType, StorageType, SourceType, Lookup>;
    runtime_source_adapter<SourceType> source(source_ptr);
    auto&& instance = execute_conversion<conversion>(source, context,
                                                     requested_type,
                                                     registered_type);
    return get_runtime_conversion_address(
        context, std::forward<decltype(instance)>(instance));
}

template <typename RTTI, typename InterfaceType, typename StorageType,
          typename SourceType, typename... Lookups>
void* dispatch_runtime_source_conversion(
    void* source_ptr, resolving_context& context,
    const typename RTTI::type_index& lookup_type, type_descriptor requested_type,
    type_descriptor registered_type, type_list<Lookups...>) {
    if constexpr (sizeof...(Lookups) == 0) {
        (void)source_ptr;
        (void)context;
        (void)lookup_type;
        (void)requested_type;
        (void)registered_type;
        return nullptr;
    }

    void* result = nullptr;
    const bool matched =
        ((RTTI::template get_type_index<lookup_type_t<Lookups>>() == lookup_type
              ? ((result =
                      apply_runtime_source_conversion_if_supported<
                          RTTI, InterfaceType, StorageType, SourceType,
                          Lookups>(source_ptr, context, requested_type,
                                   registered_type)) != nullptr)
              : false) ||
         ...);

    if (!matched) {
        return nullptr;
    }

    return result;
}

template <typename RTTI, typename InterfaceType, typename StorageType,
          typename SourceType>
void* convert_runtime_source(
    void* source_ptr, resolving_context& context,
    runtime_request_kind request_kind,
    const typename RTTI::type_index& lookup_type, type_descriptor requested_type,
    type_descriptor registered_type) {
    switch (request_kind) {
    case runtime_request_kind::value:
        return dispatch_runtime_source_conversion<RTTI, InterfaceType,
                                                  StorageType, SourceType>(
            source_ptr, context, lookup_type, requested_type, registered_type,
            typename StorageType::conversions::value_types{});
    case runtime_request_kind::lvalue_reference:
        return dispatch_runtime_source_conversion<RTTI, InterfaceType,
                                                  StorageType, SourceType>(
            source_ptr, context, lookup_type, requested_type, registered_type,
            typename StorageType::conversions::lvalue_reference_types{});
    case runtime_request_kind::rvalue_reference:
        return dispatch_runtime_source_conversion<RTTI, InterfaceType,
                                                  StorageType, SourceType>(
            source_ptr, context, lookup_type, requested_type, registered_type,
            typename StorageType::conversions::rvalue_reference_types{});
    case runtime_request_kind::pointer:
        return dispatch_runtime_source_conversion<RTTI, InterfaceType,
                                                  StorageType, SourceType>(
            source_ptr, context, lookup_type, requested_type, registered_type,
            typename StorageType::conversions::pointer_types{});
    }

    return nullptr;
}

template <typename RTTI, typename InterfaceType, typename StorageType,
          typename SourceType>
constexpr auto runtime_source_converter_for()
    -> typename runtime_binding_record<
        void*, typename RTTI::type_index>::source_converter_type {
    using storage_value_type = std::remove_cv_t<typename StorageType::type>;

    if constexpr (is_alternative_type_v<storage_value_type> ||
                  type_traits<storage_value_type>::enabled) {
        return &convert_runtime_source<RTTI, InterfaceType, StorageType,
                                       SourceType>;
    } else {
        return nullptr;
    }
}

template <typename RTTI, typename RequestKind, typename InterfaceType,
          typename StorageType, typename SourceType, typename Lookup>
constexpr auto make_runtime_selected_conversion()
    -> runtime_selected_conversion<typename RTTI::type_index> {
    using conversion = selected_conversion_ir_t<
        ir::request<void, void, void, void, RequestKind>, InterfaceType,
        StorageType, SourceType, Lookup>;
    return {true, conversion::family, conversion::kind,
            describe_type<typename conversion::target_type>()};
}

template <typename RTTI, typename RequestKind, typename InterfaceType,
          typename StorageType, typename SourceType, typename... Lookups>
auto dispatch_runtime_selected_conversion(
    const typename RTTI::type_index& lookup_type, type_list<Lookups...>)
    -> runtime_selected_conversion<typename RTTI::type_index> {
    if constexpr (sizeof...(Lookups) == 0) {
        return {};
    } else {
        runtime_selected_conversion<typename RTTI::type_index> selected;
        const bool matched =
            ((RTTI::template get_type_index<lookup_type_t<Lookups>>() ==
                      lookup_type
                  ? (selected = make_runtime_selected_conversion<
                        RTTI, RequestKind, InterfaceType, StorageType,
                        SourceType, Lookups>(),
                     true)
                  : false) ||
             ...);
        return matched ? selected
                       : runtime_selected_conversion<typename RTTI::type_index>{};
    }
}

template <typename RTTI, typename InterfaceType, typename StorageType,
          typename SourceType>
auto select_runtime_conversion(runtime_request_kind request_kind,
                               const typename RTTI::type_index& lookup_type)
    -> runtime_selected_conversion<typename RTTI::type_index> {
    auto fallback = [&]() -> runtime_selected_conversion<typename RTTI::type_index> {
        if constexpr (std::is_array_v<typename StorageType::type>) {
            switch (request_kind) {
            case runtime_request_kind::value:
                return {true, runtime_conversion_family::array,
                        request_conversion_kind_ir<ir::value_request>::value,
                        {}};
            case runtime_request_kind::lvalue_reference:
                return {true, runtime_conversion_family::array,
                        request_conversion_kind_ir<
                            ir::lvalue_reference_request>::value,
                        {}};
            case runtime_request_kind::rvalue_reference:
                return {true, runtime_conversion_family::array,
                        request_conversion_kind_ir<
                            ir::rvalue_reference_request>::value,
                        {}};
            case runtime_request_kind::pointer:
                return {true, runtime_conversion_family::array,
                        request_conversion_kind_ir<ir::pointer_request>::value,
                        {}};
            }
        }

        return {};
    };

    switch (request_kind) {
    case runtime_request_kind::value:
        if (auto selected = dispatch_runtime_selected_conversion<
            RTTI, ir::value_request, InterfaceType, StorageType, SourceType>(
                lookup_type, typename StorageType::conversions::value_types{});
            selected.available) {
            return selected;
        }
        return fallback();
    case runtime_request_kind::lvalue_reference:
        if (auto selected = dispatch_runtime_selected_conversion<
            RTTI, ir::lvalue_reference_request, InterfaceType, StorageType,
            SourceType>(
                lookup_type,
                typename StorageType::conversions::lvalue_reference_types{});
            selected.available) {
            return selected;
        }
        return fallback();
    case runtime_request_kind::rvalue_reference:
        if (auto selected = dispatch_runtime_selected_conversion<
            RTTI, ir::rvalue_reference_request, InterfaceType, StorageType,
            SourceType>(
                lookup_type,
                typename StorageType::conversions::rvalue_reference_types{});
            selected.available) {
            return selected;
        }
        return fallback();
    case runtime_request_kind::pointer:
        if (auto selected = dispatch_runtime_selected_conversion<
            RTTI, ir::pointer_request, InterfaceType, StorageType, SourceType>(
                lookup_type, typename StorageType::conversions::pointer_types{});
            selected.available) {
            return selected;
        }
        return fallback();
    }

    return {};
}

template <typename RTTI, typename InterfaceType, typename StorageType,
          typename SourceType>
constexpr auto runtime_conversion_selector_for()
    -> typename runtime_binding_record<
        void*, typename RTTI::type_index>::conversion_selector_type {
    return &select_runtime_conversion<RTTI, InterfaceType, StorageType,
                                      SourceType>;
}

template <typename RTTI, typename InterfaceType, typename StorageType,
          typename SourceType, typename ConcreteFactory = void,
          typename FactoryPtr = void*>
constexpr auto make_runtime_binding_record(std::size_t id, FactoryPtr factory)
    -> runtime_binding_record<FactoryPtr, typename RTTI::type_index> {
    using interface_service_type = typename annotated_traits<InterfaceType>::type;
    return {id,
            runtime_storage_kind_of<typename StorageType::tag_type>::value,
            describe_type<InterfaceType>(),
            describe_type<typename StorageType::type>(),
            describe_type<typename StorageType::stored_type>(),
            describe_type<SourceType>(),
            runtime_access_kind_of<SourceType>::value,
            runtime_binding_shape_kind_of<StorageType>::value,
            type_traits<std::remove_cv_t<typename StorageType::type>>::enabled,
            type_traits<std::remove_cv_t<typename StorageType::type>>::is_value_borrowable,
            is_pointer_like_type_v<std::remove_cv_t<typename StorageType::type>>,
            RTTI::template get_type_index<rebind_leaf_t<SourceType, runtime_type>>(),
            runtime_supports_source_access<StorageType, SourceType>::value,
            runtime_request_executor_for<RTTI, ConcreteFactory, FactoryPtr>(),
            runtime_source_fetcher_for<RTTI, ConcreteFactory, FactoryPtr>(),
            runtime_source_converter_for<RTTI, interface_service_type, StorageType,
                                         SourceType>(),
            runtime_conversion_selector_for<RTTI, interface_service_type, StorageType,
                                            SourceType>(),
            factory,
            StorageType::cacheable};
}

template <typename RTTI, typename Binding> struct execution_plan {
    using type_index = typename RTTI::type_index;

    runtime_request_kind request_kind;
    runtime_access_kind access_kind;
    runtime_conversion_route conversion_route;
    runtime_conversion_family conversion_family;
    runtime_conversion_kind conversion_kind;
    type_index lookup_type;
    type_descriptor requested_type;
    type_descriptor conversion_source_type;
    type_descriptor conversion_target_type;
    Binding binding;
};

constexpr std::string_view runtime_request_kind_name(runtime_request_kind kind) {
    switch (kind) {
    case runtime_request_kind::value:
        return "value";
    case runtime_request_kind::lvalue_reference:
        return "lvalue_reference";
    case runtime_request_kind::rvalue_reference:
        return "rvalue_reference";
    case runtime_request_kind::pointer:
        return "pointer";
    }

    return "unknown";
}

constexpr std::string_view runtime_access_kind_name(runtime_access_kind kind) {
    switch (kind) {
    case runtime_access_kind::value:
        return "value";
    case runtime_access_kind::lvalue_reference:
        return "lvalue_reference";
    case runtime_access_kind::rvalue_reference:
        return "rvalue_reference";
    case runtime_access_kind::pointer:
        return "pointer";
    }

    return "unknown";
}

constexpr std::string_view runtime_conversion_kind_name(
    runtime_conversion_kind kind) {
    switch (kind) {
    case runtime_conversion_kind::value_copy_or_move:
        return "value_copy_or_move";
    case runtime_conversion_kind::lvalue_reference:
        return "lvalue_reference";
    case runtime_conversion_kind::rvalue_reference:
        return "rvalue_reference";
    case runtime_conversion_kind::pointer:
        return "pointer";
    case runtime_conversion_kind::copy_or_move_from_lvalue_reference:
        return "copy_or_move_from_lvalue_reference";
    case runtime_conversion_kind::copy_or_move_from_rvalue_reference:
        return "copy_or_move_from_rvalue_reference";
    case runtime_conversion_kind::dereference_pointer_to_value:
        return "dereference_pointer_to_value";
    case runtime_conversion_kind::dereference_pointer_to_lvalue_reference:
        return "dereference_pointer_to_lvalue_reference";
    case runtime_conversion_kind::dereference_pointer_to_rvalue_reference:
        return "dereference_pointer_to_rvalue_reference";
    case runtime_conversion_kind::address_of_lvalue_reference:
        return "address_of_lvalue_reference";
    }

    return "unknown";
}

constexpr std::string_view runtime_conversion_route_name(
    runtime_conversion_route route) {
    switch (route) {
    case runtime_conversion_route::request_dispatch:
        return "request_dispatch";
    case runtime_conversion_route::source_fetch:
        return "source_fetch";
    case runtime_conversion_route::source_access:
        return "source_access";
    }

    return "unknown";
}

constexpr std::string_view runtime_conversion_family_name(
    runtime_conversion_family family) {
    switch (family) {
    case runtime_conversion_family::native:
        return "native";
    case runtime_conversion_family::array:
        return "array";
    case runtime_conversion_family::alternative:
        return "alternative";
    case runtime_conversion_family::handle:
        return "handle";
    case runtime_conversion_family::borrow:
        return "borrow";
    case runtime_conversion_family::wrapper:
        return "wrapper";
    }

    return "unknown";
}

constexpr std::string_view runtime_binding_shape_kind_name(
    runtime_binding_shape_kind kind) {
    switch (kind) {
    case runtime_binding_shape_kind::plain:
        return "plain";
    case runtime_binding_shape_kind::array:
        return "array";
    case runtime_binding_shape_kind::alternative:
        return "alternative";
    case runtime_binding_shape_kind::wrapper:
        return "wrapper";
    }

    return "unknown";
}

constexpr std::string_view runtime_storage_kind_name(runtime_storage_kind kind) {
    switch (kind) {
    case runtime_storage_kind::unique:
        return "unique";
    case runtime_storage_kind::shared:
        return "shared";
    case runtime_storage_kind::external:
        return "external";
    case runtime_storage_kind::shared_cyclical:
        return "shared_cyclical";
    case runtime_storage_kind::unknown:
        return "unknown";
    }

    return "unknown";
}

template <typename RequestKind> struct runtime_request_kind_of;

template <> struct runtime_request_kind_of<ir::value_request> {
    static constexpr auto value = runtime_request_kind::value;
};

template <> struct runtime_request_kind_of<ir::lvalue_reference_request> {
    static constexpr auto value = runtime_request_kind::lvalue_reference;
};

template <> struct runtime_request_kind_of<ir::rvalue_reference_request> {
    static constexpr auto value = runtime_request_kind::rvalue_reference;
};

template <> struct runtime_request_kind_of<ir::pointer_request> {
    static constexpr auto value = runtime_request_kind::pointer;
};

template <typename RTTI, typename Plan>
constexpr auto lower_runtime_conversion_kind(Plan) -> runtime_conversion_kind {
    if constexpr (std::is_same_v<typename Plan::conversion_type,
                                 ir::erased_conversion>) {
        using request_type = typename Plan::request_type;
        return request_conversion_kind_ir<
            typename request_type::request_kind>::value;
    } else {
        return Plan::conversion_type::kind;
    }
}

template <typename RTTI, typename Plan, typename Binding>
constexpr auto lower_runtime_conversion_kind(Plan, const Binding& binding)
    -> runtime_conversion_kind {
    using request_type = typename Plan::request_type;
    using service_type = typename request_type::service_type;

    if constexpr (std::is_same_v<typename Plan::conversion_type,
                                 ir::erased_conversion>) {
        switch (binding.access_kind) {
        case runtime_access_kind::pointer:
            if constexpr (std::is_lvalue_reference_v<service_type>) {
                return runtime_conversion_kind::
                    dereference_pointer_to_lvalue_reference;
            } else if constexpr (std::is_rvalue_reference_v<service_type>) {
                return runtime_conversion_kind::
                    dereference_pointer_to_rvalue_reference;
            } else if constexpr (!std::is_pointer_v<service_type>) {
                return runtime_conversion_kind::dereference_pointer_to_value;
            }
            break;
        case runtime_access_kind::lvalue_reference:
            if constexpr (std::is_pointer_v<service_type>) {
                return runtime_conversion_kind::address_of_lvalue_reference;
            } else if constexpr (!std::is_reference_v<service_type>) {
                return runtime_conversion_kind::
                    copy_or_move_from_lvalue_reference;
            }
            break;
        case runtime_access_kind::rvalue_reference:
            if constexpr (!std::is_reference_v<service_type> &&
                          !std::is_pointer_v<service_type>) {
                return runtime_conversion_kind::
                    copy_or_move_from_rvalue_reference;
            }
            break;
        case runtime_access_kind::value:
            break;
        }
    }

    return lower_runtime_conversion_kind<RTTI>(Plan{});
}

template <typename T>
using runtime_request_value_type_t =
    std::remove_cv_t<std::remove_reference_t<T>>;

template <typename Plan>
constexpr auto lower_runtime_conversion_family(Plan)
    -> runtime_conversion_family {
    if constexpr (std::is_same_v<typename Plan::conversion_type,
                                 ir::erased_conversion>) {
        return runtime_conversion_family::native;
    } else {
        return Plan::conversion_type::family;
    }
}

template <typename ServiceType, typename Binding>
constexpr auto lower_runtime_request_conversion_family(const Binding& binding)
    -> runtime_conversion_family {
    using request_value_type = runtime_request_value_type_t<ServiceType>;

    switch (binding.shape_kind) {
    case runtime_binding_shape_kind::array:
        return runtime_conversion_family::array;
    case runtime_binding_shape_kind::alternative:
        return runtime_conversion_family::alternative;
    case runtime_binding_shape_kind::wrapper:
        if constexpr (is_pointer_like_type_v<request_value_type>) {
            return runtime_conversion_family::handle;
        }

        return binding.value_borrowable ? runtime_conversion_family::borrow
                                        : runtime_conversion_family::wrapper;
    case runtime_binding_shape_kind::plain:
        return runtime_conversion_family::native;
    }

    return runtime_conversion_family::native;
}

constexpr bool requires_source_access_path(runtime_conversion_kind kind) {
    switch (kind) {
    case runtime_conversion_kind::copy_or_move_from_lvalue_reference:
    case runtime_conversion_kind::copy_or_move_from_rvalue_reference:
    case runtime_conversion_kind::dereference_pointer_to_value:
    case runtime_conversion_kind::dereference_pointer_to_lvalue_reference:
    case runtime_conversion_kind::dereference_pointer_to_rvalue_reference:
    case runtime_conversion_kind::address_of_lvalue_reference:
        return true;
    case runtime_conversion_kind::value_copy_or_move:
    case runtime_conversion_kind::lvalue_reference:
    case runtime_conversion_kind::rvalue_reference:
    case runtime_conversion_kind::pointer:
        return false;
    }

    return false;
}

constexpr bool requires_source_fetch_path(runtime_conversion_family family) {
    switch (family) {
    case runtime_conversion_family::alternative:
    case runtime_conversion_family::handle:
    case runtime_conversion_family::borrow:
    case runtime_conversion_family::wrapper:
        return true;
    case runtime_conversion_family::native:
    case runtime_conversion_family::array:
        return false;
    }

    return false;
}

template <typename RTTI, typename Plan, typename Binding>
constexpr auto lower_runtime_plan(Plan, Binding binding)
    -> execution_plan<RTTI, Binding> {
    using request_type = typename Plan::request_type;
    using service_type = typename request_type::service_type;
    constexpr bool request_supports_source_access =
        runtime_request_supports_source_access<service_type>::value;
    constexpr auto request_kind =
        runtime_request_kind_of<typename request_type::request_kind>::value;
    constexpr auto request_access_kind = static_cast<runtime_access_kind>(
        runtime_request_kind_of<typename request_type::request_kind>::value);
    const auto selected_conversion =
        [&]() -> runtime_selected_conversion<typename RTTI::type_index> {
        if constexpr (std::is_same_v<typename Plan::conversion_type,
                                     ir::erased_conversion>) {
            return binding.select_conversion
                       ? binding.select_conversion(request_kind,
                                                  RTTI::template get_type_index<
                                                      typename request_type::
                                                          runtime_lookup_type>())
                       : runtime_selected_conversion<typename RTTI::type_index>{};
        } else {
            return {true, Plan::conversion_type::family,
                    Plan::conversion_type::kind,
                    describe_type<typename Plan::conversion_type::target_type>()};
        }
    }();
    const auto conversion_family =
        selected_conversion.available
            ? selected_conversion.family
            : (std::is_same_v<typename Plan::conversion_type,
                              ir::erased_conversion>
                   ? lower_runtime_request_conversion_family<service_type>(
                         binding)
                   : lower_runtime_conversion_family(Plan{}));
    const auto conversion_kind =
        selected_conversion.available
            ? selected_conversion.kind
            : (binding.supports_source_access && request_supports_source_access
                   ? lower_runtime_conversion_kind<RTTI>(Plan{}, binding)
                   : lower_runtime_conversion_kind<RTTI>(Plan{}));
    const bool use_source_access = binding.supports_source_access &&
                                   request_supports_source_access &&
                                   conversion_family ==
                                       runtime_conversion_family::native &&
                                   requires_source_access_path(conversion_kind);
    const bool use_source_fetch = !use_source_access &&
                                  binding.fetch_source &&
                                  binding.convert_source &&
                                  requires_source_fetch_path(
                                      conversion_family);

    return {request_kind,
            use_source_access ? binding.access_kind : request_access_kind,
            use_source_access
                ? runtime_conversion_route::source_access
                : (use_source_fetch ? runtime_conversion_route::source_fetch
                                    : runtime_conversion_route::request_dispatch),
            conversion_family,
            conversion_kind,
            use_source_access
                ? binding.lookup_type
                : RTTI::template get_type_index<
                      typename request_type::runtime_lookup_type>(),
            describe_type<service_type>(),
            binding.source_type,
            describe_type<service_type>(),
            binding};
}

template <typename Context, typename RTTI, typename Binding>
void* execute_runtime_plan(Context& context,
                           execution_plan<RTTI, Binding> plan) {
    if (plan.conversion_route == runtime_conversion_route::source_fetch) {
        assert(plan.binding.fetch_source);
        assert(plan.binding.convert_source);

        void* source = plan.binding.fetch_source(plan.binding.factory, context);
        if (!source) {
            return nullptr;
        }

        return plan.binding.convert_source(source, context, plan.request_kind,
                                           plan.lookup_type,
                                           plan.requested_type,
                                           plan.binding.registered_type);
    }

    if (plan.conversion_route == runtime_conversion_route::request_dispatch) {
        assert(plan.binding.execute_request);
        return plan.binding.execute_request(plan.binding.factory, context,
                                            plan.request_kind, plan.lookup_type,
                                            plan.requested_type);
    }

    assert(plan.conversion_route == runtime_conversion_route::source_access);

    switch (plan.access_kind) {
    case runtime_access_kind::value:
        return plan.binding.factory->get_value(context, plan.lookup_type,
                                               plan.requested_type);
    case runtime_access_kind::lvalue_reference:
        return plan.binding.factory->get_lvalue_reference(
            context, plan.lookup_type, plan.requested_type);
    case runtime_access_kind::rvalue_reference:
        return plan.binding.factory->get_rvalue_reference(
            context, plan.lookup_type, plan.requested_type);
    case runtime_access_kind::pointer:
        return plan.binding.factory->get_pointer(context, plan.lookup_type,
                                                 plan.requested_type);
    }

    return nullptr;
}
} // namespace detail
} // namespace dingo
