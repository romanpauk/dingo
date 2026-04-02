//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/config.h>
#include <dingo/type/rebind_type.h>
#include <dingo/type/type_descriptor.h>

namespace dingo {
class resolving_context;

template <typename RTTI> struct instance_request {
    using type_index = typename RTTI::type_index;

    type_index lookup_type;
    type_descriptor requested_type;
};

template <typename T> struct request_lookup_type {
    using type = rebind_leaf_t<T, runtime_type>;
};

template <typename T> struct request_lookup_type<const T&> {
    using type = rebind_leaf_t<T&, runtime_type>;
};

template <typename T>
using request_lookup_type_t = typename request_lookup_type<T>::type;

struct instance_cache_sink {
    void* context = nullptr;
    void (*store)(void*, void*) = nullptr;

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4702)
#endif
    void operator()(void* ptr) const {
        if (store) {
            store(context, ptr);
        }
    }
#ifdef _MSC_VER
#pragma warning(pop)
#endif
};

template <typename Container> class instance_factory_interface {
  public:
    virtual ~instance_factory_interface() = default;

    virtual void* get_value(
        resolving_context&,
        const instance_request<typename Container::rtti_type>& request,
        instance_cache_sink) = 0;
    virtual void* get_lvalue_reference(
        resolving_context&,
        const instance_request<typename Container::rtti_type>& request,
        instance_cache_sink) = 0;
    virtual void* get_rvalue_reference(
        resolving_context&,
        const instance_request<typename Container::rtti_type>& request,
        instance_cache_sink) = 0;
    virtual void* get_pointer(
        resolving_context&,
        const instance_request<typename Container::rtti_type>& request,
        instance_cache_sink) = 0;

    virtual void destroy() = 0;
};
} // namespace dingo
