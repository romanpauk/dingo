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

// TODO: logic here should not depend on knowledge of storage types
struct external;
struct shared;
struct shared_cyclical;
struct unique;

template <typename Target, typename Source, typename StorageTag>
struct type_conversion {
    template <typename Context> static void* apply(Context&, Source source);
};

template <typename Target, typename Source, typename StorageTag>
struct type_conversion<Target*, Source*, StorageTag> {
    template <typename Context> static Target* apply(Context&, Source* source) {
        return source;
    }
};

template <typename Target, typename Source, typename StorageTag>
struct type_conversion<Target*, std::shared_ptr<Source>*, StorageTag> {
    template <typename Context>
    static Target* apply(Context&, std::shared_ptr<Source>* source) {
        return source->get();
    }
};

template <typename Target, typename Source, typename StorageTag>
struct type_conversion<std::shared_ptr<Target>*, std::shared_ptr<Source>*,
                       StorageTag> {
    template <typename Context>
    static std::shared_ptr<Target>* apply(Context& context,
                                          std::shared_ptr<Source>* source) {
        if constexpr (std::is_same_v<Target, Source>)
            return source;
        else
            return &context.template construct<std::shared_ptr<Target>>(
                *source);
    }
};

template <typename Target, typename Source, typename StorageTag>
struct type_conversion<Target*, std::unique_ptr<Source>*, StorageTag> {
    template <typename Context>
    static Target* apply(Context&, std::unique_ptr<Source>* source) {
        return source->get();
    }
};

template <typename Target, typename Source, typename StorageTag>
struct type_conversion<Target*, std::optional<Source>*, StorageTag> {
    template <typename Context>
    static Target* apply(Context&, std::optional<Source>* source) {
        return &source->value();
    }
};

template <typename Target, typename Source, typename StorageTag>
struct type_conversion<std::optional<Target>*, std::optional<Source>*,
                       StorageTag> {
    template <typename Context>
    static std::optional<Target>* apply(Context&,
                                        std::optional<Source>* source) {
        if constexpr (std::is_same_v<Target, Source>)
            return source;
        else
            throw type_not_convertible_exception();
    }
};

template <typename Target, typename Source, typename StorageTag>
struct type_conversion<std::unique_ptr<Target>*, Source*, StorageTag> {
    template <typename Context>
    static std::unique_ptr<Target>* apply(Context& context, Source* source) {
        if constexpr (std::is_same_v<StorageTag, unique>)
            return &context.template construct<std::unique_ptr<Target>>(source);
        else
            throw type_not_convertible_exception();
    }
};

template <typename Target, typename Source, typename StorageTag>
struct type_conversion<std::optional<Target>*, Source*, StorageTag> {
    template <typename Context>
    static std::optional<Target>* apply(Context& context, Source* source) {
        if constexpr (std::is_same_v<StorageTag, unique>)
            return &context.template construct<std::optional<Target>>(
                std::move(*source));
        else
            throw type_not_convertible_exception();
    }
};

template <typename Target, typename Source, typename StorageTag>
struct type_conversion<std::shared_ptr<Target>*, Source*, StorageTag> {
    template <typename Context>
    static std::shared_ptr<Target>* apply(Context& context, Source* source) {
        if constexpr (std::is_same_v<StorageTag, unique>)
            return &context.template construct<std::shared_ptr<Target>>(source);
        else
            throw type_not_convertible_exception();
    }
};

template <typename Target, typename Source, typename StorageTag>
struct type_conversion<std::shared_ptr<Target>*, std::unique_ptr<Source>*,
                       StorageTag> {
    template <typename Context>
    static std::shared_ptr<Target>* apply(Context& context,
                                          std::unique_ptr<Source>* source) {
        if constexpr (std::is_same_v<StorageTag, unique>)
            return &context.template construct<std::shared_ptr<Target>>(
                std::move(*source));
        else
            throw type_not_convertible_exception();
    }
};

template <typename Target, typename Source, typename StorageTag>
struct type_conversion<std::unique_ptr<Target>*, std::unique_ptr<Source>*,
                       StorageTag> {
    template <typename Context>
    static std::unique_ptr<Target>* apply(Context& context,
                                          std::unique_ptr<Source>* source) {
        if constexpr (std::is_same_v<Target, Source>)
            return source;
        else if constexpr (std::is_same_v<StorageTag, unique>)
            return &context.template construct<std::unique_ptr<Target>>(
                std::move(*source));
        else
            throw type_not_convertible_exception();
    }
};

template <typename RTTI, typename Target, typename StorageTag, typename Context,
          typename Source>
void* convert_type(Context&, type_list<>, const typename RTTI::type_index&,
                   Source&&) {
    throw type_not_convertible_exception();
}

// TODO: instead of StorageTag, rvalue reference can be used to determine
// move-ability
template <typename RTTI, typename Target, typename StorageTag, typename Context,
          typename Source, typename Head, typename... Tail>
void* convert_type(Context& context, type_list<Head, Tail...>,
                   const typename RTTI::type_index& type, Source* source) {
    if (RTTI::template get_type_index<Head>() == type) {
        using TargetResult = std::remove_pointer_t<
            std::remove_reference_t<rebind_type_t<Head, decay_t<Target>>>>;
        return type_conversion<TargetResult*, Source*, StorageTag>::apply(
            context, source);
    } else {
        return convert_type<RTTI, Target, StorageTag>(
            context, type_list<Tail...>{}, type, source);
    }
}

} // namespace dingo
