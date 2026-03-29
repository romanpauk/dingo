//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/config.h>
#include <dingo/type/type_descriptor.h>

namespace dingo {
class resolving_context;

template <typename Container> class instance_factory_interface {
  public:
    virtual ~instance_factory_interface() = default;

    virtual void*
    get_value(resolving_context&,
              const typename Container::rtti_type::type_index&,
              type_descriptor requested_type) = 0;
    virtual void*
    get_lvalue_reference(resolving_context&,
                         const typename Container::rtti_type::type_index&,
                         type_descriptor requested_type) = 0;
    virtual void*
    get_rvalue_reference(resolving_context&,
                         const typename Container::rtti_type::type_index&,
                         type_descriptor requested_type) = 0;
    virtual void*
    get_pointer(resolving_context&,
                const typename Container::rtti_type::type_index&,
                type_descriptor requested_type) = 0;

    virtual void destroy() = 0;

    bool cacheable = false; // TODO
};
} // namespace dingo
