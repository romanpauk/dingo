//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include "matrix/fixtures/variant_types.h"

#include <array>
#include <memory>
#include <optional>
#include <type_traits>
#include <variant>

namespace dingo::matrix {

struct dependency_regular {
  int marker() const { return 3; }
};

struct dependency_copy_only {
  dependency_copy_only() = default;
  dependency_copy_only(const dependency_copy_only &) = default;
  dependency_copy_only(dependency_copy_only &&) = delete;

  int marker() const { return 3; }
};

struct dependency_move_only {
  dependency_move_only() = default;
  dependency_move_only(const dependency_move_only &) = delete;
  dependency_move_only(dependency_move_only &&) = default;

  int marker() const { return 3; }
};

struct dependency_composition_mixed_anchor {};

template <typename Type> Type make_dependency_composition() { return Type{}; }

template <typename Type> using dependency_array = Type[2];

template <typename Type>
using dependency_const_pointer = std::add_pointer_t<std::add_const_t<Type>>;

template <typename Type>
using dependency_variant = std::variant<Type, variant_b>;

} // namespace dingo::matrix
