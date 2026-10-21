//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

// Constructor detection: the constructor a factory calls for a type, found
// from the largest arity down, or the one the type declares, and its
// invocation with an argument resolved for every parameter.

#include <dingo/core/primitives/constructor.h>
#include <dingo/registration/constructor.h>

#include <gtest/gtest.h>

#include <array>
#include <memory>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace {

using dingo::core::constructor_kind;
using dingo::core::declared_constructor;
using dingo::core::detected_constructor;
using dingo::core::invoke_constructor;
using dingo::core::type_list;

struct two_values {
  two_values(int init_integer, float init_real)
      : integer(init_integer), real(init_real) {}
  int integer;
  float real;
};

struct two_or_three_values {
  two_or_three_values(int, float) {}
  two_or_three_values(int, float, double) {}
};

struct defaulted {};

struct empty_base {};
struct derived_aggregate : empty_base {};

// A default constructor, so the copy constructor is the only one of arity one.
struct copyable_only {
  copyable_only() {}
};

struct annotated_aggregate {
  using dingo_constructor_type = dingo::constructor<annotated_aggregate(int)>;
};

struct typedef_selected {
  typedef_selected(int, int, int) {}
  DINGO_CONSTRUCTOR(typedef_selected(double, const char *)) {}
};

TEST(next_constructor, detects_the_constructor_of_the_largest_arity) {
  static_assert(detected_constructor<two_values>::arity == 2);
  static_assert(detected_constructor<two_values>::kind ==
                constructor_kind::concrete);
  static_assert(detected_constructor<two_or_three_values>::arity == 3);
  // The search starts at its limit, so a limit below the largest arity finds
  // the largest constructor within it.
  static_assert(dingo::core::detail::find_arity<two_or_three_values, 2>() == 2);
  static_assert(dingo::core::detail::find_arity<two_values, 1>() ==
                dingo::core::detail::invalid_arity);
  static_assert(detected_constructor<defaulted>::arity == 0);
  // An aggregate with a base takes the base as its first initializer.
  static_assert(detected_constructor<derived_aggregate>::arity == 1);
  // The copy constructor is never detected.
  static_assert(detected_constructor<copyable_only>::arity == 0);
}

TEST(next_constructor, value_initializes_a_scalar) {
  static_assert(detected_constructor<int>::arity == 0);
  static_assert(detected_constructor<int>::kind == constructor_kind::concrete);
  static_assert(detected_constructor<const int>::arity == 0);
  static_assert(detected_constructor<volatile int>::arity == 0);
  static_assert(detected_constructor<int *>::arity == 0);
}

TEST(next_constructor, takes_the_declared_constructor_arguments) {
  static_assert(!declared_constructor<two_values>::declared);
  static_assert(declared_constructor<typedef_selected>::declared);
  static_assert(
      std::is_same_v<declared_constructor<typedef_selected>::arguments,
                     type_list<double, const char *>>);
  static_assert(declared_constructor<annotated_aggregate>::declared);
  static_assert(
      std::is_same_v<declared_constructor<annotated_aggregate>::arguments,
                     type_list<int>>);
}

// Neither copyable, so an element is constructed only from the prvalue its
// argument resolves.
struct move_only {
  explicit move_only(int init) : value(init) {}
  move_only(const move_only &) = delete;
  move_only(move_only &&) = default;
  int value;
};

// Resolves every argument by its type: a handle of the next number, a
// move-only value, or an array, which it constructs by its detected
// constructor in turn.
struct counting_resolver {
  template <typename Type> Type argument() {
    if constexpr (std::is_same_v<Type, std::unique_ptr<int>>) {
      return std::make_unique<int>(++numbers);
    } else if constexpr (std::is_same_v<Type, move_only>) {
      ++resolutions;
      return move_only{7};
    } else {
      return invoke_constructor<Type>(*this, [](auto &&...args) {
        return Type{std::forward<decltype(args)>(args)...};
      });
    }
  }

  // The arrays of the tests take no reference.
  template <typename Request> Request resolve() {
    throw std::logic_error("a reference is not resolved");
  }

  int numbers = 0;
  int resolutions = 0;
};

template <typename Type> Type construct(counting_resolver &resolver) {
  return resolver.argument<Type>();
}

TEST(next_constructor, constructs_an_array_of_move_only_handles_in_order) {
  using array_type = std::array<std::unique_ptr<int>, 2>;
  static_assert(detected_constructor<array_type>::arity == 2);
  counting_resolver resolver;
  array_type values = construct<array_type>(resolver);
  ASSERT_NE(values[0], nullptr);
  ASSERT_NE(values[1], nullptr);
  EXPECT_EQ(*values[0], 1);
  EXPECT_EQ(*values[1], 2);
}

TEST(next_constructor, constructs_a_nested_array_of_move_only_handles) {
  using inner_array_type = std::array<std::unique_ptr<int>, 2>;
  using array_type = std::array<inner_array_type, 2>;
  static_assert(detected_constructor<array_type>::arity == 2);
  counting_resolver resolver;
  array_type values = construct<array_type>(resolver);
  EXPECT_EQ(*values[0][0], 1);
  EXPECT_EQ(*values[0][1], 2);
  EXPECT_EQ(*values[1][0], 3);
  EXPECT_EQ(*values[1][1], 4);
}

TEST(next_constructor, resolves_every_move_only_element_of_an_array) {
  using array_type = std::array<move_only, 2>;
  counting_resolver resolver;
  array_type values = construct<array_type>(resolver);
  EXPECT_EQ(values[0].value, 7);
  EXPECT_EQ(values[1].value, 7);
  EXPECT_EQ(resolver.resolutions, 2);
}

TEST(next_constructor, resolves_every_move_only_element_of_a_nested_array) {
  using array_type = std::array<std::array<move_only, 2>, 2>;
  counting_resolver resolver;
  array_type values = construct<array_type>(resolver);
  EXPECT_EQ(values[0][0].value, 7);
  EXPECT_EQ(values[0][1].value, 7);
  EXPECT_EQ(values[1][0].value, 7);
  EXPECT_EQ(values[1][1].value, 7);
  EXPECT_EQ(resolver.resolutions, 4);
}

} // namespace
