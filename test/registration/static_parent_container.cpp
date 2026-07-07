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

#include <vector>

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
     child_static_binding_resolves_dependency_from_parent_static_graph) {
  using parent_bindings = bindings<dingo::bind<scope<shared>, storage<config>>>;
  using child_bindings = bindings<dingo::bind<scope<unique>, storage<service>>>;

  container<parent_bindings> parent;
  container<child_bindings, decltype(parent)> child(&parent);

  EXPECT_EQ(child.resolve<config &>().value, 1);
  EXPECT_EQ(child.resolve<service>().value, 1);
}

TEST(static_parent_container_test, child_static_binding_overrides_parent) {
  using parent_bindings = bindings<dingo::bind<scope<shared>, storage<config>>>;
  using child_bindings = bindings<
      dingo::bind<scope<shared>, storage<child_config>, interfaces<config>>,
      dingo::bind<scope<unique>, storage<service>>>;

  container<parent_bindings> parent;
  container<child_bindings, decltype(parent)> child(&parent);

  EXPECT_EQ(parent.resolve<config &>().value, 1);
  EXPECT_EQ(child.resolve<config &>().value, 2);
  EXPECT_EQ(child.resolve<service>().value, 2);
}

TEST(static_parent_container_test,
     child_ambiguity_does_not_fall_back_to_parent) {
  using parent_bindings = bindings<dingo::bind<scope<shared>, storage<config>>>;
  using child_bindings = bindings<
      dingo::bind<scope<shared>, storage<child_config>, interfaces<config>>,
      dingo::bind<scope<shared>, storage<second_child_config>,
                  interfaces<config>>>;

  container<parent_bindings> parent;
  container<child_bindings, decltype(parent)> child(&parent);

  EXPECT_THROW(child.resolve<config &>(), type_ambiguous_exception);
}

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
     child_collection_falls_back_to_parent_when_local_collection_is_empty) {
  using parent_bindings = bindings<dingo::bind<scope<shared>, storage<config>>>;
  using child_bindings = bindings<dingo::bind<scope<unique>, storage<service>>>;

  container<parent_bindings> parent;
  container<child_bindings, decltype(parent)> child(&parent);

  auto values = child.resolve<std::vector<config *>>();

  ASSERT_EQ(values.size(), 1u);
  EXPECT_EQ(values[0]->value, 1);
}

TEST(static_parent_container_test,
     child_collection_overrides_parent_collection) {
  using parent_bindings = bindings<dingo::bind<scope<shared>, storage<config>>>;
  using child_bindings = bindings<
      dingo::bind<scope<shared>, storage<child_config>, interfaces<config>>>;

  container<parent_bindings> parent;
  container<child_bindings, decltype(parent)> child(&parent);

  auto values = child.resolve<std::vector<config *>>();

  ASSERT_EQ(values.size(), 1u);
  EXPECT_EQ(values[0]->value, 2);
}

TEST(static_parent_container_test,
     static_child_resolves_dependency_from_parent_static_graph) {
  using parent_bindings = bindings<dingo::bind<scope<shared>, storage<config>>>;
  using child_bindings = bindings<dingo::bind<scope<unique>, storage<service>>>;

  static_container<parent_bindings> parent;
  static_container<child_bindings, decltype(parent)> child(&parent);

  EXPECT_EQ(child.resolve<config &>().value, 1);
  EXPECT_EQ(child.resolve<service>().value, 1);
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

TEST(static_parent_container_test, static_child_binding_overrides_parent) {
  using parent_bindings = bindings<dingo::bind<scope<shared>, storage<config>>>;
  using child_bindings = bindings<
      dingo::bind<scope<shared>, storage<child_config>, interfaces<config>>,
      dingo::bind<scope<unique>, storage<service>>>;

  static_container<parent_bindings> parent;
  static_container<child_bindings, decltype(parent)> child(&parent);

  EXPECT_EQ(parent.resolve<config &>().value, 1);
  EXPECT_EQ(child.resolve<config &>().value, 2);
  EXPECT_EQ(child.resolve<service>().value, 2);
}

TEST(
    static_parent_container_test,
    static_child_collection_falls_back_to_parent_when_local_collection_is_empty) {
  using parent_bindings = bindings<dingo::bind<scope<shared>, storage<config>>>;
  using child_bindings = bindings<dingo::bind<scope<unique>, storage<service>>>;

  static_container<parent_bindings> parent;
  static_container<child_bindings, decltype(parent)> child(&parent);

  auto values = child.resolve<std::vector<config *>>();

  ASSERT_EQ(values.size(), 1u);
  EXPECT_EQ(values[0]->value, 1);
}

TEST(static_parent_container_test,
     static_child_collection_overrides_parent_collection) {
  using parent_bindings = bindings<dingo::bind<scope<shared>, storage<config>>>;
  using child_bindings = bindings<
      dingo::bind<scope<shared>, storage<child_config>, interfaces<config>>>;

  static_container<parent_bindings> parent;
  static_container<child_bindings, decltype(parent)> child(&parent);

  auto values = child.resolve<std::vector<config *>>();

  ASSERT_EQ(values.size(), 1u);
  EXPECT_EQ(values[0]->value, 2);
}
