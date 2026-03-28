//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/config.h>

#include <dingo/exceptions.h>
#include <dingo/interface_storage_traits.h>
#include <dingo/rebind_type.h>
#include <dingo/type_conversion_traits.h>
#include <dingo/type_list.h>
#include <dingo/type_traits.h>

#include <cassert>
#include <memory>

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4702)
#endif

namespace dingo {
namespace detail {
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
        throw make_type_not_convertible_exception(requested_type,
                                                  registered_type);
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
} // namespace detail

// TODO: logic here should not depend on knowledge of storage types,
// all the paths that depend on storage type are destructible (using move).

// TODO: this file is really terrible, I need to look at how to deduplicate it

struct unique;

template <typename StorageTag, typename Target, typename Source, typename = void>
struct type_conversion {
    template <typename Factory, typename Context>
    static void* apply(Factory& factory, Context& context,
                       type_descriptor requested_type,
                       type_descriptor registered_type);
};

template <typename Target, typename Source>
struct type_conversion<unique, Target, Source,
                       std::enable_if_t<!std::is_pointer_v<Source>>> {
    template <typename Factory, typename Context>
    static Target apply(Factory& factory, Context& context, type_descriptor,
                        type_descriptor) {
        return detail::resolve_source(factory, context);
    }
};

template <typename Target, typename Source>
struct type_conversion<unique, Target*, Source*, void> {
    template <typename Factory, typename Context>
    static Target* apply(Factory& factory, Context& context, type_descriptor,
                         type_descriptor) {
        return detail::resolve_source(factory, context);
    }
};

template <typename Target, typename Source>
struct type_conversion<
    unique, Target, Source*,
    std::enable_if_t<is_pointer_like_type_v<Target> &&
                     detail::has_type_from_pointer_v<Target, Source*>>> {
    template <typename Factory, typename Context>
    static Target apply(Factory& factory, Context& context, type_descriptor,
                        type_descriptor) {
        return type_traits<Target>::from_pointer(
            detail::resolve_source(factory, context));
    }
};

template <typename StorageTag, typename Target, typename Source>
struct type_conversion<
    StorageTag, Target, Source&,
    std::enable_if_t<!type_traits<Source>::enabled && !std::is_pointer_v<Target>>> {
    template <typename Factory, typename Context>
    static Target& apply(Factory& factory, Context& context, type_descriptor,
                         type_descriptor) {
        return detail::resolve_source(factory, context);
    }
};

template <typename StorageTag, typename Target, typename Source>
struct type_conversion<
    StorageTag, Target, Source&,
    std::enable_if_t<type_traits<Target>::enabled &&
                     type_traits<Source>::enabled &&
                     !is_pointer_like_type_v<Target> &&
                     detail::is_same_handle_shape_v<Target, Source>>> {
    template <typename Factory, typename Context>
    static Target& apply(Factory& factory, Context& context,
                         [[maybe_unused]] type_descriptor requested_type,
                         [[maybe_unused]] type_descriptor registered_type) {
        if constexpr (std::is_convertible_v<Source*, Target*>) {
            return static_cast<Target&>(detail::resolve_source(factory, context));
        } else {
            throw detail::make_type_not_convertible_exception(requested_type,
                                                              registered_type);
        }
    }
};

template <typename StorageTag, typename Target, typename Source>
struct type_conversion<
    StorageTag, Target*, Source&,
    std::enable_if_t<type_traits<Target>::enabled &&
                     type_traits<Source>::enabled &&
                     !is_pointer_like_type_v<Target> &&
                     detail::is_same_handle_shape_v<Target, Source>>> {
    template <typename Factory, typename Context>
    static Target* apply(Factory& factory, Context& context,
                         [[maybe_unused]] type_descriptor requested_type,
                         [[maybe_unused]] type_descriptor registered_type) {
        if constexpr (std::is_convertible_v<Source*, Target*>) {
            return std::addressof(
                static_cast<Target&>(detail::resolve_source(factory, context)));
        } else {
            throw detail::make_type_not_convertible_exception(requested_type,
                                                              registered_type);
        }
    }
};

template <typename StorageTag, typename Target, typename Source>
struct type_conversion<
    StorageTag, Target, Source&,
    std::enable_if_t<!type_traits<Target>::enabled && !std::is_pointer_v<Target> &&
                     type_traits<Source>::enabled &&
                     type_traits<Source>::is_value_borrowable>> {
    template <typename Factory, typename Context>
    static Target& apply(Factory& factory, Context& context,
                         type_descriptor requested_type,
                         type_descriptor registered_type) {
        return detail::resolve_borrowed_reference<Target, Source>(
            factory, context, requested_type, registered_type);
    }
};

template <typename StorageTag, typename Target, typename Source>
struct type_conversion<
    StorageTag, Target*, Source&,
    std::enable_if_t<!type_traits<Target>::enabled &&
                     type_traits<Source>::enabled &&
                     type_traits<Source>::is_value_borrowable>> {
    template <typename Factory, typename Context>
    static Target* apply(Factory& factory, Context& context,
                         type_descriptor requested_type,
                         type_descriptor registered_type) {
        return detail::resolve_borrowed_pointer<Target, Source>(
            factory, context, requested_type, registered_type);
    }
};

template <typename StorageTag, typename Target, typename Source>
struct type_conversion<
    StorageTag, Target, Source&,
    std::enable_if_t<is_pointer_like_type_v<Target> && is_pointer_like_type_v<Source>>> {
    template <typename Factory, typename Context>
    static Target& apply(Factory& factory, Context& context,
                         type_descriptor requested_type,
                         type_descriptor registered_type) {
        return detail::resolve_handle_or_borrow<Target, Source>(
            factory, context, requested_type, registered_type);
    }
};

template <typename StorageTag, typename Target, typename Source>
struct type_conversion<
    StorageTag, Target*, Source&,
    std::enable_if_t<is_pointer_like_type_v<Target> && is_pointer_like_type_v<Source>>> {
    template <typename Factory, typename Context>
    static Target* apply(Factory& factory, Context& context,
                         type_descriptor requested_type,
                         type_descriptor registered_type) {
        return detail::resolve_handle_or_borrow_pointer<Target, Source>(
            factory, context, requested_type, registered_type);
    }
};

template <typename StorageTag, typename Target, typename Source>
struct type_conversion<StorageTag, Target*, Source*, void> {
    template <typename Factory, typename Context>
    static Target* apply(Factory& factory, Context& context, type_descriptor,
                         type_descriptor) {
        return detail::resolve_source(factory, context);
    }
};

template <typename StorageTag, typename Target, typename Source>
struct type_conversion<
    StorageTag, Target, Source*,
    std::enable_if_t<!is_pointer_like_type_v<Target> && !std::is_pointer_v<Target>>> {
    template <typename Factory, typename Context>
    static Target& apply(Factory& factory, Context& context, type_descriptor,
                         type_descriptor) {
        return detail::dereference_source(factory, context);
    }
};

template <typename StorageTag, typename Target, typename Source>
struct type_conversion<
    StorageTag, Target*, Source&,
    std::enable_if_t<!type_traits<Source>::enabled && !is_pointer_like_type_v<Target>>> {
    template <typename Factory, typename Context>
    static Target* apply(Factory& factory, Context& context, type_descriptor,
                         type_descriptor) {
        return detail::resolve_address(factory, context);
    }
};
} // namespace dingo

#ifdef _MSC_VER
#pragma warning(pop)
#endif
