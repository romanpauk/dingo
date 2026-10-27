//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/container.h>

#include <cstddef>
#include <string>

namespace dingo::model {

template <typename Interface>
struct indexed_container_traits : dingo::dynamic_container_traits {
  using lookup_definition_type =
      dingo::lookups<dingo::associative<std::size_t, Interface>>;
};

template <typename Interface>
struct indexed_int_container_traits : dingo::dynamic_container_traits {
  using lookup_definition_type =
      dingo::lookups<dingo::associative<int, Interface>>;
};

template <typename Interface>
struct indexed_string_container_traits : dingo::dynamic_container_traits {
  using lookup_definition_type =
      dingo::lookups<dingo::associative<std::string, Interface>>;
};

} // namespace dingo::model
