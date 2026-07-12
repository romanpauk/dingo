//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/core/config.h>
#include <dingo/core/construction_scope.h>
#include <dingo/resolution/cache.h>
#include <dingo/type/rebind_type.h>
#include <dingo/type/type_descriptor.h>

namespace dingo {
template <typename Allocator> class runtime_context;

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

template <typename Container, typename Context>
class runtime_binding_interface {
public:
  virtual ~runtime_binding_interface() = default;

  detail::cache::entry *cache_slot() noexcept { return cache_slot_; }

  virtual void *
  get_value(construction_scope, Context &,
            const instance_request<typename Container::rtti_type> &request,
            detail::cache::sink) = 0;
  virtual void *get_lvalue_reference(
      construction_scope, Context &,
      const instance_request<typename Container::rtti_type> &request,
      detail::cache::sink) = 0;
  virtual void *get_rvalue_reference(
      construction_scope, Context &,
      const instance_request<typename Container::rtti_type> &request,
      detail::cache::sink) = 0;
  virtual void *
  get_pointer(construction_scope, Context &,
              const instance_request<typename Container::rtti_type> &request,
              detail::cache::sink) = 0;

protected:
  void cache_slot(detail::cache::entry *slot) noexcept { cache_slot_ = slot; }

private:
  detail::cache::entry *cache_slot_ = nullptr;
};
} // namespace dingo
