//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include "matrix/support/values.h"

#include <array>
#include <memory>
#include <variant>

namespace dingo::matrix {

struct nested_variant_a {
  int value = 3;
};

struct nested_variant_b {
  int value = 7;
};

using nested_variant_type = std::variant<nested_variant_a, nested_variant_b>;
using shared_unique_value_type = std::shared_ptr<std::unique_ptr<value_type>>;
using shared_unique_array_value_type =
    std::shared_ptr<std::unique_ptr<value_type[]>>;
using unique_shared_value_type = std::unique_ptr<std::shared_ptr<value_type>>;
using variant_unique_value_type =
    std::variant<std::unique_ptr<value_type>, nested_variant_b>;
using variant_shared_value_type =
    std::variant<std::shared_ptr<value_type>, nested_variant_b>;
using variant_shared_unique_value_type =
    std::variant<std::shared_ptr<std::unique_ptr<value_type>>,
                 nested_variant_b>;
using unique_variant_value_type = std::unique_ptr<nested_variant_type>;
using shared_variant_value_type = std::shared_ptr<nested_variant_type>;
using array_variant_value_type =
    std::variant<std::array<value_type, 2>, nested_variant_b>;

inline shared_unique_value_type make_shared_unique_value() {
  return std::make_shared<std::unique_ptr<value_type>>(
      std::make_unique<value_type>());
}

inline shared_unique_array_value_type make_shared_unique_array_value() {
  return std::make_shared<std::unique_ptr<value_type[]>>(
      std::make_unique<value_type[]>(2));
}

inline unique_shared_value_type make_unique_shared_value() {
  return std::make_unique<std::shared_ptr<value_type>>(
      std::make_shared<value_type>());
}

inline variant_unique_value_type make_variant_unique_value() {
  return variant_unique_value_type(
      std::in_place_type<std::unique_ptr<value_type>>,
      std::make_unique<value_type>());
}

inline variant_shared_value_type make_variant_shared_value() {
  return variant_shared_value_type(
      std::in_place_type<std::shared_ptr<value_type>>,
      std::make_shared<value_type>());
}

inline variant_shared_unique_value_type make_variant_shared_unique_value() {
  return variant_shared_unique_value_type(
      std::in_place_type<std::shared_ptr<std::unique_ptr<value_type>>>,
      std::make_shared<std::unique_ptr<value_type>>(
          std::make_unique<value_type>()));
}

inline unique_variant_value_type make_unique_variant_value() {
  return std::make_unique<nested_variant_type>(
      std::in_place_type<nested_variant_a>);
}

inline shared_variant_value_type make_shared_variant_value() {
  return std::make_shared<nested_variant_type>(
      std::in_place_type<nested_variant_a>);
}

inline array_variant_value_type make_array_variant_value() {
  return array_variant_value_type(
      std::in_place_type<std::array<value_type, 2>>);
}

} // namespace dingo::matrix
