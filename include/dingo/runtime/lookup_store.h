//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/base/type_list.h>
#include <dingo/runtime/lookup_index.h>

namespace dingo {
namespace detail {

template <typename BindingInterface> struct runtime_lookup_binding_view {
  BindingInterface *binding = nullptr;
};

template <typename BindingInterface> class runtime_lookup_value {
public:
  using binding_view_type = runtime_lookup_binding_view<BindingInterface>;

  explicit runtime_lookup_value(binding_view_type view) : view_(view) {}

  runtime_lookup_value(runtime_lookup_value &&) noexcept = default;
  runtime_lookup_value &operator=(runtime_lookup_value &&) noexcept = default;
  runtime_lookup_value(const runtime_lookup_value &) = delete;
  runtime_lookup_value &operator=(const runtime_lookup_value &) = delete;

  BindingInterface &binding() { return *view_.binding; }

private:
  binding_view_type view_;
};

template <typename LookupEntries, typename BindingInterface, typename Allocator>
struct runtime_bindings_state {
  using value_type = runtime_lookup_value<BindingInterface>;

  explicit runtime_bindings_state(Allocator &allocator)
      : lookup_indexes(allocator) {}

  lookup_index<LookupEntries, value_type, Allocator> lookup_indexes;
};

} // namespace detail
} // namespace dingo
