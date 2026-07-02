//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include "matrix/support/runtime_containers.h"

#include <dingo/container.h>
#include <dingo/rtti/static_provider.h>
#include <dingo/type/type_map.h>

#include <memory>
#include <tuple>

namespace dingo::matrix {

struct custom_rtti_container_traits {
  template <typename> using rebind_t = custom_rtti_container_traits;

  using tag_type = void;
  using rtti_type = dingo::rtti<dingo::static_provider>;
  template <typename Value, typename Allocator>
  using type_map_type = dingo::dynamic_type_map<Value, rtti_type, Allocator>;
  using allocator_type = std::allocator<char>;
  using lookup_definition_type = std::tuple<>;
};

} // namespace dingo::matrix
