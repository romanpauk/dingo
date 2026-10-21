//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

// Constructor injection of the next implementation: the factory constructs a
// leaf by its detected constructor and resolves its arguments from the
// container.

#include <dingo/container.h>
#include <dingo/static_container.h>
#include <dingo/storage/shared.h>

#include <gtest/gtest.h>

#include <memory>
#include <optional>

namespace {

using dingo::scope;
using dingo::storage;

// A class with members and no constructor arguments declares its default
// constructor; as an aggregate, it would take its members as arguments.
struct config {
  config() {}
  int value = 3;
};

struct logger {
  logger() {}
  int lines = 0;
};

struct database {
  database(config &init_settings, const logger &init_log)
      : settings(init_settings), log(init_log) {}
  config &settings;
  const logger &log;
};

// Neither copyable nor movable, so it is constructed only in place.
struct service {
  service(database &init_db, std::shared_ptr<logger> init_log, config init_copy)
      : db(init_db), log(std::move(init_log)), copy(init_copy) {}
  service(const service &) = delete;
  service &operator=(const service &) = delete;
  database &db;
  std::shared_ptr<logger> log;
  config copy;
};

// An aggregate takes its members as its constructor arguments.
struct report {
  config &settings;
  database *db;
};

template <typename Container> void check_graph(Container &container) {
  service &object = container.template resolve<service &>();
  EXPECT_EQ(&object.db, &container.template resolve<database &>());
  EXPECT_EQ(&object.db.settings, &container.template resolve<config &>());
  EXPECT_EQ(&object.db.log, container.template resolve<logger *>());
  EXPECT_EQ(object.log.get(), container.template resolve<logger *>());
  EXPECT_EQ(object.copy.value, 3);
  EXPECT_EQ(&container.template resolve<service &>(), &object);
}

TEST(next_construction, injects_constructor_arguments) {
  dingo::container<> container;
  container.register_type<scope<dingo::shared>, storage<config>>();
  container
      .register_type<scope<dingo::shared>, storage<std::shared_ptr<logger>>>();
  container.register_type<scope<dingo::shared>, storage<database>>();
  container
      .register_type<scope<dingo::shared>, storage<std::unique_ptr<service>>>();
  check_graph(container);
}

TEST(next_construction, injects_constructor_arguments_statically) {
  using bindings = dingo::bindings<
      dingo::bind<scope<dingo::shared>, storage<config>>,
      dingo::bind<scope<dingo::shared>, storage<std::shared_ptr<logger>>>,
      dingo::bind<scope<dingo::shared>, storage<database>>,
      dingo::bind<scope<dingo::shared>, storage<std::unique_ptr<service>>>>;
  dingo::static_container<bindings> container;
  check_graph(container);
}

TEST(next_construction, constructs_the_element_of_a_layer_in_place) {
  dingo::container<> container;
  container.register_type<scope<dingo::shared>, storage<config>>();
  container
      .register_type<scope<dingo::shared>, storage<std::shared_ptr<logger>>>();
  container
      .register_type<scope<dingo::shared>, storage<std::optional<database>>>();
  container
      .register_type<scope<dingo::shared>, storage<std::shared_ptr<service>>>();
  service &object = container.resolve<service &>();
  EXPECT_EQ(&object.db, &container.resolve<database &>());
}

TEST(next_construction, initializes_an_aggregate) {
  dingo::container<> container;
  container.register_type<scope<dingo::shared>, storage<config>>();
  container.register_type<scope<dingo::shared>, storage<logger>>();
  container.register_type<scope<dingo::shared>, storage<database>>();
  container.register_type<scope<dingo::shared>, storage<report>>();
  report &object = container.resolve<report &>();
  EXPECT_EQ(&object.settings, &container.resolve<config &>());
  EXPECT_EQ(object.db, container.resolve<database *>());
}

struct second;

struct first {
  explicit first(second &init_next) : next(init_next) {}
  second &next;
};

struct second {
  explicit second(first &init_previous) : previous(init_previous) {}
  first &previous;
};

TEST(next_construction, reports_a_recursion) {
  dingo::container<> container;
  container.register_type<scope<dingo::shared>, storage<first>>();
  container.register_type<scope<dingo::shared>, storage<second>>();
  EXPECT_THROW(container.resolve<first &>(), dingo::type_recursion_exception);
  // The failed construction leaves the bindings unresolved.
  EXPECT_THROW(container.resolve<second &>(), dingo::type_recursion_exception);
}

struct failing {
  static inline int attempts = 0;
  failing() {
    if (attempts++ == 0) {
      throw std::runtime_error("first attempt");
    }
  }
};

TEST(next_construction, retries_after_a_failed_construction) {
  dingo::container<> container;
  container.register_type<scope<dingo::shared>, storage<failing>>();
  failing::attempts = 0;
  EXPECT_THROW(container.resolve<failing &>(), std::runtime_error);
  failing &object = container.resolve<failing &>();
  EXPECT_EQ(&container.resolve<failing &>(), &object);
  EXPECT_EQ(failing::attempts, 2);
}

} // namespace
