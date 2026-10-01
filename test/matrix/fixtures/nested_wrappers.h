//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include "matrix/fixtures/values.h"

#include "support/custom_wrappers.h"

#include <array>
#include <memory>
#include <optional>
#include <variant>

namespace dingo::matrix {

struct nested_variant_a {
  int value = 3;
};

struct nested_variant_b {
  int value = 7;
};

// A leaf that can be moved but not copied, behind the same wrappers.
struct move_only_leaf_type {
  move_only_leaf_type() = default;
  move_only_leaf_type(move_only_leaf_type &&) = default;
  move_only_leaf_type &operator=(move_only_leaf_type &&) = default;

  bool valid() const { return marker_ == 3; }

private:
  int marker_ = 3;
};

inline bool is_constructed_value(const move_only_leaf_type &value) {
  return value.valid();
}

using nested_variant_type = std::variant<nested_variant_a, nested_variant_b>;
using shared_unique_value_type = std::shared_ptr<std::unique_ptr<value_type>>;
using shared_unique_array_value_type =
    std::shared_ptr<std::unique_ptr<value_type[]>>;
using shared_unique_move_only_type =
    std::shared_ptr<std::unique_ptr<move_only_leaf_type>>;
using shared_optional_value_type = std::shared_ptr<std::optional<value_type>>;
using shared_optional_move_only_type =
    std::shared_ptr<std::optional<move_only_leaf_type>>;
using shared_const_value_type = std::shared_ptr<const value_type>;
using shared_custom_optional_value_type =
    std::shared_ptr<test_optional<value_type>>;
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

inline shared_unique_move_only_type make_shared_unique_move_only() {
  return std::make_shared<std::unique_ptr<move_only_leaf_type>>(
      std::make_unique<move_only_leaf_type>());
}

inline shared_optional_value_type make_shared_optional_value() {
  return std::make_shared<std::optional<value_type>>(std::in_place);
}

inline shared_optional_move_only_type make_shared_optional_move_only() {
  return std::make_shared<std::optional<move_only_leaf_type>>(std::in_place);
}

inline shared_const_value_type make_shared_const_value() {
  return std::make_shared<const value_type>();
}

inline shared_custom_optional_value_type make_shared_custom_optional_value() {
  return std::make_shared<test_optional<value_type>>(
      type_traits<test_optional<value_type>>::make());
}

inline value_type *make_value_pointer() { return new value_type; }

inline move_only_leaf_type *make_move_only_pointer() {
  return new move_only_leaf_type;
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
