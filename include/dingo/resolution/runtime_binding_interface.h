//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/core/config.h>
#include <dingo/resolution/cache.h>
#include <dingo/type/rebind_type.h>
#include <dingo/type/type_descriptor.h>

namespace dingo {
class runtime_context;

template <typename RTTI> struct instance_request {
  using type_index = typename RTTI::type_index;

  type_index lookup_type;
  type_descriptor requested_type;
};

template <typename T> struct request_lookup_type {
  using type = rebind_leaf_t<T, runtime_type>;
};

template <typename T> struct request_lookup_type<const T &> {
  using type = rebind_leaf_t<T &, runtime_type>;
};

template <typename T>
using request_lookup_type_t = typename request_lookup_type<T>::type;

template <typename Container> class runtime_binding_interface {
public:
  virtual ~runtime_binding_interface() = default;

  virtual void *
  get_value(runtime_context &,
            const instance_request<typename Container::rtti_type> &request,
            detail::cache::sink) = 0;
  virtual void *get_lvalue_reference(
      runtime_context &,
      const instance_request<typename Container::rtti_type> &request,
      detail::cache::sink) = 0;
  virtual void *get_rvalue_reference(
      runtime_context &,
      const instance_request<typename Container::rtti_type> &request,
      detail::cache::sink) = 0;
  virtual void *
  get_pointer(runtime_context &,
              const instance_request<typename Container::rtti_type> &request,
              detail::cache::sink) = 0;
};
} // namespace dingo
