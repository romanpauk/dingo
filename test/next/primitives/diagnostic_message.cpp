//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

// The message of a diagnostic reported when a program is compiled names its
// subjects as a diagnostic reported at runtime does.

#include <dingo/core/primitives/diagnostic_message.h>
#include <dingo/core/vocabulary/compile_diagnostics.gen.h>

#include <gtest/gtest.h>

#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

namespace {

struct plain {};
template <typename Type> struct box {};

template <typename Type> std::string written() {
  char letters[256] = {};
  const std::size_t size = dingo::core::write_type_name<Type>(nullptr, 0);
  dingo::core::write_type_name<Type>(letters, 0);
  return std::string(letters, size);
}

template <typename... Types> void expect_same_spelling() {
  const std::pair<std::string, std::string> spellings[] = {
      {written<Types>(), dingo::core::type_name_of<Types>()}...};
  for (const auto &[compiled, resolved] : spellings) {
    EXPECT_EQ(compiled, resolved);
  }
}

TEST(next_diagnostic_message, writes_types_as_type_name_of_names_them) {
  expect_same_spelling<plain, plain &, const plain &, plain &&, plain *,
                       const plain *const, volatile plain, box<plain>,
                       box<box<plain>>, std::shared_ptr<plain>,
                       std::map<int, plain>, plain[2], int>();
}

struct text {
  static constexpr std::string_view pieces[] = {"from ", " to ", "."};
};

TEST(next_diagnostic_message, puts_the_subjects_in_the_placeholders) {
  constexpr dingo::core::formatted_message<text, plain, box<int> &> message;
  static_assert(message.size() == message.view().size());
  EXPECT_EQ(message.view(), "from " + dingo::core::type_name_of<plain>() +
                                " to " +
                                dingo::core::type_name_of<box<int> &>() + ".");
}

} // namespace
