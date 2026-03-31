//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/config.h>

#include <dingo/exceptions.h>
#include <dingo/resolution/ir.h>
#include <dingo/resolution/interface_storage_traits.h>
#include <dingo/type/rebind_type.h>
#include <dingo/type/type_conversion_traits.h>
#include <dingo/type/type_list.h>
#include <dingo/type/type_traits.h>

#include <cassert>
#include <cstddef>
#include <memory>

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4702)
#endif

namespace dingo {
namespace detail {
using runtime_conversion_kind = ir::conversion_kind;

template <typename Factory, typename Context>
decltype(auto) resolve_source(Factory& factory, Context& context) {
    return factory.resolve(context);
}

template <typename Factory, typename Context>
auto* resolve_address(Factory& factory, Context& context) {
    return std::addressof(resolve_source(factory, context));
}

template <typename Factory, typename Context>
decltype(auto) dereference_source(Factory& factory, Context& context) {
    return *resolve_source(factory, context);
}

[[noreturn]] inline void throw_not_convertible(type_descriptor requested_type,
                                               type_descriptor registered_type) {
    throw make_type_not_convertible_exception(requested_type, registered_type);
}

template <typename Source, typename Factory, typename Context>
decltype(auto) resolve_source_value(Factory& factory, Context& context) {
    if constexpr (std::is_pointer_v<Source>) {
        return *resolve_source(factory, context);
    } else {
        return resolve_source(factory, context);
    }
}

template <typename Source, typename Factory, typename Context>
auto resolve_source_pointer(Factory& factory, Context& context) {
    if constexpr (std::is_pointer_v<Source>) {
        return resolve_source(factory, context);
    } else {
        return std::addressof(resolve_source(factory, context));
    }
}

template <typename Target, typename Source>
inline constexpr bool is_same_handle_shape_v =
    std::is_same_v<rebind_leaf_t<Target, runtime_type>,
                   rebind_leaf_t<Source, runtime_type>>;

template <typename Target, typename Source>
inline constexpr bool is_handle_conversion_usable_v =
    !std::is_same_v<Target, Source> && is_same_handle_shape_v<Target, Source> &&
    is_handle_rebindable_v<Source, Target>;

template <typename Target, typename Source>
Target& borrow_reference(Source& source, type_descriptor requested_type,
                         type_descriptor registered_type) {
    if constexpr (std::is_same_v<Target, Source>) {
        return source;
    } else if constexpr (std::is_convertible_v<Source*, Target*>) {
        return static_cast<Target&>(source);
    } else if constexpr (type_traits<Source>::enabled &&
                         type_traits<Source>::is_value_borrowable) {
        return borrow_reference<Target>(type_traits<Source>::borrow(source),
                                        requested_type, registered_type);
    } else {
        throw_not_convertible(requested_type, registered_type);
    }
}

template <typename Target, typename Source, typename Factory, typename Context>
Target& resolve_borrowed_reference(Factory& factory, Context& context,
                                   type_descriptor requested_type,
                                   type_descriptor registered_type) {
    return borrow_reference<Target>(resolve_source(factory, context),
                                    requested_type, registered_type);
}

template <typename Target, typename Source, typename Factory, typename Context>
Target* resolve_borrowed_pointer(Factory& factory, Context& context,
                                 type_descriptor requested_type,
                                 type_descriptor registered_type) {
    return std::addressof(resolve_borrowed_reference<Target, Source>(
        factory, context, requested_type, registered_type));
}

template <typename Target, typename Source, typename Factory, typename Context>
Target& resolve_handle_or_borrow(Factory& factory, Context& context,
                                 type_descriptor requested_type,
                                 type_descriptor registered_type) {
    if constexpr (is_handle_conversion_usable_v<Target, Source>) {
        return type_traits<Source>::template resolve_type<Target>(
            factory, context, requested_type, registered_type);
    } else {
        return resolve_borrowed_reference<Target, Source>(
            factory, context, requested_type, registered_type);
    }
}

template <typename Target, typename Source, typename Factory, typename Context>
Target* resolve_handle_or_borrow_pointer(Factory& factory, Context& context,
                                         type_descriptor requested_type,
                                         type_descriptor registered_type) {
    if constexpr (is_handle_conversion_usable_v<Target, Source>) {
        return std::addressof(
            type_traits<Source>::template resolve_type<Target>(
                factory, context, requested_type, registered_type));
    } else {
        return resolve_borrowed_pointer<Target, Source>(
            factory, context, requested_type, registered_type);
    }
}

template <typename Target, typename Source, typename Factory, typename Context>
Target& resolve_exact_wrapper_reference(Factory& factory, Context& context,
                                        type_descriptor requested_type,
                                        type_descriptor registered_type) {
    if constexpr (std::is_convertible_v<Source*, Target*>) {
        return static_cast<Target&>(resolve_source(factory, context));
    } else {
        throw_not_convertible(requested_type, registered_type);
    }
}

template <typename Target, typename Source, typename Factory, typename Context>
Target* resolve_exact_wrapper_pointer(Factory& factory, Context& context,
                                      type_descriptor requested_type,
                                      type_descriptor registered_type) {
    return std::addressof(resolve_exact_wrapper_reference<Target, Source>(
        factory, context, requested_type, registered_type));
}

template <typename Target, typename Sum>
Target extract_alternative_type_value(Sum&& sum, type_descriptor requested_type,
                                      type_descriptor registered_type) {
    using selected_type = std::remove_const_t<Target>;
    using alternative_type = std::remove_cv_t<std::remove_reference_t<Sum>>;

    if (auto* value =
            alternative_type_traits<alternative_type>::template get<selected_type>(
                sum)) {
        return Target(std::move(*value));
    }

    throw_not_convertible(requested_type, registered_type);
}

template <typename Target, typename Sum>
Target& resolve_alternative_type_reference(Sum& sum,
                                           type_descriptor requested_type,
                                           type_descriptor registered_type) {
    using selected_type = std::remove_const_t<Target>;
    using alternative_type = std::remove_cv_t<std::remove_reference_t<Sum>>;

    if (auto* value =
            alternative_type_traits<alternative_type>::template get<selected_type>(
                sum)) {
        return *value;
    }

    throw_not_convertible(requested_type, registered_type);
}

template <typename Target, typename Sum>
Target* resolve_alternative_type_pointer(Sum& sum,
                                         type_descriptor requested_type,
                                         type_descriptor registered_type) {
    return std::addressof(
        resolve_alternative_type_reference<Target>(sum, requested_type,
                                                   registered_type));
}

template <typename Source>
using borrowed_value_type_t = std::remove_cv_t<std::remove_reference_t<
    decltype(type_traits<Source>::borrow(std::declval<Source&>()))>>;

template <typename Source, typename Target, typename = void>
struct is_borrowed_alternative_type_alternative : std::false_type {};

template <typename Source, typename Target, typename = void>
struct is_borrowed_alternative_type : std::false_type {};

template <typename Source, typename Target>
struct is_borrowed_alternative_type<
    Source, Target,
    std::enable_if_t<type_traits<Source>::enabled &&
                     type_traits<Source>::is_value_borrowable &&
                     is_alternative_type_v<borrowed_value_type_t<Source>>>>
    : std::bool_constant<
          std::is_same_v<std::remove_cv_t<Target>, borrowed_value_type_t<Source>>> {};

template <typename Source, typename Target>
struct is_borrowed_alternative_type_alternative<
    Source, Target,
    std::enable_if_t<type_traits<Source>::enabled &&
                     type_traits<Source>::is_value_borrowable &&
                     is_alternative_type_v<borrowed_value_type_t<Source>>>>
    : std::bool_constant<
          !std::is_same_v<std::remove_cv_t<Target>, borrowed_value_type_t<Source>> &&
          (alternative_type_count<borrowed_value_type_t<Source>,
                                  std::remove_cv_t<Target>>::value == 1)> {};

template <typename Source, typename Target>
inline constexpr bool is_borrowed_alternative_type_v =
    is_borrowed_alternative_type<Source, Target>::value;

template <typename Source, typename Target>
inline constexpr bool is_borrowed_alternative_type_alternative_v =
    is_borrowed_alternative_type_alternative<Source, Target>::value;

template <typename T, typename = void>
struct factory_result_has_value_type : std::false_type {};

template <typename T>
struct factory_result_has_value_type<T, std::void_t<typename T::value_type>>
    : std::true_type {};

template <typename T> constexpr bool factory_result_copies_from_storage() {
    if constexpr (type_traits<T>::enabled && is_pointer_like_type_v<T> &&
                  std::is_copy_constructible_v<T>) {
        return true;
    } else if constexpr (factory_result_has_value_type<T>::value) {
        return std::is_copy_constructible_v<typename T::value_type>;
    } else {
        return std::is_copy_constructible_v<T>;
    }
}

template <typename ServiceType>
decltype(auto) convert_runtime_value(void* ptr) {
    if constexpr (factory_result_copies_from_storage<ServiceType>()) {
        return *static_cast<ServiceType*>(ptr);
    } else {
        return std::move(*static_cast<ServiceType*>(ptr));
    }
}

template <typename ServiceType>
decltype(auto) convert_runtime_lvalue_reference(void* ptr) {
    return *static_cast<std::remove_reference_t<ServiceType>*>(ptr);
}

template <typename ServiceType>
decltype(auto) convert_runtime_rvalue_reference(void* ptr) {
    return std::move(*static_cast<std::remove_reference_t<ServiceType>*>(ptr));
}

template <typename ServiceType>
decltype(auto) convert_runtime_pointer(void* ptr) {
    return static_cast<ServiceType>(ptr);
}

template <typename ServiceType>
decltype(auto) unreachable_runtime_conversion() {
    using value_type = std::remove_reference_t<ServiceType>;

    assert(false);

    if constexpr (std::is_lvalue_reference_v<ServiceType>) {
        return *static_cast<value_type*>(nullptr);
    } else if constexpr (std::is_rvalue_reference_v<ServiceType>) {
        return std::move(*static_cast<value_type*>(nullptr));
    } else if constexpr (std::is_pointer_v<ServiceType>) {
        return static_cast<ServiceType>(nullptr);
    } else {
        return std::move(*static_cast<value_type*>(nullptr));
    }
}

template <typename TargetType>
decltype(auto) unreachable_conversion() {
    using value_type = std::remove_reference_t<TargetType>;

    assert(false);

    if constexpr (std::is_lvalue_reference_v<TargetType>) {
        return *static_cast<value_type*>(nullptr);
    } else if constexpr (std::is_rvalue_reference_v<TargetType>) {
        return std::move(*static_cast<value_type*>(nullptr));
    } else if constexpr (std::is_pointer_v<TargetType>) {
        return static_cast<TargetType>(nullptr);
    } else {
        return std::move(*static_cast<value_type*>(nullptr));
    }
}

template <typename T> struct unsupported_unique_array_conversion : std::false_type {};

template <typename Array, typename Deleter>
struct unsupported_unique_array_conversion<std::unique_ptr<Array, Deleter>>
    : std::bool_constant<(std::rank_v<Array> > 1) &&
                         (std::extent_v<Array, 0> != 0)> {};

template <typename T> struct unsupported_shared_array_conversion : std::false_type {};

template <typename Array>
struct unsupported_shared_array_conversion<std::shared_ptr<Array>>
    : std::bool_constant<(std::rank_v<Array> > 1) &&
                         (std::extent_v<Array, 0> != 0)> {};

template <typename Conversion, typename Factory, typename Context>
decltype(auto) execute_direct_conversion(Factory& factory, Context& context,
                                         type_descriptor requested_type,
                                         type_descriptor registered_type) {
    using storage_tag = typename Conversion::storage_tag;
    using target_type = typename Conversion::target_type;
    using source_type = typename Conversion::source_type;

    if constexpr (std::is_same_v<storage_tag, unique> &&
                  !std::is_pointer_v<source_type> &&
                  !is_alternative_type_v<source_type>) {
        return static_cast<target_type>(resolve_source(factory, context));
    } else if constexpr (std::is_same_v<storage_tag, unique> &&
                         std::is_pointer_v<source_type> &&
                         std::is_pointer_v<target_type> &&
                         !std::is_array_v<std::remove_pointer_t<target_type>>) {
        return static_cast<target_type>(resolve_source(factory, context));
    } else if constexpr (
        std::is_same_v<storage_tag, unique> && std::is_pointer_v<source_type> &&
        is_pointer_like_type_v<target_type> &&
        has_type_from_pointer_v<target_type, source_type>) {
        return type_traits<target_type>::from_pointer(
            resolve_source(factory, context));
    } else if constexpr (
        std::is_same_v<storage_tag, unique> && std::is_pointer_v<source_type> &&
        unsupported_unique_array_conversion<target_type>::value) {
        throw_not_convertible(requested_type, registered_type);
    } else if constexpr (
        std::is_same_v<storage_tag, unique> && std::is_pointer_v<source_type> &&
        unsupported_shared_array_conversion<target_type>::value) {
        throw_not_convertible(requested_type, registered_type);
    } else if constexpr (!std::is_same_v<storage_tag, unique> &&
                         std::is_pointer_v<source_type> &&
                         std::is_pointer_v<target_type> &&
                         !std::is_array_v<std::remove_pointer_t<target_type>> &&
                         !is_alternative_type_v<source_type>) {
        return static_cast<target_type>(resolve_source(factory, context));
    } else if constexpr (!is_pointer_like_type_v<target_type> &&
                         !std::is_pointer_v<target_type> &&
                         !std::is_array_v<target_type> &&
                         std::is_pointer_v<source_type> &&
                         !is_alternative_type_v<source_type>) {
        return static_cast<target_type&>(dereference_source(factory, context));
    } else {
        throw_not_convertible(requested_type, registered_type);
        return unreachable_conversion<target_type>();
    }
}

template <typename Conversion>
using conversion_target_t = typename Conversion::target_type;

template <typename Conversion>
using conversion_source_t = typename Conversion::source_type;

template <typename Conversion>
using conversion_source_value_t =
    std::remove_reference_t<conversion_source_t<Conversion>>;

template <typename Conversion>
using conversion_target_value_t =
    std::remove_pointer_t<conversion_target_t<Conversion>>;

template <runtime_conversion_kind ConversionKind, typename ServiceType>
ServiceType convert_runtime_plan_case(void* ptr) {
    if constexpr (ConversionKind == runtime_conversion_kind::value_copy_or_move ||
                  ConversionKind ==
                      runtime_conversion_kind::copy_or_move_from_lvalue_reference ||
                  ConversionKind ==
                      runtime_conversion_kind::copy_or_move_from_rvalue_reference ||
                  ConversionKind ==
                      runtime_conversion_kind::dereference_pointer_to_value) {
        if constexpr (!std::is_reference_v<ServiceType> &&
                      !std::is_pointer_v<ServiceType>) {
            return convert_runtime_value<ServiceType>(ptr);
        }
    } else if constexpr (
        ConversionKind == runtime_conversion_kind::lvalue_reference ||
        ConversionKind ==
            runtime_conversion_kind::dereference_pointer_to_lvalue_reference) {
        if constexpr (std::is_lvalue_reference_v<ServiceType>) {
            return convert_runtime_lvalue_reference<ServiceType>(ptr);
        }
    } else if constexpr (
        ConversionKind == runtime_conversion_kind::rvalue_reference ||
        ConversionKind ==
            runtime_conversion_kind::dereference_pointer_to_rvalue_reference) {
        if constexpr (std::is_rvalue_reference_v<ServiceType>) {
            return convert_runtime_rvalue_reference<ServiceType>(ptr);
        }
    } else if constexpr (
        ConversionKind == runtime_conversion_kind::pointer ||
        ConversionKind == runtime_conversion_kind::address_of_lvalue_reference) {
        if constexpr (std::is_pointer_v<ServiceType>) {
            return convert_runtime_pointer<ServiceType>(ptr);
        }
    }

    return unreachable_runtime_conversion<ServiceType>();
}

template <typename Request>
typename Request::service_type convert_request_result(void* ptr, Request) {
    using service_type = typename Request::service_type;
    using request_kind = typename Request::request_kind;

    if constexpr (std::is_same_v<request_kind, ir::value_request>) {
        return convert_runtime_plan_case<
            runtime_conversion_kind::value_copy_or_move, service_type>(ptr);
    } else if constexpr (std::is_same_v<request_kind,
                                        ir::lvalue_reference_request>) {
        return convert_runtime_plan_case<
            runtime_conversion_kind::lvalue_reference, service_type>(ptr);
    } else if constexpr (std::is_same_v<request_kind,
                                        ir::rvalue_reference_request>) {
        return convert_runtime_plan_case<
            runtime_conversion_kind::rvalue_reference, service_type>(ptr);
    } else if constexpr (std::is_same_v<request_kind, ir::pointer_request>) {
        return convert_runtime_plan_case<runtime_conversion_kind::pointer,
                                         service_type>(ptr);
    } else {
        return unreachable_runtime_conversion<service_type>();
    }
}

template <typename Plan>
using runtime_plan_service_type_t = typename Plan::request_type::service_type;

template <typename Plan>
typename Plan::request_type::service_type convert_runtime_plan(
    void* ptr, runtime_conversion_kind conversion_kind, Plan) {
    switch (conversion_kind) {
    case runtime_conversion_kind::value_copy_or_move:
        return convert_runtime_plan_case<
            runtime_conversion_kind::value_copy_or_move,
            runtime_plan_service_type_t<Plan>>(ptr);
    case runtime_conversion_kind::lvalue_reference:
        return convert_runtime_plan_case<runtime_conversion_kind::lvalue_reference,
                                         runtime_plan_service_type_t<Plan>>(ptr);
    case runtime_conversion_kind::rvalue_reference:
        return convert_runtime_plan_case<runtime_conversion_kind::rvalue_reference,
                                         runtime_plan_service_type_t<Plan>>(ptr);
    case runtime_conversion_kind::pointer:
        return convert_runtime_plan_case<runtime_conversion_kind::pointer,
                                         runtime_plan_service_type_t<Plan>>(ptr);
    case runtime_conversion_kind::copy_or_move_from_lvalue_reference:
        return convert_runtime_plan_case<
            runtime_conversion_kind::copy_or_move_from_lvalue_reference,
            runtime_plan_service_type_t<Plan>>(ptr);
    case runtime_conversion_kind::copy_or_move_from_rvalue_reference:
        return convert_runtime_plan_case<
            runtime_conversion_kind::copy_or_move_from_rvalue_reference,
            runtime_plan_service_type_t<Plan>>(ptr);
    case runtime_conversion_kind::dereference_pointer_to_value:
        return convert_runtime_plan_case<
            runtime_conversion_kind::dereference_pointer_to_value,
            runtime_plan_service_type_t<Plan>>(ptr);
    case runtime_conversion_kind::dereference_pointer_to_lvalue_reference:
        return convert_runtime_plan_case<
            runtime_conversion_kind::dereference_pointer_to_lvalue_reference,
            runtime_plan_service_type_t<Plan>>(ptr);
    case runtime_conversion_kind::dereference_pointer_to_rvalue_reference:
        return convert_runtime_plan_case<
            runtime_conversion_kind::dereference_pointer_to_rvalue_reference,
            runtime_plan_service_type_t<Plan>>(ptr);
    case runtime_conversion_kind::address_of_lvalue_reference:
        return convert_runtime_plan_case<
            runtime_conversion_kind::address_of_lvalue_reference,
            runtime_plan_service_type_t<Plan>>(ptr);
    }

    assert(false);
    return unreachable_runtime_conversion<runtime_plan_service_type_t<Plan>>();
}

template <typename RTTI, typename Plan>
typename Plan::request_type::service_type convert_runtime_plan(
    void* ptr, runtime_conversion_kind conversion_kind, Plan) {
    return convert_runtime_plan(ptr, conversion_kind, Plan{});
}

template <typename Plan, typename RuntimePlan>
typename Plan::request_type::service_type convert_runtime_plan(
    void* ptr, const RuntimePlan& runtime_plan, Plan) {
    return convert_runtime_plan(ptr, runtime_plan.conversion_kind, Plan{});
}

template <typename RTTI, typename Plan, typename RuntimePlan>
typename Plan::request_type::service_type convert_runtime_plan(
    void* ptr, const RuntimePlan& runtime_plan, Plan) {
    return convert_runtime_plan(ptr, runtime_plan, Plan{});
}

template <typename Conversion, typename = void> struct native_conversion_impl {
    template <typename Factory, typename Context>
    static decltype(auto) apply(Factory& factory, Context& context,
                                type_descriptor requested_type,
                                type_descriptor registered_type) {
        using target_type = conversion_target_t<Conversion>;
        using source_type = conversion_source_t<Conversion>;

        if constexpr (Conversion::kind ==
                      ir::conversion_kind::address_of_lvalue_reference) {
            return static_cast<target_type>(
                resolve_source_pointer<source_type>(factory, context));
        } else if constexpr (
            Conversion::kind ==
                ir::conversion_kind::dereference_pointer_to_value ||
            Conversion::kind ==
                ir::conversion_kind::dereference_pointer_to_lvalue_reference ||
            Conversion::kind ==
                ir::conversion_kind::dereference_pointer_to_rvalue_reference) {
            return static_cast<target_type&>(dereference_source(factory, context));
        } else if constexpr (Conversion::kind ==
                             ir::conversion_kind::pointer) {
            if constexpr (std::is_pointer_v<source_type>) {
                return static_cast<target_type>(
                    resolve_source(factory, context));
            } else {
                return static_cast<target_type>(
                    resolve_source_pointer<source_type>(factory, context));
            }
        } else if constexpr (
            Conversion::kind == ir::conversion_kind::value_copy_or_move ||
            Conversion::kind ==
                ir::conversion_kind::copy_or_move_from_lvalue_reference ||
            Conversion::kind ==
                ir::conversion_kind::copy_or_move_from_rvalue_reference ||
            Conversion::kind == ir::conversion_kind::lvalue_reference ||
            Conversion::kind == ir::conversion_kind::rvalue_reference) {
            if constexpr (std::is_lvalue_reference_v<source_type>) {
                return static_cast<target_type&>(resolve_source(factory, context));
            } else if constexpr (std::is_rvalue_reference_v<source_type> &&
                                 Conversion::kind ==
                                     ir::conversion_kind::rvalue_reference) {
                return static_cast<target_type&&>(resolve_source(factory, context));
            } else {
                return static_cast<target_type>(resolve_source(factory, context));
            }
        } else {
            assert(false);
            throw_not_convertible(requested_type, registered_type);
        }
    }
};

template <typename Conversion, typename = void> struct array_conversion_impl {
    template <typename Factory, typename Context>
    static decltype(auto) apply(Factory& factory, Context& context,
                                type_descriptor requested_type,
                                type_descriptor registered_type) {
        using target_type = conversion_target_t<Conversion>;
        using source_type = conversion_source_t<Conversion>;
        using source_value_type = conversion_source_value_t<Conversion>;

        if constexpr (std::is_array_v<target_type>) {
            if constexpr (std::is_pointer_v<source_type>) {
                using array_pointer = std::add_pointer_t<target_type>;
                return *reinterpret_cast<array_pointer>(
                    resolve_source(factory, context));
            } else {
                return resolve_source(factory, context);
            }
        } else if constexpr (std::is_pointer_v<target_type> &&
                             std::is_array_v<conversion_target_value_t<Conversion>>) {
            using target_array_type = conversion_target_value_t<Conversion>;

            if constexpr (std::is_pointer_v<source_type>) {
                return reinterpret_cast<target_type>(resolve_source(factory, context));
            } else if constexpr (std::is_same_v<
                                     std::remove_cv_t<source_value_type>,
                                     std::remove_cv_t<target_array_type>>) {
                return std::addressof(resolve_source(factory, context));
            } else {
                return std::addressof(resolve_source(factory, context)[0]);
            }
        } else if constexpr (std::is_pointer_v<target_type> &&
                             !type_traits<target_type>::enabled) {
            if constexpr (std::is_pointer_v<source_type>) {
                return resolve_source(factory, context);
            } else if constexpr (std::is_array_v<source_value_type>) {
                return std::addressof(resolve_source(factory, context)[0]);
            } else {
                assert(false);
                throw_not_convertible(requested_type, registered_type);
            }
        } else {
            return execute_direct_conversion<Conversion>(
                factory, context, requested_type, registered_type);
        }
    }
};

template <typename Conversion, typename = void>
struct alternative_conversion_impl {
    template <typename Factory, typename Context>
    static decltype(auto) apply(Factory& factory, Context& context,
                                type_descriptor requested_type,
                                type_descriptor registered_type) {
        using target_type = conversion_target_t<Conversion>;
        using source_type = conversion_source_t<Conversion>;
        using source_value_type = std::conditional_t<
            std::is_pointer_v<source_type>, std::remove_pointer_t<source_type>,
            conversion_source_value_t<Conversion>>;
        using source_alternative_type = std::remove_cv_t<source_value_type>;

        if constexpr (!std::is_pointer_v<target_type>) {
            if constexpr (std::is_pointer_v<source_type> ||
                          std::is_lvalue_reference_v<source_type>) {
                auto& source = resolve_source_value<source_type>(factory, context);
                if constexpr (std::is_same_v<std::remove_cv_t<target_type>,
                                             source_alternative_type>) {
                    return source;
                } else {
                    return resolve_alternative_type_reference<target_type>(
                        source, requested_type, registered_type);
                }
            } else {
                if constexpr (std::is_same_v<std::remove_cv_t<target_type>,
                                             source_alternative_type>) {
                    return static_cast<target_type>(
                        resolve_source(factory, context));
                } else {
                    auto&& source = resolve_source(factory, context);
                    return extract_alternative_type_value<target_type>(
                        std::forward<decltype(source)>(source), requested_type,
                        registered_type);
                }
            }
        } else {
            using target_value_type = conversion_target_value_t<Conversion>;

            if constexpr (std::is_pointer_v<source_type> ||
                          std::is_lvalue_reference_v<source_type>) {
                auto& source = resolve_source_value<source_type>(factory, context);
                if constexpr (std::is_same_v<std::remove_cv_t<target_value_type>,
                                             source_alternative_type>) {
                    return resolve_source_pointer<source_type>(factory, context);
                } else {
                    return resolve_alternative_type_pointer<target_value_type>(
                        source, requested_type, registered_type);
                }
            } else {
                return execute_direct_conversion<Conversion>(
                    factory, context, requested_type, registered_type);
            }
        }
    }
};

template <typename Conversion, typename = void> struct handle_conversion_impl {
    template <typename Factory, typename Context>
    static decltype(auto) apply(Factory& factory, Context& context,
                                type_descriptor requested_type,
                                type_descriptor registered_type) {
        using target_type = conversion_target_t<Conversion>;
        using source_type = conversion_source_t<Conversion>;

        if constexpr (std::is_pointer_v<source_type> &&
                      is_pointer_like_type_v<target_type> &&
                      has_type_from_pointer_v<target_type, source_type>) {
            return type_traits<target_type>::from_pointer(
                resolve_source(factory, context));
        } else if constexpr (std::is_lvalue_reference_v<source_type>) {
            using source_value_type = conversion_source_value_t<Conversion>;

            if constexpr (is_pointer_like_type_v<target_type> &&
                          is_pointer_like_type_v<source_value_type>) {
                return resolve_handle_or_borrow<target_type, source_value_type>(
                    factory, context, requested_type, registered_type);
            } else {
                return execute_direct_conversion<Conversion>(
                    factory, context, requested_type, registered_type);
            }
        } else {
            return execute_direct_conversion<Conversion>(
                factory, context, requested_type, registered_type);
        }
    }
};

template <typename Conversion, typename = void> struct borrow_conversion_impl {
    template <typename Factory, typename Context>
    static decltype(auto) apply(Factory& factory, Context& context,
                                type_descriptor requested_type,
                                type_descriptor registered_type) {
        using target_type = conversion_target_t<Conversion>;
        using source_type = conversion_source_t<Conversion>;

        if constexpr (std::is_lvalue_reference_v<source_type>) {
            using borrowed_source_type = conversion_source_value_t<Conversion>;

            if constexpr (!std::is_pointer_v<target_type> &&
                          detail::is_borrowed_alternative_type_v<
                              borrowed_source_type, target_type>) {
                auto& source = resolve_source(factory, context);
                return type_traits<borrowed_source_type>::borrow(source);
            } else if constexpr (
                !std::is_pointer_v<target_type> &&
                detail::is_borrowed_alternative_type_alternative_v<
                    borrowed_source_type, target_type>) {
                auto& source = resolve_source(factory, context);
                return resolve_alternative_type_reference<target_type>(
                    type_traits<borrowed_source_type>::borrow(source),
                    requested_type, registered_type);
            } else if constexpr (
                !type_traits<target_type>::enabled &&
                !std::is_pointer_v<target_type> &&
                type_traits<borrowed_source_type>::enabled &&
                type_traits<borrowed_source_type>::is_value_borrowable &&
                !detail::is_borrowed_alternative_type_v<borrowed_source_type,
                                                        target_type> &&
                !detail::is_borrowed_alternative_type_alternative_v<
                    borrowed_source_type, target_type>) {
                return resolve_borrowed_reference<target_type,
                                                  borrowed_source_type>(
                    factory, context, requested_type, registered_type);
            } else if constexpr (
                std::is_pointer_v<target_type> &&
                detail::is_borrowed_alternative_type_v<
                    borrowed_source_type, conversion_target_value_t<Conversion>>) {
                auto& source = resolve_source(factory, context);
                return std::addressof(static_cast<conversion_target_value_t<Conversion>&>(
                    type_traits<borrowed_source_type>::borrow(source)));
            } else if constexpr (
                std::is_pointer_v<target_type> &&
                detail::is_borrowed_alternative_type_alternative_v<
                    borrowed_source_type, conversion_target_value_t<Conversion>>) {
                auto& source = resolve_source(factory, context);
                return resolve_alternative_type_pointer<
                    conversion_target_value_t<Conversion>>(
                    type_traits<borrowed_source_type>::borrow(source),
                    requested_type, registered_type);
            } else if constexpr (
                std::is_pointer_v<target_type> &&
                !type_traits<conversion_target_value_t<Conversion>>::enabled &&
                type_traits<borrowed_source_type>::enabled &&
                type_traits<borrowed_source_type>::is_value_borrowable &&
                !detail::is_borrowed_alternative_type_v<
                    borrowed_source_type, conversion_target_value_t<Conversion>> &&
                !detail::is_borrowed_alternative_type_alternative_v<
                    borrowed_source_type, conversion_target_value_t<Conversion>>) {
                return resolve_borrowed_pointer<
                    conversion_target_value_t<Conversion>, borrowed_source_type>(
                    factory, context, requested_type, registered_type);
            } else {
                return execute_direct_conversion<Conversion>(
                    factory, context, requested_type, registered_type);
            }
        } else {
            return execute_direct_conversion<Conversion>(
                factory, context, requested_type, registered_type);
        }
    }
};

template <typename Conversion>
struct borrow_conversion_impl<
    Conversion,
    std::enable_if_t<
        std::is_lvalue_reference_v<conversion_source_t<Conversion>> &&
        !std::is_pointer_v<conversion_target_t<Conversion>> &&
        type_traits<conversion_target_t<Conversion>>::enabled &&
        type_traits<conversion_source_value_t<Conversion>>::enabled &&
        type_traits<conversion_source_value_t<Conversion>>::is_value_borrowable &&
        !is_pointer_like_type_v<conversion_target_t<Conversion>> &&
        is_same_handle_shape_v<conversion_target_t<Conversion>,
                               conversion_source_value_t<Conversion>>>> {
    template <typename Factory, typename Context>
    static decltype(auto) apply(Factory& factory, Context& context,
                                type_descriptor requested_type,
                                type_descriptor registered_type) {
        return resolve_exact_wrapper_reference<conversion_target_t<Conversion>,
                                               conversion_source_value_t<Conversion>>(
            factory, context, requested_type, registered_type);
    }
};

template <typename Conversion>
struct borrow_conversion_impl<
    Conversion,
    std::enable_if_t<
        std::is_lvalue_reference_v<conversion_source_t<Conversion>> &&
        std::is_pointer_v<conversion_target_t<Conversion>> &&
        type_traits<conversion_target_value_t<Conversion>>::enabled &&
        type_traits<conversion_source_value_t<Conversion>>::enabled &&
        type_traits<conversion_source_value_t<Conversion>>::is_value_borrowable &&
        !is_pointer_like_type_v<conversion_target_value_t<Conversion>> &&
        is_same_handle_shape_v<conversion_target_value_t<Conversion>,
                               conversion_source_value_t<Conversion>>>> {
    template <typename Factory, typename Context>
    static decltype(auto) apply(Factory& factory, Context& context,
                                type_descriptor requested_type,
                                type_descriptor registered_type) {
        return resolve_exact_wrapper_pointer<conversion_target_value_t<Conversion>,
                                             conversion_source_value_t<Conversion>>(
            factory, context, requested_type, registered_type);
    }
};

template <typename Conversion>
struct borrow_conversion_impl<
    Conversion,
    std::enable_if_t<
        std::is_lvalue_reference_v<conversion_source_t<Conversion>> &&
        std::is_pointer_v<conversion_target_t<Conversion>> &&
        is_pointer_like_type_v<conversion_target_value_t<Conversion>> &&
        is_pointer_like_type_v<conversion_source_value_t<Conversion>> &&
        type_traits<conversion_source_value_t<Conversion>>::is_value_borrowable>> {
    template <typename Factory, typename Context>
    static decltype(auto) apply(Factory& factory, Context& context,
                                type_descriptor requested_type,
                                type_descriptor registered_type) {
        return resolve_handle_or_borrow_pointer<
            conversion_target_value_t<Conversion>,
            conversion_source_value_t<Conversion>>(factory, context,
                                                   requested_type,
                                                   registered_type);
    }
};

template <typename Conversion, typename = void> struct wrapper_conversion_impl {
    template <typename Factory, typename Context>
    static decltype(auto) apply(Factory& factory, Context& context,
                                type_descriptor requested_type,
                                type_descriptor registered_type) {
        using target_type = conversion_target_t<Conversion>;
        using source_type = conversion_source_t<Conversion>;

        if constexpr (std::is_lvalue_reference_v<source_type>) {
            using source_value_type = conversion_source_value_t<Conversion>;

            if constexpr (std::is_pointer_v<target_type>) {
                using target_value_type = conversion_target_value_t<Conversion>;

                if constexpr (
                    !type_traits<target_value_type>::enabled &&
                    type_traits<source_value_type>::enabled &&
                    !type_traits<source_value_type>::is_value_borrowable) {
                    auto& source = resolve_source(factory, context);
                    if constexpr (std::is_convertible_v<
                                      decltype(type_traits<source_value_type>::get(
                                          source)),
                                      target_type>) {
                        return type_traits<source_value_type>::get(source);
                    } else {
                        throw_not_convertible(requested_type, registered_type);
                    }
                } else {
                    return execute_direct_conversion<Conversion>(
                        factory, context, requested_type, registered_type);
                }
            } else {
                return execute_direct_conversion<Conversion>(
                    factory, context, requested_type, registered_type);
            }
        } else {
            return execute_direct_conversion<Conversion>(
                factory, context, requested_type, registered_type);
        }
    }
};

template <typename Conversion>
struct wrapper_conversion_impl<
    Conversion,
    std::enable_if_t<
        std::is_lvalue_reference_v<conversion_source_t<Conversion>> &&
        !std::is_pointer_v<conversion_target_t<Conversion>> &&
        type_traits<conversion_target_t<Conversion>>::enabled &&
        type_traits<conversion_source_value_t<Conversion>>::enabled &&
        !type_traits<conversion_source_value_t<Conversion>>::is_value_borrowable &&
        !is_pointer_like_type_v<conversion_target_t<Conversion>> &&
        is_same_handle_shape_v<conversion_target_t<Conversion>,
                               conversion_source_value_t<Conversion>>>> {
    template <typename Factory, typename Context>
    static decltype(auto) apply(Factory& factory, Context& context,
                                type_descriptor requested_type,
                                type_descriptor registered_type) {
        return resolve_exact_wrapper_reference<conversion_target_t<Conversion>,
                                               conversion_source_value_t<Conversion>>(
            factory, context, requested_type, registered_type);
    }
};

template <typename Conversion>
struct wrapper_conversion_impl<
    Conversion,
    std::enable_if_t<
        std::is_lvalue_reference_v<conversion_source_t<Conversion>> &&
        std::is_pointer_v<conversion_target_t<Conversion>> &&
        type_traits<conversion_target_value_t<Conversion>>::enabled &&
        type_traits<conversion_source_value_t<Conversion>>::enabled &&
        !type_traits<conversion_source_value_t<Conversion>>::is_value_borrowable &&
        !is_pointer_like_type_v<conversion_target_value_t<Conversion>> &&
        is_same_handle_shape_v<conversion_target_value_t<Conversion>,
                               conversion_source_value_t<Conversion>>>> {
    template <typename Factory, typename Context>
    static decltype(auto) apply(Factory& factory, Context& context,
                                type_descriptor requested_type,
                                type_descriptor registered_type) {
        return resolve_exact_wrapper_pointer<conversion_target_value_t<Conversion>,
                                             conversion_source_value_t<Conversion>>(
            factory, context, requested_type, registered_type);
    }
};

template <typename Conversion>
struct wrapper_conversion_impl<
    Conversion,
    std::enable_if_t<
        std::is_lvalue_reference_v<conversion_source_t<Conversion>> &&
        std::is_pointer_v<conversion_target_t<Conversion>> &&
        is_pointer_like_type_v<conversion_target_value_t<Conversion>> &&
        is_pointer_like_type_v<conversion_source_value_t<Conversion>> &&
        !type_traits<conversion_source_value_t<Conversion>>::is_value_borrowable>> {
    template <typename Factory, typename Context>
    static decltype(auto) apply(Factory& factory, Context& context,
                                type_descriptor requested_type,
                                type_descriptor registered_type) {
        return resolve_handle_or_borrow_pointer<
            conversion_target_value_t<Conversion>,
            conversion_source_value_t<Conversion>>(factory, context,
                                                   requested_type,
                                                   registered_type);
    }
};

template <ir::conversion_family Family> struct conversion_executor;

template <> struct conversion_executor<ir::conversion_family::native> {
    template <typename Conversion, typename Factory, typename Context>
    static decltype(auto) apply(Factory& factory, Context& context,
                                type_descriptor requested_type,
                                type_descriptor registered_type) {
        return native_conversion_impl<Conversion>::apply(
            factory, context, requested_type, registered_type);
    }
};

template <> struct conversion_executor<ir::conversion_family::array> {
    template <typename Conversion, typename Factory, typename Context>
    static decltype(auto) apply(Factory& factory, Context& context,
                                type_descriptor requested_type,
                                type_descriptor registered_type) {
        return array_conversion_impl<Conversion>::apply(
            factory, context, requested_type, registered_type);
    }
};

template <> struct conversion_executor<ir::conversion_family::alternative> {
    template <typename Conversion, typename Factory, typename Context>
    static decltype(auto) apply(Factory& factory, Context& context,
                                type_descriptor requested_type,
                                type_descriptor registered_type) {
        return alternative_conversion_impl<Conversion>::apply(
            factory, context, requested_type, registered_type);
    }
};

template <> struct conversion_executor<ir::conversion_family::handle> {
    template <typename Conversion, typename Factory, typename Context>
    static decltype(auto) apply(Factory& factory, Context& context,
                                type_descriptor requested_type,
                                type_descriptor registered_type) {
        return handle_conversion_impl<Conversion>::apply(
            factory, context, requested_type, registered_type);
    }
};

template <> struct conversion_executor<ir::conversion_family::borrow> {
    template <typename Conversion, typename Factory, typename Context>
    static decltype(auto) apply(Factory& factory, Context& context,
                                type_descriptor requested_type,
                                type_descriptor registered_type) {
        return borrow_conversion_impl<Conversion>::apply(
            factory, context, requested_type, registered_type);
    }
};

template <> struct conversion_executor<ir::conversion_family::wrapper> {
    template <typename Conversion, typename Factory, typename Context>
    static decltype(auto) apply(Factory& factory, Context& context,
                                type_descriptor requested_type,
                                type_descriptor registered_type) {
        return wrapper_conversion_impl<Conversion>::apply(
            factory, context, requested_type, registered_type);
    }
};

template <typename Conversion, typename Factory, typename Context>
decltype(auto) execute_conversion(Factory& factory, Context& context,
                                  type_descriptor requested_type,
                                  type_descriptor registered_type) {
    return conversion_executor<Conversion::family>::template apply<Conversion>(
        factory, context, requested_type, registered_type);
}
} // namespace detail

// This file is the shared conversion core for both runtime-plan execution and
// the remaining request-shaped paths. The last low-level identity and raw
// pointer cases are handled by execute_direct_conversion() above.

} // namespace dingo

#ifdef _MSC_VER
#pragma warning(pop)
#endif
