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
  using lookup_definition_type =
      dingo::lookups<dingo::associative<element_interface, std::size_t>>;
};

struct indexed_int_container_traits : dingo::dynamic_container_traits {
  using lookup_definition_type =
      dingo::lookups<dingo::associative<element_interface, int>>;
};

struct indexed_string_container_traits : dingo::dynamic_container_traits {
  using lookup_definition_type =
      dingo::lookups<dingo::associative<element_interface, std::string>>;
};

struct indexed_dsl_container_traits : dingo::dynamic_container_traits {
  using lookup_definition_type = dingo::lookups<
      dingo::associative<dingo::interfaces<element_interface>, std::size_t>>;
};

} // namespace dingo::matrix
