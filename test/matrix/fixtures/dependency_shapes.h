//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include "matrix/fixtures/variant_types.h"

#include <array>
#include <cstddef>
#include <memory>
#include <optional>
#include <type_traits>
#include <utility>
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

template <typename Type>
inline std::size_t dependency_composition_factory_calls = 0;

template <typename Type, bool OwnPointer = false>
struct dependency_composition_factory {
  static Type make() { return Type{}; }
};

template <typename Type, bool OwnPointer>
struct dependency_composition_factory<Type *, OwnPointer> {
  static Type *make() {
    using pointee_type = std::remove_cv_t<Type>;
    if constexpr (OwnPointer) {
      return new pointee_type{
          dependency_composition_factory<pointee_type>::make()};
    } else {
      static pointee_type value =
          dependency_composition_factory<pointee_type>::make();
      return &value;
    }
  }
};

template <typename Type, bool OwnPointer>
struct dependency_composition_factory<std::shared_ptr<Type>, OwnPointer> {
  static std::shared_ptr<Type> make() {
    if constexpr (std::is_move_constructible_v<Type>) {
      auto value = dependency_composition_factory<Type>::make();
      return std::make_shared<Type>(std::move(value));
    } else if constexpr (std::is_copy_constructible_v<Type>) {
      auto value = dependency_composition_factory<Type>::make();
      return std::make_shared<Type>(value);
    } else {
      return std::make_shared<Type>();
    }
  }
};

template <typename Type, bool OwnPointer>
struct dependency_composition_factory<std::unique_ptr<Type>, OwnPointer> {
  static std::unique_ptr<Type> make() {
    if constexpr (std::is_move_constructible_v<Type>) {
      auto value = dependency_composition_factory<Type>::make();
      return std::make_unique<Type>(std::move(value));
    } else if constexpr (std::is_copy_constructible_v<Type>) {
      auto value = dependency_composition_factory<Type>::make();
      return std::make_unique<Type>(value);
    } else {
      return std::make_unique<Type>();
    }
  }
};

template <typename Type, bool OwnPointer>
struct dependency_composition_factory<std::optional<Type>, OwnPointer> {
  static std::optional<Type> make() {
    if constexpr (std::is_move_constructible_v<Type>) {
      auto value = dependency_composition_factory<Type>::make();
      return std::optional<Type>(std::in_place, std::move(value));
    } else if constexpr (std::is_copy_constructible_v<Type>) {
      auto value = dependency_composition_factory<Type>::make();
      return std::optional<Type>(std::in_place, value);
    } else {
      return std::optional<Type>(std::in_place);
    }
  }
};

template <typename Type, std::size_t Size, bool OwnPointer>
struct dependency_composition_factory<std::array<Type, Size>, OwnPointer> {
private:
  template <std::size_t... Indices>
  static std::array<Type, Size> make(std::index_sequence<Indices...>) {
    return {((void)Indices, dependency_composition_factory<Type>::make())...};
  }

public:
  static std::array<Type, Size> make() {
    return make(std::make_index_sequence<Size>{});
  }
};

template <bool OwnPointer, typename First, typename... Rest>
struct dependency_composition_factory<std::variant<First, Rest...>,
                                      OwnPointer> {
  static std::variant<First, Rest...> make() {
    if constexpr (std::is_move_constructible_v<First>) {
      auto value = dependency_composition_factory<First>::make();
      return std::variant<First, Rest...>(std::in_place_type<First>,
                                          std::move(value));
    } else if constexpr (std::is_copy_constructible_v<First>) {
      auto value = dependency_composition_factory<First>::make();
      return std::variant<First, Rest...>(std::in_place_type<First>, value);
    } else {
      return {};
    }
  }
};

template <typename Type, bool OwnPointer = false>
Type make_dependency_composition() {
  ++dependency_composition_factory_calls<Type>;
  return dependency_composition_factory<Type, OwnPointer>::make();
}

template <typename Type> using dependency_array = Type[2];

template <typename Type>
using dependency_const_pointer = std::add_pointer_t<std::add_const_t<Type>>;

template <typename Type>
using dependency_variant = std::variant<Type, variant_b>;

} // namespace dingo::matrix
