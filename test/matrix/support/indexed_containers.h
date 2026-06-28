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
#include <dingo/index/array.h>
#include <dingo/index/map.h>
#include <dingo/index/sequence.h>
#include <dingo/index/unordered_map.h>

#include <cstddef>
#include <string>
#include <vector>

namespace dingo::matrix {

struct indexed_container_traits : dingo::dynamic_container_traits {
  using index_definition_type = dingo::indexes<
      dingo::index<element_interface, std::size_t, dingo::index_type::map>>;
};

struct indexed_int_container_traits : dingo::dynamic_container_traits {
  using index_definition_type = dingo::indexes<
      dingo::index<element_interface, int, dingo::index_type::map>>;
};

struct indexed_string_container_traits : dingo::dynamic_container_traits {
  using index_definition_type = dingo::indexes<
      dingo::index<element_interface, std::string, dingo::index_type::map>>;
};

struct indexed_unordered_container_traits : dingo::dynamic_container_traits {
  using index_definition_type =
      dingo::indexes<dingo::index<element_interface, std::size_t,
                                  dingo::index_type::unordered_map>>;
};

struct indexed_int_unordered_container_traits
    : dingo::dynamic_container_traits {
  using index_definition_type = dingo::indexes<
      dingo::index<element_interface, int, dingo::index_type::unordered_map>>;
};

struct indexed_array_container_traits : dingo::dynamic_container_traits {
  using index_definition_type =
      dingo::indexes<dingo::index<element_interface, std::size_t,
                                  dingo::index_type::array<8>>>;
};

struct indexed_dsl_container_traits : dingo::dynamic_container_traits {
  using index_definition_type = dingo::indexes<
      dingo::index<dingo::interfaces<element_interface>,
                   dingo::key<std::size_t>, dingo::index_type::map>>;
};

struct indexed_vector_container_traits : dingo::dynamic_container_traits {
  using index_definition_type =
      dingo::indexes<dingo::index<element_interface, std::size_t,
                                  dingo::index_type::sequence<std::vector>>>;
};

} // namespace dingo::matrix
