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
    static void* apply(Source source);
};

template <typename Target, typename Source>
struct type_conversion<Target*, Source*> {
    static Target* apply(Source* source) { return source; }
};

template <typename Target, typename Source>
struct type_conversion<Target*, std::shared_ptr<Source>*> {
    static Target* apply(std::shared_ptr<Source>* source) {
        return source->get();
    }
};

template <typename Target, typename Source>
struct type_conversion<std::shared_ptr<Target>*, std::shared_ptr<Source>*> {
    static std::shared_ptr<Target>* apply(std::shared_ptr<Source>* source) {
        return source;
    }
};

template <typename Target, typename Source>
struct type_conversion<Target*, std::unique_ptr<Source>*> {
    static Target* apply(std::unique_ptr<Source>* source) {
        return source->get();
    }
};

template <typename Target, typename Source>
struct type_conversion<std::unique_ptr<Target>*, std::unique_ptr<Source>*> {
    static std::unique_ptr<Target>* apply(std::unique_ptr<Source>* source) {
        return source;
    }
};

template <typename RTTI, typename T>
void* convert_type(type_list<>, const typename RTTI::type_index&, T&&) {
    assert(false);
    throw type_not_convertible_exception();
}

template <typename RTTI, typename T, typename Head, typename... Tail>
void* convert_type(type_list<Head, Tail...>,
                   const typename RTTI::type_index& type, T* instance) {
    if (RTTI::template get_type_index<Head>() == type) {
        using PrimitiveT = decay_t<T>;
        using ResultT = std::remove_pointer_t<
            std::remove_reference_t<rebind_type_t<Head, PrimitiveT>>>;
        return type_conversion<ResultT*, T*>::apply(instance);
    } else {
        return convert_type<RTTI>(type_list<Tail...>{}, type, instance);
    }
}
} // namespace dingo
