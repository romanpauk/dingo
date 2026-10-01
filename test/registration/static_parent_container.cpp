//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#include <dingo/container.h>
#include <dingo/static_container.h>
#include <dingo/storage/shared.h>
#include <dingo/storage/unique.h>

#include <gtest/gtest.h>

using namespace dingo;

namespace {
struct config {
  config() : value(1) {}

  int value;
};

struct service {
  explicit service(config &cfg) : value(cfg.value) {}

  int value;
};

struct child_config : config {
  child_config() { value = 2; }
};

struct second_child_config : config {
  second_child_config() { value = 3; }
};

struct parent_temporary {
  parent_temporary() : value(4) {}
  ~parent_temporary() {}

  int value;
};
} // namespace

TEST(static_parent_container_test,
     child_dependency_ambiguity_does_not_fall_back_to_parent) {
  using parent_bindings = bindings<dingo::bind<scope<shared>, storage<config>>>;
  using child_bindings = bindings<
      dingo::bind<scope<shared>, storage<child_config>, interfaces<config>>,
      dingo::bind<scope<shared>, storage<second_child_config>,
                  interfaces<config>>,
      dingo::bind<scope<unique>, storage<service>>>;

  container<parent_bindings> parent;
  container<child_bindings, decltype(parent)> child(&parent);

  EXPECT_THROW(child.resolve<service>(), type_ambiguous_exception);
}

TEST(static_parent_container_test,
     static_child_context_is_sized_for_parent_temporaries) {
  using parent_bindings =
      bindings<dingo::bind<scope<unique>, storage<parent_temporary>>>;
  using child_bindings = bindings<>;

  static_container<parent_bindings> parent;
  static_container<child_bindings, decltype(parent)> child(&parent);

  EXPECT_EQ(child.resolve<parent_temporary>().value, 4);
}

TEST(static_parent_container_test,
     static_child_context_is_sized_for_grandparent_temporaries) {
  using grandparent_bindings =
      bindings<dingo::bind<scope<unique>, storage<parent_temporary>>>;
  using parent_bindings = bindings<>;
  using child_bindings = bindings<>;

  static_container<grandparent_bindings> grandparent;
  static_container<parent_bindings, decltype(grandparent)> parent(&grandparent);
  static_container<child_bindings, decltype(parent)> child(&parent);

  EXPECT_EQ(child.resolve<parent_temporary>().value, 4);
}

namespace {
struct retained_interface {
  virtual ~retained_interface() = default;

  int value = 5;
};

struct retained_service : retained_interface {};

struct retained_dependent : retained_interface {
  explicit retained_dependent(parent_temporary temporary) {
    value = temporary.value;
  }
};

struct child_only {
  child_only() : value(6) {}

  int value;
};
} // namespace

TEST(static_parent_container_test,
     static_empty_child_resolves_parent_retained_conversion) {
  using parent_bindings =
      bindings<dingo::bind<scope<shared>, storage<retained_service>,
                           interfaces<retained_interface>>>;
  using child_bindings = bindings<>;

  static_container<parent_bindings> parent;
  static_container<child_bindings, decltype(parent)> child(&parent);

  auto &resolved = child.resolve<retained_interface &>();
  EXPECT_EQ(resolved.value, 5);
  EXPECT_EQ(&resolved, &child.resolve<retained_interface &>());
  EXPECT_EQ(&resolved, &parent.resolve<retained_interface &>());
}

TEST(static_parent_container_test,
     static_non_empty_child_resolves_parent_retained_conversion) {
  using parent_bindings =
      bindings<dingo::bind<scope<shared>, storage<retained_service>,
                           interfaces<retained_interface>>>;
  using child_bindings =
      bindings<dingo::bind<scope<unique>, storage<child_only>>>;

  static_container<parent_bindings> parent;
  static_container<child_bindings, decltype(parent)> child(&parent);

  auto &resolved = child.resolve<retained_interface &>();
  EXPECT_EQ(resolved.value, 5);
  EXPECT_EQ(&resolved, &child.resolve<retained_interface &>());
  EXPECT_EQ(&resolved, &parent.resolve<retained_interface &>());
  EXPECT_EQ(child.resolve<child_only>().value, 6);
}

TEST(static_parent_container_test,
     static_non_empty_child_retains_parent_dependency_temporaries) {
  using parent_bindings =
      bindings<dingo::bind<scope<unique>, storage<parent_temporary>>,
               dingo::bind<scope<shared>, storage<retained_dependent>,
                           interfaces<retained_interface>>>;
  using child_bindings =
      bindings<dingo::bind<scope<unique>, storage<child_only>>>;

  static_container<parent_bindings> parent;
  static_container<child_bindings, decltype(parent)> child(&parent);

  auto &resolved = child.resolve<retained_interface &>();
  EXPECT_EQ(resolved.value, 4);
  EXPECT_EQ(&resolved, &parent.resolve<retained_interface &>());
}

TEST(static_parent_container_test,
     static_non_empty_child_resolves_grandparent_retained_conversion) {
  using grandparent_bindings =
      bindings<dingo::bind<scope<shared>, storage<retained_service>,
                           interfaces<retained_interface>>>;
  using parent_bindings =
      bindings<dingo::bind<scope<unique>, storage<child_only>>>;
  using child_bindings = bindings<dingo::bind<scope<unique>, storage<config>>>;

  static_container<grandparent_bindings> grandparent;
  static_container<parent_bindings, decltype(grandparent)> parent(&grandparent);
  static_container<child_bindings, decltype(parent)> child(&parent);

  auto &resolved = child.resolve<retained_interface &>();
  EXPECT_EQ(resolved.value, 5);
  EXPECT_EQ(&resolved, &grandparent.resolve<retained_interface &>());
}
