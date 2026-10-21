//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

// Children of static parents in the next implementation: a child that names
// no binding of a request asks its parent, whose requests construct and keep
// their objects in the parent, through any number of generations, and an
// ambiguity among the bindings of a child is never answered by its parent.

#include <dingo/container.h>
#include <dingo/lookup/lookup.h>
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

struct many_traits : dynamic_container_traits {
  using lookup_definition_type = lookups<collection<config>>;
};
} // namespace

// Two static bindings of the child are rejected when the child is formed
// unless the domain is declared many, and with many a request among them is
// rejected when it is compiled (spec: behavior_changes.
// duplicates_rejected_in_both_domains). A static and a runtime binding of the
// child make the dependency of service ambiguous when it is resolved, and the
// child does not ask its parent instead.
TEST(next_static_parents,
     child_dependency_ambiguity_does_not_fall_back_to_parent) {
  using parent_bindings = bindings<dingo::bind<scope<shared>, storage<config>>>;
  using child_bindings = bindings<
      dingo::bind<scope<shared>, storage<child_config>, interfaces<config>>,
      dingo::bind<scope<unique>, storage<service>>>;

  container<parent_bindings> parent;
  container<child_bindings, many_traits, decltype(parent)> child(&parent);
  EXPECT_EQ(child.resolve<service>().value, 2);
  child.register_type<scope<shared>, storage<second_child_config>,
                      interfaces<config>>();

  EXPECT_THROW(child.resolve<service>(), type_ambiguous_exception);
  EXPECT_EQ(parent.resolve<config &>().value, 1);
}

TEST(next_static_parents,
     static_child_context_is_sized_for_parent_temporaries) {
  using parent_bindings =
      bindings<dingo::bind<scope<unique>, storage<parent_temporary>>>;
  using child_bindings = bindings<>;

  static_container<parent_bindings> parent;
  static_container<child_bindings, decltype(parent)> child(&parent);

  EXPECT_EQ(child.resolve<parent_temporary>().value, 4);
}

TEST(next_static_parents,
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

TEST(next_static_parents,
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

TEST(next_static_parents,
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

TEST(next_static_parents,
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

TEST(next_static_parents,
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

namespace {
struct declared_dependency {
  declared_dependency() : value(5) {}

  int value;
};

struct declared_consumer {
  explicit declared_consumer(declared_dependency &dependency)
      : value(dependency.value) {}

  int value;
};

// An aggregate, which a container constructs when no binding names it.
struct constructed_dependency {
  static constexpr int value = 6;
};

struct declared_value_consumer {
  explicit declared_value_consumer(constructed_dependency /*dependency*/)
      : value(constructed_dependency::value) {}

  int value;
};

// A dependency a static binding declares is decided when the container is
// compiled: one its parent or auto-construction answers compiles, and one no
// source names is rejected then (the generated diagnostic test
// type_not_found_required_by).
TEST(next_static_parents, a_declared_dependency_answered_when_compiled) {
  using parent_bindings =
      bindings<dingo::bind<scope<shared>, storage<declared_dependency>>>;
  static_container<parent_bindings> parent;
  static_container<
      bindings<dingo::bind<scope<shared>, storage<declared_consumer>,
                           dependencies<declared_dependency &>>>,
      static_container<parent_bindings>>
      child(&parent);
  EXPECT_EQ(child.resolve<declared_consumer &>().value, 5);

  static_container<
      bindings<dingo::bind<scope<shared>, storage<declared_value_consumer>,
                           dependencies<constructed_dependency>>>>
      constructing;
  EXPECT_EQ(constructing.resolve<declared_value_consumer &>().value, 6);
}
} // namespace
