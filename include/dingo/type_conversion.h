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

#include <cassert>
#include <memory>
#include <optional>

namespace dingo {

// TODO: logic here should not depend on knowledge of storage types,
// all the paths that depend on storage type are destructible (using move).

// TODO: this file is really terrible, I need to look at how to deduplicate it

struct unique;

template <typename StorageTag, typename Target, typename Source>
struct type_conversion {
    template <typename Factory, typename Context, typename Temporary>
    static void* apply(Factory& factory, Context& context, Temporary&);
};

template <typename Target, typename Source>
struct type_conversion<unique, Target, Source> {
    template <typename Factory, typename Context, typename Temporary>
    static Target apply(Factory& factory, Context& context, Temporary&) {
        return factory.resolve(context);
    }
};

template <typename Target, typename Source>
struct type_conversion<unique, Target*, Source*> {
    template <typename Factory, typename Context, typename Temporary>
    static Target* apply(Factory& factory, Context& context, Temporary&) {
        return factory.resolve(context);
    }
};

template <typename Target, typename Source>
struct type_conversion<unique, std::unique_ptr<Target>, Source*> {
    template <typename Factory, typename Context, typename Temporary>
    static std::unique_ptr<Target> apply(Factory& factory, Context& context,
                                         Temporary&) {
        return std::unique_ptr<Target>(factory.resolve(context));
    }
};

template <typename Target, typename Source>
struct type_conversion<unique, std::unique_ptr<Target>, Source> {
    template <typename Factory, typename Context, typename Temporary>
    static std::unique_ptr<Target> apply(Factory& factory, Context& context,
                                         Temporary&) {
        return std::unique_ptr<Target>(new Source{ factory.resolve(context) });
    }
};


template <typename Target, typename Source>
struct type_conversion<unique, std::unique_ptr<Target>,
                       std::unique_ptr<Source>> {
    template <typename Factory, typename Context, typename Temporary>
    static std::unique_ptr<Target> apply(Factory& factory, Context& context,
                                         Temporary&) {
        return factory.resolve(context);
    }
};

template <typename Target, typename Source>
struct type_conversion<unique, std::shared_ptr<Target>, Source*> {
    template <typename Factory, typename Context, typename Temporary>
    static std::shared_ptr<Target> apply(Factory& factory, Context& context,
                                         Temporary&) {
        return std::shared_ptr<Target>(factory.resolve(context));
    }
};

template <typename Target, typename Source>
struct type_conversion<unique, std::shared_ptr<Target>,
                       std::shared_ptr<Source>> {
    template <typename Factory, typename Context, typename Temporary>
    static std::shared_ptr<Target> apply(Factory& factory, Context& context,
                                         Temporary&) {
        return factory.resolve(context);
    }
};

template <typename StorageTag, typename Target, typename Source>
struct type_conversion<StorageTag, Target, Source&> {
    template <typename Factory, typename Context, typename Temporary>
    static Target& apply(Factory& factory, Context& context, Temporary&) {
        return factory.resolve(context);
    }
};

template <typename StorageTag, typename Target, typename Source>
struct type_conversion<StorageTag, Target*, Source&> {
    template <typename Factory, typename Context, typename Temporary>
    static Target* apply(Factory& factory, Context& context, Temporary&) {
        return &factory.resolve(context);
    }
};

template <typename StorageTag, typename Target, typename Source>
struct type_conversion<StorageTag, Target*, Source*> {
    template <typename Factory, typename Context, typename Temporary>
    static Target* apply(Factory& factory, Context& context, Temporary&) {
        return factory.resolve(context);
    }
};

template <typename StorageTag, typename Target, typename Source>
struct type_conversion<StorageTag, Target, Source*> {
    template <typename Factory, typename Context, typename Temporary>
    static Target& apply(Factory& factory, Context& context, Temporary&) {
        return *factory.resolve(context);
    }
};

template <typename StorageTag, typename Target, typename Source>
struct type_conversion<StorageTag, Target*, std::shared_ptr<Source>&> {
    template <typename Factory, typename Context, typename Temporary>
    static Target* apply(Factory& factory, Context& context, Temporary&) {
        return factory.resolve(context).get();
    }
};

template <typename StorageTag, typename Target, typename Source>
struct type_conversion<StorageTag, Target, std::shared_ptr<Source>&> {
    template <typename Factory, typename Context, typename Temporary>
    static Target& apply(Factory& factory, Context& context, Temporary&) {
        return *factory.resolve(context);
    }
};

template <typename StorageTag, typename Target, typename Source>
struct type_conversion<StorageTag, std::shared_ptr<Target>,
                       std::shared_ptr<Source>&> {
    template <typename Factory, typename Context, typename Temporary>
    static std::shared_ptr<Target>& apply(Factory& factory, Context& context,
                                          Temporary& temporary) {
        if constexpr (std::is_same_v<Target, Source>)
            return factory.resolve(context);
        else
            return temporary.template construct<std::shared_ptr<Target>>(
                factory.resolve(context));
    }
};

template <typename StorageTag, typename Target, typename Source>
struct type_conversion<StorageTag, std::shared_ptr<Target>*,
                       std::shared_ptr<Source>&> {
    template <typename Factory, typename Context, typename Temporary>
    static std::shared_ptr<Target>* apply(Factory& factory, Context& context,
                                          Temporary& temporary) {
        if constexpr (std::is_same_v<Target, Source>)
            return &factory.resolve(context);
        else
            return &temporary.template construct<std::shared_ptr<Target>>(
                factory.resolve(context));
    }
};

template <typename StorageTag, typename Target, typename Source>
struct type_conversion<StorageTag, Target*, std::unique_ptr<Source>&> {
    template <typename Factory, typename Context, typename Temporary>
    static Target* apply(Factory& factory, Context& context, Temporary&) {
        return factory.resolve(context).get();
    }
};

template <typename StorageTag, typename Target, typename Source>
struct type_conversion<StorageTag, std::unique_ptr<Target>*,
                       std::unique_ptr<Source>&> {
    template <typename Factory, typename Context, typename Temporary>
    static std::unique_ptr<Target>* apply(Factory& factory, Context& context,
                                          Temporary&) {
        if constexpr (std::is_same_v<Target, Source>)
            return &factory.resolve(context);
        throw type_not_convertible_exception();
    }
};

template <typename StorageTag, typename Target, typename Source>
struct type_conversion<StorageTag, Target, std::unique_ptr<Source>&> {
    template <typename Factory, typename Context, typename Temporary>
    static Target& apply(Factory& factory, Context& context, Temporary&) {
        return *factory.resolve(context);
    }
};

template <typename StorageTag, typename Target, typename Source>
struct type_conversion<StorageTag, std::unique_ptr<Target>,
                       std::unique_ptr<Source>&> {
    template <typename Factory, typename Context, typename Temporary>
    static std::unique_ptr<Target>& apply(Factory& factory, Context& context,
                                          Temporary&) {
        if constexpr (std::is_same_v<Target, Source>)
            return factory.resolve(context);
        throw type_not_convertible_exception();
    }
};

template <typename StorageTag, typename Target, typename Source>
struct type_conversion<StorageTag, Target, std::optional<Source>&> {
    template <typename Factory, typename Context, typename Temporary>
    static Target& apply(Factory& factory, Context& context, Temporary&) {
        return factory.resolve(context).value();
    }
};

template <typename StorageTag, typename Target, typename Source>
struct type_conversion<StorageTag, Target*, std::optional<Source>&> {
    template <typename Factory, typename Context, typename Temporary>
    static Target* apply(Factory& factory, Context& context, Temporary&) {
        return &factory.resolve(context).value();
    }
};

template <typename StorageTag, typename Target, typename Source>
struct type_conversion<StorageTag, std::optional<Target>,
                       std::optional<Source>&> {
    template <typename Factory, typename Context, typename Temporary>
    static std::optional<Source>& apply(Factory& factory, Context& context,
                                        Temporary&) {
        if constexpr (std::is_same_v<Target, Source>)
            return factory.resolve(context);
        throw type_not_convertible_exception();
    }
};

template <typename StorageTag, typename Target, typename Source>
struct type_conversion<StorageTag, std::optional<Target>*,
                       std::optional<Source>&> {
    template <typename Factory, typename Context, typename Temporary>
    static std::optional<Target>* apply(Factory& factory, Context& context,
                                        Temporary&) {
        if constexpr (std::is_same_v<Target, Source>)
            return &factory.resolve(context);
        throw type_not_convertible_exception();
    }
};

} // namespace dingo
