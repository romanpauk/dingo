//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

// The names of types in diagnostics: the compiler's spelling of a type that is
// not a pointer or a reference, with its cv-qualifiers in front, and pointers
// and references composed around it.

#include <dingo/primitives/type_name.h>

#include <gtest/gtest.h>

#include <initializer_list>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace next_type_name_types {
struct value {};

struct functions {
  static void call(value & /*argument*/) {}
};

struct callable {
  void operator()(value & /*argument*/) const {}
};
} // namespace next_type_name_types

namespace {

using dingo::core::spelled_type_name;
using dingo::core::type_name_of;
using next_type_name_types::value;

#ifdef _MSC_VER
constexpr std::string_view value_name = "struct next_type_name_types::value";
#else
constexpr std::string_view value_name = "next_type_name_types::value";
#endif

std::string with_value(std::string_view before, std::string_view after) {
  return std::string(before) + std::string(value_name) + std::string(after);
}

// Whether `name` holds the `parts` in their order.
void expect_parts(const std::string &name,
                  std::initializer_list<std::string_view> parts) {
  std::size_t offset = 0;
  for (const auto part : parts) {
    const auto found = name.find(part, offset);
    ASSERT_NE(found, std::string::npos)
        << "name: " << name << "\nmissing part: " << part;
    offset = found + part.size();
  }
}

static_assert(spelled_type_name<value>() == value_name);
static_assert(dingo::core::detail::find_text("abcdef", "cd") == 2);
static_assert(dingo::core::detail::find_text("abcdef", "gh") == 6);

TEST(next_type_name, spells_a_class_as_the_compiler_does) {
  EXPECT_EQ(type_name_of<value>(), value_name);
}

TEST(next_type_name, keeps_the_extent_of_an_array) {
  EXPECT_NE(type_name_of<int[3]>().find("[3]"), std::string::npos);
}

TEST(next_type_name, puts_the_qualifiers_of_a_class_in_front) {
  EXPECT_EQ(type_name_of<const value>(), with_value("const ", ""));
  EXPECT_EQ(type_name_of<const volatile value>(),
            with_value("const volatile ", ""));
  EXPECT_EQ(type_name_of<const value &>(), with_value("const ", "&"));
  EXPECT_EQ(type_name_of<value &&>(), with_value("", "&&"));
}

TEST(next_type_name, composes_pointers_around_their_pointee) {
  EXPECT_EQ(type_name_of<value *>(), with_value("", "*"));
  EXPECT_EQ(type_name_of<value **>(), with_value("", "**"));
  EXPECT_EQ(type_name_of<const value *const &>(),
            with_value("const ", "* const&"));
  EXPECT_EQ(type_name_of<const value *const *volatile>(),
            with_value("const ", "* const* volatile"));
}

TEST(next_type_name, names_a_wrapper_with_its_element) {
  const std::string shared = type_name_of<std::shared_ptr<value>>();
  EXPECT_NE(shared.find("shared_ptr"), std::string::npos);
  EXPECT_NE(shared.find(value_name), std::string::npos);
  const std::string optional = type_name_of<std::optional<value>>();
  EXPECT_NE(optional.find("optional"), std::string::npos);
  EXPECT_NE(optional.find(value_name), std::string::npos);
}

TEST(next_type_name, names_the_parts_of_a_function_type) {
  expect_parts(type_name_of<void(int, const value &)>(),
               {"void", "int", "const", value_name, "&"});
  expect_parts(type_name_of<void(value &)>(), {"void", value_name, "&"});
  expect_parts(type_name_of<int (*)(double, value *)>(),
               {"int", "double", value_name, "*", "*"});
  expect_parts(type_name_of<decltype(&next_type_name_types::functions::call)>(),
               {"void", value_name, "&", "*"});
  expect_parts(
      type_name_of<decltype(&next_type_name_types::callable::operator())>(),
      {"void", "callable::", "*", value_name, "&", "const"});
}

} // namespace
