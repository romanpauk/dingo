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
#include <dingo/type_traits.h>

#include <cassert>
#include <memory>
#include <optional>

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4702)
#endif

namespace dingo {
namespace detail {
template <typename T> struct is_std_optional : std::false_type {};
template <typename T> struct is_std_optional<std::optional<T>> : std::true_type {};

template <typename T>
inline constexpr bool is_std_optional_v = is_std_optional<T>::value;
} // namespace detail

// TODO: logic here should not depend on knowledge of storage types,
// all the paths that depend on storage type are destructible (using move).

// TODO: this file is really terrible, I need to look at how to deduplicate it

struct unique;

template <typename StorageTag, typename Target, typename Source, typename = void>
struct type_conversion {
    template <typename Factory, typename Context>
    static void* apply(Factory& factory, Context& context);
};

template <typename Target, typename Source>
struct type_conversion<unique, Target, Source,
                       std::enable_if_t<!std::is_pointer_v<Source>>> {
    template <typename Factory, typename Context>
    static Target apply(Factory& factory, Context& context) {
        return factory.resolve(context);
    }
};

template <typename Target, typename Source>
struct type_conversion<unique, Target*, Source*, void> {
    template <typename Factory, typename Context>
    static Target* apply(Factory& factory, Context& context) {
        return factory.resolve(context);
    }
};

template <typename Target, typename Source>
struct type_conversion<
    unique, Target, Source*,
    std::enable_if_t<is_pointer_like_type_v<Target> &&
                     detail::has_type_from_pointer_v<Target, Source*>>> {
    template <typename Factory, typename Context>
    static Target apply(Factory& factory, Context& context) {
        return type_traits<Target>::from_pointer(factory.resolve(context));
    }
};

template <typename StorageTag, typename Target, typename Source>
struct type_conversion<
    StorageTag, Target, Source&,
    std::enable_if_t<!has_type_traits_v<Source> && !std::is_pointer_v<Target>>> {
    template <typename Factory, typename Context>
    static Target& apply(Factory& factory, Context& context) {
        return factory.resolve(context);
    }
};

template <typename StorageTag, typename Target, typename Source>
struct type_conversion<
    StorageTag, Target, Source&,
    std::enable_if_t<has_type_traits_v<Target> && !is_pointer_like_type_v<Target> &&
                     !detail::is_std_optional_v<Target> &&
                     std::is_same_v<Target, Source>>> {
    template <typename Factory, typename Context>
    static Target& apply(Factory& factory, Context& context) {
        return factory.resolve(context);
    }
};

template <typename StorageTag, typename Target, typename Source>
struct type_conversion<
    StorageTag, Target*, Source&,
    std::enable_if_t<has_type_traits_v<Target> && !is_pointer_like_type_v<Target> &&
                     !detail::is_std_optional_v<Target> &&
                     std::is_same_v<Target, Source>>> {
    template <typename Factory, typename Context>
    static Target* apply(Factory& factory, Context& context) {
        return std::addressof(factory.resolve(context));
    }
};

template <typename StorageTag, typename Target, typename Source>
struct type_conversion<
    StorageTag, Target, Source&,
    std::enable_if_t<!has_type_traits_v<Target> && !std::is_pointer_v<Target> &&
                     has_type_traits_v<Source> && !detail::is_std_optional_v<Source>>> {
    template <typename Factory, typename Context>
    static Target& apply(Factory& factory, Context& context) {
        return *type_traits<Source>::get(factory.resolve(context));
    }
};

template <typename StorageTag, typename Target, typename Source>
struct type_conversion<
    StorageTag, Target*, Source&,
    std::enable_if_t<!has_type_traits_v<Target> && has_type_traits_v<Source> &&
                     !detail::is_std_optional_v<Source>>> {
    template <typename Factory, typename Context>
    static Target* apply(Factory& factory, Context& context) {
        return type_traits<Source>::get(factory.resolve(context));
    }
};

template <typename StorageTag, typename Target, typename Source>
struct type_conversion<
    StorageTag, Target, Source&,
    std::enable_if_t<is_pointer_like_type_v<Target> && is_pointer_like_type_v<Source>>> {
    template <typename Factory, typename Context>
    static Target& apply(Factory& factory, Context& context) {
        return type_traits<Source>::template resolve_type<Target>(
            factory, context);
    }
};

template <typename StorageTag, typename Target, typename Source>
struct type_conversion<
    StorageTag, Target*, Source&,
    std::enable_if_t<is_pointer_like_type_v<Target> && is_pointer_like_type_v<Source>>> {
    template <typename Factory, typename Context>
    static Target* apply(Factory& factory, Context& context) {
        return std::addressof(
            type_traits<Source>::template resolve_type<Target>(factory,
                                                               context));
    }
};

template <typename StorageTag, typename Target, typename Source>
struct type_conversion<StorageTag, Target*, Source*, void> {
    template <typename Factory, typename Context>
    static Target* apply(Factory& factory, Context& context) {
        return factory.resolve(context);
    }
};

template <typename StorageTag, typename Target, typename Source>
struct type_conversion<
    StorageTag, Target, Source*,
    std::enable_if_t<!is_pointer_like_type_v<Target> && !std::is_pointer_v<Target>>> {
    template <typename Factory, typename Context>
    static Target& apply(Factory& factory, Context& context) {
        return *factory.resolve(context);
    }
};

template <typename StorageTag, typename Target, typename Source>
struct type_conversion<
    StorageTag, Target*, Source&,
    std::enable_if_t<!has_type_traits_v<Source> && !is_pointer_like_type_v<Target>>> {
    template <typename Factory, typename Context>
    static Target* apply(Factory& factory, Context& context) {
        return std::addressof(factory.resolve(context));
    }
};

template <typename StorageTag, typename Target, typename Source>
struct type_conversion<StorageTag, Target, std::optional<Source>&, void> {
    template <typename Factory, typename Context>
    static Target& apply(Factory& factory, Context& context) {
        return factory.resolve(context).value();
    }
};

template <typename StorageTag, typename Target, typename Source>
struct type_conversion<StorageTag, Target*, std::optional<Source>&, void> {
    template <typename Factory, typename Context>
    static Target* apply(Factory& factory, Context& context) {
        return &factory.resolve(context).value();
    }
};

template <typename StorageTag, typename Target, typename Source>
struct type_conversion<StorageTag, std::optional<Target>,
                       std::optional<Source>&, void> {
    template <typename Factory, typename Context>
    static std::optional<Source>& apply(Factory& factory, Context& context) {
        if constexpr (std::is_same_v<Target, Source>)
            return factory.resolve(context);
        throw type_not_convertible_exception();
    }
};

template <typename StorageTag, typename Target, typename Source>
struct type_conversion<StorageTag, std::optional<Target>*,
                       std::optional<Source>&, void> {
    template <typename Factory, typename Context>
    static std::optional<Target>* apply(Factory& factory, Context& context) {
        if constexpr (std::is_same_v<Target, Source>)
            return &factory.resolve(context);
        throw type_not_convertible_exception();
    }
};

} // namespace dingo

#ifdef _MSC_VER
#pragma warning(pop)
#endif
