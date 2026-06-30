//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include "matrix/support/runtime_containers.h"
#include "matrix/support/values.h"

#include <dingo/container.h>

#include <cstddef>
#include <string>

namespace dingo::matrix {

struct indexed_container_traits : dingo::dynamic_container_traits {
  using view_definition_type =
      dingo::views<dingo::associative<std::size_t, element_interface>>;
};

struct indexed_int_container_traits : dingo::dynamic_container_traits {
  using view_definition_type =
      dingo::views<dingo::associative<int, element_interface>>;
};

struct indexed_string_container_traits : dingo::dynamic_container_traits {
  using view_definition_type =
      dingo::views<dingo::associative<std::string, element_interface>>;
};

struct indexed_dsl_container_traits : dingo::dynamic_container_traits {
  using view_definition_type = dingo::views<
      dingo::associative<std::size_t, dingo::interfaces<element_interface>>>;
};

} // namespace dingo::matrix
