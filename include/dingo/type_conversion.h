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

namespace dingo {
template <typename Target, typename Source> struct type_conversion {
    template <typename Context> static void* apply(Context&, Source source);
};

template <typename Target, typename Source>
struct type_conversion<Target*, Source*> {
    template <typename Context> static Target* apply(Context&, Source* source) {
        return source;
    }
};

template <typename Target, typename Source>
struct type_conversion<Target*, std::shared_ptr<Source>*> {
    template <typename Context>
    static Target* apply(Context&, std::shared_ptr<Source>* source) {
        return source->get();
    }
};

template <typename Target, typename Source>
struct type_conversion<std::shared_ptr<Target>*, std::shared_ptr<Source>*> {
    template <typename Context>
    static std::shared_ptr<Target>* apply(Context&,
                                          std::shared_ptr<Source>* source) {
        return source;
    }
};

template <typename Target, typename Source>
struct type_conversion<Target*, std::unique_ptr<Source>*> {
    template <typename Context>
    static Target* apply(Context&, std::unique_ptr<Source>* source) {
        return source->get();
    }
};

template <typename Target, typename Source>
struct type_conversion<std::unique_ptr<Target>*, Source*> {
    template <typename Context>
    static std::unique_ptr<Target>* apply(Context& context, Source* source) {
        return context.template construct<std::unique_ptr<Target>>(source);
    }
};

template <typename Target, typename Source>
struct type_conversion<std::shared_ptr<Target>*, Source*> {
    template <typename Context>
    static std::shared_ptr<Target>* apply(Context& context, Source* source) {
        return context.template construct<std::shared_ptr<Target>>(source);
    }
};

template <typename Target, typename Source>
struct type_conversion<std::shared_ptr<Target>*, std::unique_ptr<Source>*> {
    template <typename Context>
    static std::shared_ptr<Target>* apply(Context& context,
                                          std::unique_ptr<Source>* source) {
        return context.template construct<std::shared_ptr<Target>>(
            std::move(*source));
    }
};

template <typename Target, typename Source>
struct type_conversion<std::unique_ptr<Target>*, std::unique_ptr<Source>*> {
    template <typename Context>
    static std::unique_ptr<Target>* apply(Context&,
                                          std::unique_ptr<Source>* source) {
        return source;
    }
};

template <typename RTTI, typename Context, typename T>
void* convert_type(Context&, type_list<>, const typename RTTI::type_index&,
                   T&&) {
    assert(false);
    throw type_not_convertible_exception();
}

template <typename RTTI, typename Context, typename T, typename Head,
          typename... Tail>
void* convert_type(Context& context, type_list<Head, Tail...>,
                   const typename RTTI::type_index& type, T* instance) {
    if (RTTI::template get_type_index<Head>() == type) {
        using PrimitiveT = decay_t<T>;
        using ResultT = std::remove_pointer_t<
            std::remove_reference_t<rebind_type_t<Head, PrimitiveT>>>;
        return type_conversion<ResultT*, T*>::apply(context, instance);
    } else {
        return convert_type<RTTI>(context, type_list<Tail...>{}, type,
                                  instance);
    }
}
} // namespace dingo
