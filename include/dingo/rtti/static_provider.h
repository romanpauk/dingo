//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/base/config.h>
#include <dingo/base/type_descriptor.h>
#include <dingo/rtti/rtti.h>

#include <cstdint>

namespace dingo {

template <> class rtti<static_provider> {
public:
  // The address of a per-type tag. The tags are writable, so identical-data
  // folding cannot merge them.
  // TODO: This will not work across modules.
  using type_index = std::uintptr_t;

  template <typename T> static type_index get_type_index() {
    return reinterpret_cast<std::uintptr_t>(&detail::type_tag<T>);
  }
};

} // namespace dingo
