//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/config.h>

#include <dingo/exceptions.h>
#include <dingo/storage/materialized_source.h>
#include <dingo/type/rebind_type.h>
#include <dingo/type/type_conversion_traits.h>
#include <dingo/type/type_list.h>
#include <dingo/type/type_traits.h>

#include <cassert>
#include <cstddef>
#include <memory>
#include <utility>

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4702)
#endif

namespace dingo {
namespace detail {
template <typename Target, typename Source>
inline constexpr bool is_same_handle_shape_v =
    std::is_same_v<rebind_leaf_t<Target, runtime_type>,
                   rebind_leaf_t<Source, runtime_type>>;

template <typename Target, typename Source>
inline constexpr bool is_rebindable_handle_v =
    // Identical handle types should reuse the already materialized source
    // instead of re-entering factory.resolve<Target>(), which can re-trigger
    // shared recursion handling while the source instance is still resolving.
    !std::is_same_v<Target, Source> && is_same_handle_shape_v<Target, Source> &&
    type_traits<Source>::enabled && type_traits<Source>::is_pointer_like &&
    type_traits<Source>::template is_rebindable<Target>;

template <typename SourceCapability>
using materialized_source_traits_t = materialized_source_traits<
    std::remove_cv_t<std::remove_reference_t<SourceCapability>>>;

template <typename SourceCapability>
using source_value_type_t =
    typename materialized_source_traits_t<SourceCapability>::value_type;

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
        throw make_type_not_convertible_exception(requested_type,
                                                  registered_type);
    }
}

template <typename Target, typename Source, typename Factory, typename Context,
          typename SourceCapability>
Target& resolve_handle_or_borrow(Factory& factory, Context& context,
                                 SourceCapability&& source,
                                 type_descriptor requested_type,
                                 type_descriptor registered_type) {
    if constexpr (is_rebindable_handle_v<Target, Source>) {
        return type_traits<Source>::template resolve_type<Target>(
            factory, context, requested_type, registered_type);
    } else {
        return borrow_reference<Target>(
            materialized_source_traits_t<SourceCapability>::value(
                std::forward<SourceCapability>(source)),
            requested_type, registered_type);
    }
}

template <typename Target, typename Source, typename Factory, typename Context,
          typename SourceCapability>
Target* resolve_handle_or_borrow_pointer(Factory& factory, Context& context,
                                         SourceCapability&& source,
                                         type_descriptor requested_type,
                                         type_descriptor registered_type) {
    if constexpr (is_rebindable_handle_v<Target, Source>) {
        return std::addressof(
            type_traits<Source>::template resolve_type<Target>(
                factory, context, requested_type, registered_type));
    } else {
        return std::addressof(borrow_reference<Target>(
            materialized_source_traits_t<SourceCapability>::value(
                std::forward<SourceCapability>(source)),
            requested_type, registered_type));
    }
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

    throw make_type_not_convertible_exception(requested_type, registered_type);
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

    throw make_type_not_convertible_exception(requested_type, registered_type);
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
          std::is_same_v<std::remove_cv_t<Target>, borrowed_value_type_t<Source>>> {
};

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
} // namespace detail

// TODO: this file is really terrible, I need to look at how to deduplicate it

template <typename Target, typename SourceCapability, typename = void>
struct type_conversion {
    template <typename Factory, typename Context, typename Source>
    static void* apply(Factory&, Context&, Source&&,
                       type_descriptor requested_type,
                       type_descriptor registered_type);
};

template <typename Target, typename Source>
struct type_conversion<Target, detail::rvalue_source<Source>,
                       std::enable_if_t<!std::is_pointer_v<Source> &&
                                        !is_alternative_type_v<Source>>> {
    template <typename Factory, typename Context, typename SourceCapability>
    static decltype(auto) apply(Factory&, Context&, SourceCapability&& source,
                                type_descriptor, type_descriptor) {
        return std::move(*source.get_ptr());
    }
};

template <typename Target, typename Source>
struct type_conversion<
    Target, detail::rvalue_source<Source>,
    std::enable_if_t<
        is_alternative_type_v<Source> &&
        !std::is_pointer_v<Target> &&
        !std::is_same_v<std::remove_cv_t<Target>, std::remove_cv_t<Source>> &&
        (detail::alternative_type_count<Source,
                                        std::remove_cv_t<Target>>::value == 1)>> {
    template <typename Factory, typename Context, typename SourceCapability>
    static Target apply(Factory&, Context&, SourceCapability&& source,
                        type_descriptor requested_type,
                        type_descriptor registered_type) {
        auto&& value =
            detail::materialized_source_traits_t<SourceCapability>::value(
                std::forward<SourceCapability>(source));
        return detail::extract_alternative_type_value<Target>(
            std::move(value), requested_type, registered_type);
    }
};

template <typename Target, typename Source>
struct type_conversion<
    Target, detail::rvalue_source<Source>,
    std::enable_if_t<is_alternative_type_v<Source> &&
                     std::is_same_v<std::remove_cv_t<Target>,
                                    std::remove_cv_t<Source>>>> {
    template <typename Factory, typename Context, typename SourceCapability>
    static decltype(auto) apply(Factory&, Context&, SourceCapability&& source,
                                type_descriptor, type_descriptor) {
        return std::move(*source.get_ptr());
    }
};

template <typename Target, typename Source>
struct type_conversion<
    Target, detail::rvalue_source<Source>,
    std::enable_if_t<
        is_alternative_type_v<Source> &&
        !std::is_pointer_v<Target> &&
        !std::is_same_v<std::remove_cv_t<Target>, std::remove_cv_t<Source>> &&
        (detail::alternative_type_count<Source,
                                        std::remove_cv_t<Target>>::value != 1)>> {
    template <typename Factory, typename Context, typename SourceCapability>
    static Target apply(Factory&, Context&, SourceCapability&&,
                        type_descriptor requested_type,
                        type_descriptor registered_type) {
        throw detail::make_type_not_convertible_exception(requested_type,
                                                          registered_type);
    }
};

template <typename Target, typename Source>
struct type_conversion<
    Target*, detail::rvalue_source<Source*>,
    std::enable_if_t<std::is_array_v<Target> &&
                     std::is_same_v<std::remove_cv_t<Source>,
                                    std::remove_cv_t<Target>>>> {
    template <typename Factory, typename Context, typename SourceCapability>
    static Target* apply(Factory&, Context&, SourceCapability&& source,
                         type_descriptor, type_descriptor) {
        return detail::materialized_source_traits_t<SourceCapability>::value(
            std::forward<SourceCapability>(source));
    }
};

template <typename Target, size_t N, typename Source>
struct type_conversion<
    Target (*)[N], detail::rvalue_source<Source*>,
    std::enable_if_t<std::is_same_v<std::remove_cv_t<Source>,
                                    std::remove_cv_t<Target>>>> {
    template <typename Factory, typename Context, typename SourceCapability>
    static Target (*apply(Factory&, Context&, SourceCapability&& source,
                          type_descriptor, type_descriptor))[N] {
        return reinterpret_cast<Target(*)[N]>(
            detail::materialized_source_traits_t<SourceCapability>::value(
                std::forward<SourceCapability>(source)));
    }
};

template <typename Target, typename Source>
struct type_conversion<Target*, detail::rvalue_source<Source*>,
                       std::enable_if_t<!std::is_array_v<Target>>> {
    template <typename Factory, typename Context, typename SourceCapability>
    static Target* apply(Factory&, Context&, SourceCapability&& source,
                         type_descriptor, type_descriptor) {
        return detail::materialized_source_traits_t<SourceCapability>::value(
            std::forward<SourceCapability>(source));
    }
};

template <typename Target, typename Source>
struct type_conversion<
    Target, detail::rvalue_source<Source*>,
    std::enable_if_t<is_pointer_like_type_v<Target> &&
                     detail::has_type_from_pointer_v<Target, Source*>>> {
    template <typename Factory, typename Context, typename SourceCapability>
    static Target apply(Factory&, Context&, SourceCapability&& source,
                        type_descriptor, type_descriptor) {
        return type_traits<Target>::from_pointer(
            detail::materialized_source_traits_t<SourceCapability>::value(
                std::forward<SourceCapability>(source)));
    }
};

template <typename Array, typename Source, typename Deleter>
struct type_conversion<
    std::unique_ptr<Array, Deleter>, detail::rvalue_source<Source*>,
    std::enable_if_t<(std::rank_v<Array> > 1) &&
                     (std::extent_v<Array, 0> != 0)>> {
    template <typename Factory, typename Context, typename SourceCapability>
    static std::unique_ptr<Array, Deleter>
    apply(Factory&, Context&, SourceCapability&&, type_descriptor requested_type,
          type_descriptor registered_type) {
        throw detail::make_type_not_convertible_exception(requested_type,
                                                          registered_type);
    }
};

template <typename Array, typename Source>
struct type_conversion<std::shared_ptr<Array>, detail::rvalue_source<Source*>,
                       std::enable_if_t<(std::rank_v<Array> > 1) &&
                                        (std::extent_v<Array, 0> != 0)>> {
    template <typename Factory, typename Context, typename SourceCapability>
    static std::shared_ptr<Array>
    apply(Factory&, Context&, SourceCapability&&, type_descriptor requested_type,
          type_descriptor registered_type) {
        throw detail::make_type_not_convertible_exception(requested_type,
                                                          registered_type);
    }
};

template <typename Target, typename SourceCapability>
struct type_conversion<
    Target, SourceCapability,
    std::enable_if_t<
        detail::materialized_source_traits_t<SourceCapability>::reference_like &&
        is_alternative_type_v<detail::source_value_type_t<SourceCapability>> &&
        !std::is_pointer_v<Target> &&
        !std::is_same_v<
            std::remove_cv_t<Target>,
            std::remove_cv_t<detail::source_value_type_t<SourceCapability>>> &&
        (detail::alternative_type_count<
             detail::source_value_type_t<SourceCapability>,
             std::remove_cv_t<Target>>::value == 1)>> {
    template <typename Factory, typename Context, typename Capability>
    static Target& apply(Factory&, Context&, Capability&& source,
                         type_descriptor requested_type,
                         type_descriptor registered_type) {
        return detail::resolve_alternative_type_reference<Target>(
            detail::materialized_source_traits_t<Capability>::reference(source),
            requested_type, registered_type);
    }
};

template <typename Target, typename SourceCapability>
struct type_conversion<
    Target, SourceCapability,
    std::enable_if_t<
        detail::materialized_source_traits_t<SourceCapability>::reference_like &&
        is_alternative_type_v<detail::source_value_type_t<SourceCapability>> &&
        std::is_same_v<
            std::remove_cv_t<Target>,
            std::remove_cv_t<detail::source_value_type_t<SourceCapability>>>>> {
    template <typename Factory, typename Context, typename Capability>
    static Target& apply(Factory&, Context&, Capability&& source,
                         type_descriptor, type_descriptor) {
        return detail::materialized_source_traits_t<Capability>::reference(source);
    }
};

template <typename Target, typename SourceCapability>
struct type_conversion<
    Target, SourceCapability,
    std::enable_if_t<
        detail::materialized_source_traits_t<SourceCapability>::reference_like &&
        is_alternative_type_v<detail::source_value_type_t<SourceCapability>> &&
        !std::is_pointer_v<Target> &&
        !std::is_same_v<
            std::remove_cv_t<Target>,
            std::remove_cv_t<detail::source_value_type_t<SourceCapability>>> &&
        (detail::alternative_type_count<
             detail::source_value_type_t<SourceCapability>,
             std::remove_cv_t<Target>>::value != 1)>> {
    template <typename Factory, typename Context, typename Capability>
    static Target& apply(Factory&, Context&, Capability&&,
                         type_descriptor requested_type,
                         type_descriptor registered_type) {
        throw detail::make_type_not_convertible_exception(requested_type,
                                                          registered_type);
    }
};

template <typename Target, typename Source>
struct type_conversion<
    Target, detail::lvalue_source<Source>,
    std::enable_if_t<!type_traits<Source>::enabled && !std::is_pointer_v<Target> &&
                     !is_alternative_type_v<Source>>> {
    template <typename Factory, typename Context, typename SourceCapability>
    static Target& apply(Factory&, Context&, SourceCapability&& source,
                         type_descriptor, type_descriptor) {
        return detail::materialized_source_traits_t<SourceCapability>::reference(
            source);
    }
};

template <typename Target, typename SourceCapability>
struct type_conversion<
    Target*, SourceCapability,
    std::enable_if_t<
        detail::materialized_source_traits_t<SourceCapability>::reference_like &&
        is_alternative_type_v<detail::source_value_type_t<SourceCapability>> &&
        !std::is_same_v<
            std::remove_cv_t<Target>,
            std::remove_cv_t<detail::source_value_type_t<SourceCapability>>> &&
        (detail::alternative_type_count<
             detail::source_value_type_t<SourceCapability>,
             std::remove_cv_t<Target>>::value == 1)>> {
    template <typename Factory, typename Context, typename Capability>
    static Target* apply(Factory&, Context&, Capability&& source,
                         type_descriptor requested_type,
                         type_descriptor registered_type) {
        return detail::resolve_alternative_type_pointer<Target>(
            detail::materialized_source_traits_t<Capability>::reference(source),
            requested_type, registered_type);
    }
};

template <typename Target, typename SourceCapability>
struct type_conversion<
    Target*, SourceCapability,
    std::enable_if_t<
        detail::materialized_source_traits_t<SourceCapability>::reference_like &&
        is_alternative_type_v<detail::source_value_type_t<SourceCapability>> &&
        std::is_same_v<
            std::remove_cv_t<Target>,
            std::remove_cv_t<detail::source_value_type_t<SourceCapability>>>>> {
    template <typename Factory, typename Context, typename Capability>
    static Target* apply(Factory&, Context&, Capability&& source,
                         type_descriptor, type_descriptor) {
        return std::addressof(
            detail::materialized_source_traits_t<Capability>::reference(source));
    }
};

template <typename Target, typename SourceCapability>
struct type_conversion<
    Target*, SourceCapability,
    std::enable_if_t<
        detail::materialized_source_traits_t<SourceCapability>::reference_like &&
        is_alternative_type_v<detail::source_value_type_t<SourceCapability>> &&
        !std::is_same_v<
            std::remove_cv_t<Target>,
            std::remove_cv_t<detail::source_value_type_t<SourceCapability>>> &&
        (detail::alternative_type_count<
             detail::source_value_type_t<SourceCapability>,
             std::remove_cv_t<Target>>::value != 1)>> {
    template <typename Factory, typename Context, typename Capability>
    static Target* apply(Factory&, Context&, Capability&&,
                         type_descriptor requested_type,
                         type_descriptor registered_type) {
        throw detail::make_type_not_convertible_exception(requested_type,
                                                          registered_type);
    }
};

template <typename Target, typename Source>
struct type_conversion<
    Target, detail::lvalue_source<Source>,
    std::enable_if_t<type_traits<Target>::enabled &&
                     type_traits<Source>::enabled &&
                     !is_pointer_like_type_v<Target> &&
                     detail::is_same_handle_shape_v<Target, Source>>> {
    template <typename Factory, typename Context, typename SourceCapability>
    static Target& apply(Factory&, Context&, SourceCapability&& source,
                         [[maybe_unused]] type_descriptor requested_type,
                         [[maybe_unused]] type_descriptor registered_type) {
        if constexpr (std::is_convertible_v<Source*, Target*>) {
            return static_cast<Target&>(
                detail::materialized_source_traits_t<SourceCapability>::reference(
                    source));
        } else {
            throw detail::make_type_not_convertible_exception(requested_type,
                                                              registered_type);
        }
    }
};

template <typename Target, typename Source>
struct type_conversion<
    Target*, detail::lvalue_source<Source>,
    std::enable_if_t<type_traits<Target>::enabled &&
                     type_traits<Source>::enabled &&
                     !is_pointer_like_type_v<Target> &&
                     detail::is_same_handle_shape_v<Target, Source>>> {
    template <typename Factory, typename Context, typename SourceCapability>
    static Target* apply(Factory&, Context&, SourceCapability&& source,
                         [[maybe_unused]] type_descriptor requested_type,
                         [[maybe_unused]] type_descriptor registered_type) {
        if constexpr (std::is_convertible_v<Source*, Target*>) {
            return std::addressof(static_cast<Target&>(
                detail::materialized_source_traits_t<SourceCapability>::reference(
                    source)));
        } else {
            throw detail::make_type_not_convertible_exception(requested_type,
                                                              registered_type);
        }
    }
};

template <typename Target, typename Source>
struct type_conversion<
    Target, detail::lvalue_source<Source>,
    std::enable_if_t<!std::is_pointer_v<Target> &&
                     detail::is_borrowed_alternative_type_v<Source, Target>>> {
    template <typename Factory, typename Context, typename SourceCapability>
    static Target& apply(Factory&, Context&, SourceCapability&& source,
                         type_descriptor, type_descriptor) {
        auto& value =
            detail::materialized_source_traits_t<SourceCapability>::reference(
                source);
        return type_traits<Source>::borrow(value);
    }
};

template <typename Target, typename Source>
struct type_conversion<
    Target, detail::lvalue_source<Source>,
    std::enable_if_t<
        !std::is_pointer_v<Target> &&
        detail::is_borrowed_alternative_type_alternative_v<Source, Target>>> {
    template <typename Factory, typename Context, typename SourceCapability>
    static Target& apply(Factory&, Context&, SourceCapability&& source,
                         type_descriptor requested_type,
                         type_descriptor registered_type) {
        auto& value =
            detail::materialized_source_traits_t<SourceCapability>::reference(
                source);
        return detail::resolve_alternative_type_reference<Target>(
            type_traits<Source>::borrow(value), requested_type, registered_type);
    }
};

template <typename Target, typename Source>
struct type_conversion<
    Target, detail::lvalue_source<Source>,
    std::enable_if_t<!type_traits<Target>::enabled && !std::is_pointer_v<Target> &&
                     type_traits<Source>::enabled &&
                     type_traits<Source>::is_value_borrowable &&
                     !detail::is_borrowed_alternative_type_v<Source, Target> &&
                     !detail::is_borrowed_alternative_type_alternative_v<Source,
                                                                         Target>>> {
    template <typename Factory, typename Context, typename SourceCapability>
    static Target& apply(Factory&, Context&,
                         SourceCapability&& source,
                         type_descriptor requested_type,
                         type_descriptor registered_type) {
        return detail::borrow_reference<Target>(
            detail::materialized_source_traits_t<SourceCapability>::reference(
                source),
            requested_type,
            registered_type);
    }
};

template <typename Target, size_t N, typename Source>
struct type_conversion<
    Target[N], detail::pointer_source<Source>,
    std::enable_if_t<std::is_same_v<std::remove_cv_t<Source>,
                                    std::remove_cv_t<Target>>>> {
    template <typename Factory, typename Context, typename SourceCapability>
    static Target (&apply(Factory&, Context&, SourceCapability&& source,
                          type_descriptor, type_descriptor))[N] {
        return *reinterpret_cast<Target(*)[N]>(
            detail::materialized_source_traits_t<SourceCapability>::pointer(source));
    }
};

template <typename Target, typename Source>
struct type_conversion<
    Target*, detail::lvalue_source<Source>,
    std::enable_if_t<detail::is_borrowed_alternative_type_v<Source, Target>>> {
    template <typename Factory, typename Context, typename SourceCapability>
    static Target* apply(Factory&, Context&, SourceCapability&& source,
                         type_descriptor, type_descriptor) {
        auto& value =
            detail::materialized_source_traits_t<SourceCapability>::reference(
                source);
        return std::addressof(static_cast<Target&>(type_traits<Source>::borrow(value)));
    }
};

template <typename Target, typename Source>
struct type_conversion<
    Target*, detail::lvalue_source<Source>,
    std::enable_if_t<
        detail::is_borrowed_alternative_type_alternative_v<Source, Target>>> {
    template <typename Factory, typename Context, typename SourceCapability>
    static Target* apply(Factory&, Context&, SourceCapability&& source,
                         type_descriptor requested_type,
                         type_descriptor registered_type) {
        auto& value =
            detail::materialized_source_traits_t<SourceCapability>::reference(
                source);
        return detail::resolve_alternative_type_pointer<Target>(
            type_traits<Source>::borrow(value), requested_type, registered_type);
    }
};

template <typename Target, typename Source>
struct type_conversion<
    Target*, detail::lvalue_source<Source>,
    std::enable_if_t<!type_traits<Target>::enabled &&
                     type_traits<Source>::enabled &&
                     type_traits<Source>::is_value_borrowable &&
                     !detail::is_borrowed_alternative_type_v<Source, Target> &&
                     !detail::is_borrowed_alternative_type_alternative_v<Source,
                                                                         Target>>> {
    template <typename Factory, typename Context, typename SourceCapability>
    static Target* apply(Factory&, Context&,
                         SourceCapability&& source,
                         type_descriptor requested_type,
                         type_descriptor registered_type) {
        return std::addressof(detail::borrow_reference<Target>(
            detail::materialized_source_traits_t<SourceCapability>::reference(
                source),
            requested_type,
            registered_type));
    }
};

template <typename Target, typename Source>
struct type_conversion<
    Target*, detail::pointer_source<Source>,
    std::enable_if_t<std::is_array_v<Target> &&
                     std::is_same_v<std::remove_cv_t<Source>,
                                    std::remove_cv_t<Target>>>> {
    template <typename Factory, typename Context, typename SourceCapability>
    static Target* apply(Factory&, Context&, SourceCapability&& source,
                         type_descriptor, type_descriptor) {
        return detail::materialized_source_traits_t<SourceCapability>::pointer(
            source);
    }
};

template <typename Target, size_t N, typename Source>
struct type_conversion<
    Target (*)[N], detail::pointer_source<Source>,
    std::enable_if_t<std::is_same_v<std::remove_cv_t<Source>,
                                    std::remove_cv_t<Target>>>> {
    template <typename Factory, typename Context, typename SourceCapability>
    static Target (*apply(Factory&, Context&, SourceCapability&& source,
                          type_descriptor, type_descriptor))[N] {
        return reinterpret_cast<Target(*)[N]>(
            detail::materialized_source_traits_t<SourceCapability>::pointer(
                source));
    }
};

template <typename Target, typename Source>
struct type_conversion<
    Target*, detail::lvalue_source<Source>,
    std::enable_if_t<!type_traits<Target>::enabled &&
                     type_traits<Source>::enabled &&
                     !type_traits<Source>::is_value_borrowable>> {
    template <typename Factory, typename Context, typename SourceCapability>
    static Target* apply(Factory&, Context&, SourceCapability&& source,
                         [[maybe_unused]] type_descriptor requested_type,
                         [[maybe_unused]] type_descriptor registered_type) {
        auto& value =
            detail::materialized_source_traits_t<SourceCapability>::reference(
                source);
        if constexpr (std::is_convertible_v<decltype(type_traits<Source>::get(value)),
                                            Target*>) {
            return type_traits<Source>::get(value);
        } else {
            throw detail::make_type_not_convertible_exception(requested_type,
                                                              registered_type);
        }
    }
};

template <typename Target, typename Source>
struct type_conversion<
    Target, detail::lvalue_source<Source>,
    std::enable_if_t<is_pointer_like_type_v<Target> &&
                     is_pointer_like_type_v<Source>>> {
    template <typename Factory, typename Context, typename SourceCapability>
    static Target& apply(Factory& factory, Context& context,
                         SourceCapability&& source,
                         type_descriptor requested_type,
                         type_descriptor registered_type) {
        return detail::resolve_handle_or_borrow<Target, Source>(
            factory, context, std::forward<SourceCapability>(source),
            requested_type, registered_type);
    }
};

template <typename Target, typename Source>
struct type_conversion<
    Target*, detail::lvalue_source<Source>,
    std::enable_if_t<is_pointer_like_type_v<Target> &&
                     is_pointer_like_type_v<Source>>> {
    template <typename Factory, typename Context, typename SourceCapability>
    static Target* apply(Factory& factory, Context& context,
                         SourceCapability&& source,
                         type_descriptor requested_type,
                         type_descriptor registered_type) {
        return detail::resolve_handle_or_borrow_pointer<Target, Source>(
            factory, context, std::forward<SourceCapability>(source),
            requested_type, registered_type);
    }
};

template <typename Target, typename Source>
struct type_conversion<
    Target*, detail::pointer_source<Source>,
    std::enable_if_t<!std::is_array_v<Target> &&
                     !is_alternative_type_v<Source>>> {
    template <typename Factory, typename Context, typename SourceCapability>
    static Target* apply(Factory&, Context&, SourceCapability&& source,
                         type_descriptor, type_descriptor) {
        return detail::materialized_source_traits_t<SourceCapability>::pointer(
            source);
    }
};

template <typename Target, typename Source>
struct type_conversion<
    Target, detail::pointer_source<Source>,
    std::enable_if_t<!is_pointer_like_type_v<Target> &&
                     !std::is_pointer_v<Target> &&
                     !std::is_array_v<Target> && !is_alternative_type_v<Source>>> {
    template <typename Factory, typename Context, typename SourceCapability>
    static Target& apply(Factory&, Context&, SourceCapability&& source,
                         type_descriptor, type_descriptor) {
        return detail::materialized_source_traits_t<SourceCapability>::reference(
            source);
    }
};

template <typename Target, typename Source>
struct type_conversion<
    Target*, detail::lvalue_source<Source>,
    std::enable_if_t<!type_traits<Source>::enabled &&
                     !is_pointer_like_type_v<Target> &&
                     !is_alternative_type_v<Source>>> {
    template <typename Factory, typename Context, typename SourceCapability>
    static Target* apply(Factory&, Context&, SourceCapability&& source,
                         type_descriptor, type_descriptor) {
        return detail::materialized_source_traits_t<SourceCapability>::pointer(
            source);
    }
};
} // namespace dingo

#ifdef _MSC_VER
#pragma warning(pop)
#endif
