//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/lookup/container_lookup_definition.h>
#include <dingo/rtti/typeid_provider.h>

#include <memory>
#include <tuple>
#include <type_traits>

namespace dingo {

struct dynamic_container_traits {
  using rtti_type = rtti<typeid_provider>;
  using allocator_type = std::allocator<char>;
  using lookup_definition_type = std::tuple<>;
};

} // namespace dingo
