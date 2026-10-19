//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

// The messages of the exceptions of the next implementation, which name the
// request and the types whose objects are being constructed.

#include <dingo/container.h>
#include <dingo/storage/shared.h>
#include <dingo/storage/unique.h>

#include <gtest/gtest.h>

#include <memory>
#include <string>

namespace {

using dingo::scope;
using dingo::storage;
using dingo::core::type_name_of;

template <typename Exception, typename Resolve>
std::string message_of(Resolve &&resolve) {
  try {
    resolve();
  } catch (const Exception &exception) {
    return exception.what();
  }
  return "no exception";
}

struct missing_type {};

struct dependency {};

struct consumer {
  explicit consumer(dependency &) {}
};

struct outer {
  explicit outer(std::unique_ptr<consumer>) {}
};

TEST(next_diagnostics, names_the_request_that_is_not_found) {
  dingo::container<> container;
  EXPECT_EQ(message_of<dingo::type_not_found_exception>(
                [&] { (void)container.resolve<missing_type>(); }),
            "type not found: " + type_name_of<missing_type>());
}

TEST(next_diagnostics, names_the_path_that_requires_a_missing_type) {
  dingo::container<> container;
  container.register_type<scope<dingo::unique>,
                          storage<std::unique_ptr<consumer>>>();
  container.register_type<scope<dingo::shared>, storage<outer>>();
  EXPECT_EQ(message_of<dingo::type_not_found_exception>(
                [&] { (void)container.resolve<outer &>(); }),
            "type not found: " + type_name_of<dependency &>() +
                " (required by " + type_name_of<outer>() + " -> " +
                type_name_of<consumer>() + ")");
}

TEST(next_diagnostics, names_the_request_that_is_not_convertible) {
  dingo::container<> container;
  container.register_type<scope<dingo::shared>, storage<dependency>>();
  EXPECT_EQ(message_of<dingo::type_not_convertible_exception>(
                [&] { (void)container.resolve<dependency &&>(); }),
            "type is not convertible to " + type_name_of<dependency &&>() +
                " from " + type_name_of<dependency>());
}

TEST(next_diagnostics, names_the_request_that_is_ambiguous) {
  dingo::container<> container;
  container.register_type<scope<dingo::shared>, storage<dependency>>();
  container.register_type<scope<dingo::shared>,
                          storage<std::shared_ptr<dependency>>>();
  EXPECT_EQ(message_of<dingo::type_ambiguous_exception>(
                [&] { (void)container.resolve<dependency &>(); }),
            "type resolution is ambiguous: " + type_name_of<dependency &>());
}

struct second;

struct first {
  explicit first(second &next) : next(next) {}
  second &next;
};

struct second {
  explicit second(first &previous) : previous(previous) {}
  first &previous;
};

TEST(next_diagnostics, names_the_path_of_a_recursion) {
  dingo::container<> container;
  container.register_type<scope<dingo::shared>, storage<first>>();
  container.register_type<scope<dingo::shared>, storage<second>>();
  EXPECT_EQ(message_of<dingo::type_recursion_exception>(
                [&] { (void)container.resolve<first &>(); }),
            "recursive dependency detected: " + type_name_of<first>() + " -> " +
                type_name_of<second>() + " -> " + type_name_of<first>());
}

} // namespace
