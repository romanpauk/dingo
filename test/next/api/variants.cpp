//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

// A route through an alternative of a variant is served only when the variant
// holds that alternative: the route is chosen when it is derived, and the
// object it reaches into is checked when it is walked.

#include <dingo/container.h>
#include <dingo/static_container.h>
#include <dingo/storage/external.h>

#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <utility>
#include <variant>

namespace {

struct service {
  int value = 7;
};

using alternatives = std::variant<service, std::unique_ptr<service>>;

using dingo::scope;
using dingo::storage;

// A variant holding its second alternative.
alternatives holding_handle() {
  return alternatives(std::in_place_index<1>, std::make_unique<service>());
}

TEST(next_variants, serve_the_held_alternative) {
  alternatives stored = holding_handle();
  dingo::container<> container;
  container.register_type<scope<dingo::external>, storage<alternatives &>>(
      stored);
  EXPECT_EQ(&container.resolve<std::unique_ptr<service> &>(),
            &std::get<1>(stored));
}

TEST(next_variants, serve_through_the_held_alternative) {
  alternatives stored = holding_handle();
  dingo::container<> container;
  container.register_type<scope<dingo::external>, storage<alternatives &>>(
      stored);
  // The shortest route to the service takes the first alternative; the
  // variant holds the second, whose route reaches the service too.
  EXPECT_EQ(&container.resolve<service &>(), std::get<1>(stored).get());
  stored.emplace<0>();
  EXPECT_EQ(&container.resolve<service &>(), &std::get<0>(stored));
}

TEST(next_variants, refuse_an_alternative_not_held) {
  alternatives stored(std::in_place_index<0>);
  dingo::container<> container;
  container.register_type<scope<dingo::external>, storage<alternatives &>>(
      stored);
  // Only the second alternative reaches the handle, which the variant does
  // not hold.
  try {
    (void)container.resolve<std::unique_ptr<service> &>();
    FAIL() << "the variant does not hold the handle";
  } catch (const dingo::type_not_convertible_exception &exception) {
    EXPECT_NE(std::string(exception.what())
                  .find("holds another alternative than the one the request "
                        "is routed through"),
              std::string::npos)
        << exception.what();
  }
}

TEST(next_variants, check_the_held_alternative_each_time) {
  alternatives stored = holding_handle();
  dingo::container<> container;
  container.register_type<scope<dingo::external>, storage<alternatives &>>(
      stored);
  EXPECT_EQ(&container.resolve<std::unique_ptr<service> &>(),
            &std::get<1>(stored));
  stored.emplace<0>();
  EXPECT_THROW((void)container.resolve<std::unique_ptr<service> &>(),
               dingo::type_not_convertible_exception);
  EXPECT_EQ(container.resolve<service &>().value, 7);
}

TEST(next_variants, check_the_held_alternative_in_a_static_container) {
  alternatives stored = holding_handle();
  using bindings = dingo::bindings<
      dingo::bind<scope<dingo::external>, storage<alternatives &>>>;
  dingo::static_container<bindings> container(stored);
  EXPECT_EQ(&container.resolve<std::unique_ptr<service> &>(),
            &std::get<1>(stored));
  EXPECT_EQ(&container.resolve<service &>(), std::get<1>(stored).get());
  stored.emplace<0>();
  EXPECT_EQ(&container.resolve<service &>(), &std::get<0>(stored));
  EXPECT_THROW((void)container.resolve<std::unique_ptr<service> &>(),
               dingo::type_not_convertible_exception);
}

// Both alternatives reach the service, each by a handle of its own.
TEST(next_variants, serve_by_the_handle_held) {
  using handles =
      std::variant<std::unique_ptr<service>, std::shared_ptr<service>>;
  handles stored(std::in_place_index<1>, std::make_shared<service>());
  dingo::container<> container;
  container.register_type<scope<dingo::external>, storage<handles &>>(stored);
  EXPECT_EQ(&container.resolve<service &>(), std::get<1>(stored).get());
  stored.emplace<0>(std::make_unique<service>());
  EXPECT_EQ(&container.resolve<service &>(), std::get<0>(stored).get());
}

} // namespace
