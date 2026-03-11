//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/config.h>

#include <dingo/exceptions.h>
#include <dingo/rebind_type.h>
#include <dingo/type_list.h>
#include <dingo/wrapper_traits.h>

#include <cassert>
#include <memory>
#include <optional>

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4702)
#endif

namespace dingo {

// TODO: logic here should not depend on knowledge of storage types,
// all the paths that depend on storage type are destructible (using move).

// TODO: this file is really terrible, I need to look at how to deduplicate it

struct unique;

namespace detail {

template <typename Source>
inline constexpr bool is_wrapper_family_v =
    !std::is_same_v<wrapper_base_t<Source>, wrapper_element_t<Source>>;

template <typename Source, typename Target>
inline constexpr bool is_same_wrapper_family_v =
    is_wrapper_family_v<Source> &&
    std::is_same_v<wrapper_rebind_t<Source, wrapper_element_t<Target>>,
                   wrapper_base_t<Target>>;

template <typename Source, typename Target>
inline constexpr bool can_cache_wrapper_conversion_v =
    is_same_wrapper_family_v<Source, Target> &&
    wrapper_traits<wrapper_base_t<Source>>::is_indirect &&
    !std::is_same_v<wrapper_base_t<Source>, wrapper_base_t<Target>> &&
    std::is_copy_constructible_v<wrapper_base_t<Target>> &&
    std::is_constructible_v<wrapper_base_t<Target>, Source>;

} // namespace detail

template <typename StorageTag, typename Target, typename Source>
struct type_conversion {
    template <typename Factory, typename Context>
    static void* apply(Factory& factory, Context& context);
};

template <typename Target, typename Source>
struct type_conversion<unique, Target, Source> {
    template <typename Factory, typename Context>
    static Target apply(Factory& factory, Context& context) {
        using source_base = detail::wrapper_base_t<Source>;
        using target_base = detail::wrapper_base_t<Target>;
        using target_element = detail::wrapper_element_t<Target>;

        if constexpr (detail::is_same_wrapper_family_v<Source, Target>) {
            if constexpr (std::is_constructible_v<target_base, Source>) {
                return factory.resolve(context);
            } else if constexpr (detail::can_adopt_released_wrapper_v<Source,
                                                                      Target>) {
                auto source = factory.resolve(context);
                return wrapper_traits<target_base>::template adopt<
                    target_element>(
                    wrapper_traits<source_base>::release(source));
            } else {
                return factory.resolve(context);
            }
        } else if constexpr (detail::can_adopt_released_wrapper_v<Source,
                                                                  Target>) {
            auto source = factory.resolve(context);
            return wrapper_traits<target_base>::template adopt<target_element>(
                wrapper_traits<source_base>::release(source));
        } else {
            return factory.resolve(context);
        }
    }
};

template <typename Target, typename Source>
struct type_conversion<unique, Target*, Source*> {
    template <typename Factory, typename Context>
    static Target* apply(Factory& factory, Context& context) {
        return factory.resolve(context);
    }
};

template <typename Target, typename Source>
struct type_conversion<unique, Target, Source*> {
    template <typename Factory, typename Context>
    static Target apply(Factory& factory, Context& context) {
        using target_base = detail::wrapper_base_t<Target>;
        using target_element = detail::wrapper_element_t<Target>;

        if constexpr (detail::has_wrapper_adopt_v<Target, target_element>) {
            return wrapper_traits<target_base>::template adopt<target_element>(
                factory.resolve(context));
        } else {
            return factory.resolve(context);
        }
    }
};

template <typename StorageTag, typename Target, typename Source>
struct type_conversion<StorageTag, Target, Source&> {
    template <typename Factory, typename Context>
    static Target& apply(Factory& factory, Context& context) {
        using source_base = detail::wrapper_base_t<Source>;

        if constexpr (detail::is_same_wrapper_family_v<Source, Target>) {
            if constexpr (std::is_same_v<source_base,
                                         detail::wrapper_base_t<Target>>) {
                return factory.resolve(context);
            } else if constexpr (detail::can_cache_wrapper_conversion_v<Source,
                                                                        Target>) {
                return factory.template resolve<Target>(context);
            } else {
                throw type_not_convertible_exception();
            }
        } else {
            return *wrapper_traits<source_base>::get_pointer(
                factory.resolve(context));
        }
    }
};

template <typename StorageTag, typename Target, typename Source>
struct type_conversion<StorageTag, Target*, Source&> {
    template <typename Factory, typename Context>
    static Target* apply(Factory& factory, Context& context) {
        using source_base = detail::wrapper_base_t<Source>;

        if constexpr (detail::is_same_wrapper_family_v<Source, Target>) {
            if constexpr (std::is_same_v<source_base,
                                         detail::wrapper_base_t<Target>>) {
                return &factory.resolve(context);
            } else if constexpr (detail::can_cache_wrapper_conversion_v<Source,
                                                                        Target>) {
                return &factory.template resolve<Target>(context);
            } else {
                throw type_not_convertible_exception();
            }
        } else {
            return wrapper_traits<source_base>::get_pointer(
                factory.resolve(context));
        }
    }
};

template <typename StorageTag, typename Target, typename Source>
struct type_conversion<StorageTag, Target*, Source*> {
    template <typename Factory, typename Context>
    static Target* apply(Factory& factory, Context& context) {
        return factory.resolve(context);
    }
};

template <typename StorageTag, typename Target, typename Source>
struct type_conversion<StorageTag, Target, Source*> {
    template <typename Factory, typename Context>
    static Target& apply(Factory& factory, Context& context) {
        return *factory.resolve(context);
    }
};

} // namespace dingo

#ifdef _MSC_VER
#pragma warning(pop)
#endif
