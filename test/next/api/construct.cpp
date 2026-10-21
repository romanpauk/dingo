//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

// The construction of an unregistered object: inside the layers of its
// shape, by a named constructor of a leaf that converts to the stored one,
// and with a dependency constructed by the constructor its type declares.

#include <dingo/container.h>
#include <dingo/registration/constructor.h>
#include <dingo/storage/external.h>
#include <dingo/type/auto_constructible.h>

#include <gtest/gtest.h>

#include <optional>
#include <variant>

namespace {

using dingo::scope;
using dingo::storage;

struct selected_alternative {
  explicit selected_alternative(int init) : value(init) {}
  int value;
};

struct unselected_alternative {};

TEST(next_construct, constructs_a_named_alternative_inside_layers) {
  using alternative =
      std::variant<selected_alternative, unselected_alternative>;
  using nested = std::optional<alternative>;
  using deeply_nested = std::optional<nested>;

  dingo::container<> container;
  container.register_type<scope<dingo::external>, storage<int>>(7);

  auto value =
      container.construct<nested, dingo::constructor<selected_alternative>>();
  ASSERT_TRUE(value.has_value());
  ASSERT_TRUE(std::holds_alternative<selected_alternative>(*value));
  EXPECT_EQ(std::get<selected_alternative>(*value).value, 7);

  auto deeply_nested_value =
      container
          .construct<deeply_nested, dingo::constructor<selected_alternative>>();
  ASSERT_TRUE(deeply_nested_value.has_value());
  ASSERT_TRUE(deeply_nested_value->has_value());
  ASSERT_TRUE(
      std::holds_alternative<selected_alternative>(**deeply_nested_value));
  EXPECT_EQ(std::get<selected_alternative>(**deeply_nested_value).value, 7);
}

// A pointer to a variant points to a variant that holds the named alternative.
TEST(next_construct, DISABLED_constructs_a_named_alternative_behind_a_pointer) {
  using alternative =
      std::variant<selected_alternative, unselected_alternative>;

  dingo::container<> container;
  container.register_type<scope<dingo::external>, storage<int>>(7);

  auto *pointer =
      container
          .construct<alternative *, dingo::constructor<selected_alternative>>();
  ASSERT_NE(pointer, nullptr);
  ASSERT_TRUE(std::holds_alternative<selected_alternative>(*pointer));
  EXPECT_EQ(std::get<selected_alternative>(*pointer).value, 7);
  delete pointer;
}

TEST(next_construct, a_named_leaf_converts_to_the_alternative_it_names) {
  dingo::container<> container;
  container.register_type<scope<dingo::external>, storage<int>>(7);
  // The leaf is made from the int the container holds, and selects the
  // alternative of its type, though an int would select the other one.
  auto value =
      container
          .construct<std::variant<int, long>, dingo::constructor<long(int)>>();
  ASSERT_TRUE(std::holds_alternative<long>(value));
  EXPECT_EQ(std::get<long>(value), 7);
}

// Neither copyable nor movable, so it is constructed only in place.
struct immovable {
  explicit immovable(int init) : value(init) {}
  immovable(const immovable &) = delete;
  immovable(immovable &&) = delete;
  int value;
};

TEST(next_construct, constructs_the_element_of_an_optional_in_place) {
  dingo::container<> container;
  container.register_type<scope<dingo::external>, storage<int>>(11);
  auto value = container.construct<std::optional<immovable>>();
  ASSERT_TRUE(value.has_value());
  EXPECT_EQ(value->value, 11);
}

struct selected_dependency {
  DINGO_CONSTRUCTOR(selected_dependency()) : value(1) {}
  explicit selected_dependency(int) : value(2) {}
  int value;
};

struct selected_consumer {
  explicit selected_consumer(selected_dependency dependency)
      : value(dependency.value) {}
  int value;
};

} // namespace

template <>
struct dingo::is_auto_constructible<selected_dependency> : std::true_type {};

namespace {

TEST(next_construct, auto_constructs_a_dependency_by_its_declared_constructor) {
  dingo::container<> container;
  container.register_type<scope<dingo::external>, storage<int>>(5);
  EXPECT_EQ(container.construct<selected_consumer>().value, 1);
}

} // namespace
