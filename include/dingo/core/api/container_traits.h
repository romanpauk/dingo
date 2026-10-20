//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

// The traits of the containers of the next implementation: the lookup
// declarations they list, none by default.

#include <dingo/core/api/rtti/typeid_provider.h>

#include <memory>
#include <tuple>

namespace dingo {

struct dynamic_container_traits {
  using rtti_type = rtti<typeid_provider>;
  using allocator_type = std::allocator<char>;
  using lookup_definition_type = std::tuple<>;
};

struct static_container_traits {
  using rtti_type = rtti<typeid_provider>;
  using lookup_definition_type = std::tuple<>;
};

} // namespace dingo
