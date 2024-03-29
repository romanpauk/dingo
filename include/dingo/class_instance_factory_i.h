//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/config.h>

namespace dingo {
class resolving_context;

template <typename Container> class class_instance_factory_i {
  public:
    virtual ~class_instance_factory_i() = default;

    virtual void*
    get_value(resolving_context&,
              const typename Container::rtti_type::type_index&) = 0;
    virtual void*
    get_lvalue_reference(resolving_context&,
                         const typename Container::rtti_type::type_index&) = 0;
    virtual void*
    get_rvalue_reference(resolving_context&,
                         const typename Container::rtti_type::type_index&) = 0;
    virtual void*
    get_pointer(resolving_context&,
                const typename Container::rtti_type::type_index&) = 0;

    virtual void destroy() = 0;

    bool cacheable = false; // TODO
};
} // namespace dingo
