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

// Both types are default-constructible, but constructor detection selects the
// highest arity constructor, so the dependency cycle is still reachable.
struct recursion_b;

struct recursion_a {
  recursion_a() {}
  recursion_a(recursion_b &) {}
};

struct recursion_b {
  recursion_b() {}
  recursion_b(recursion_a &) {}
};

using recursion_bindings = bindings<bind<scope<shared>, storage<recursion_a>>,
                                    bind<scope<shared>, storage<recursion_b>>>;

struct leaf {};

struct middle {
  middle(leaf &) {}
};

struct top {
  top(middle &) {}
};

using acyclic_bindings = bindings<bind<scope<shared>, storage<leaf>>,
                                  bind<scope<shared>, storage<middle>>,
                                  bind<scope<shared>, storage<top>>>;

} // namespace

TEST(recursion_test, container_default_constructible_cycle) {
  container<> container;
  container.register_type<scope<shared>, storage<recursion_a>>();
  container.register_type<scope<shared>, storage<recursion_b>>();

  ASSERT_THROW(container.resolve<recursion_a &>(), type_recursion_exception);
  ASSERT_THROW(container.resolve<recursion_b &>(), type_recursion_exception);
}

TEST(recursion_test, container_unique_default_constructible_cycle) {
  container<> container;
  container.register_type<scope<unique>, storage<recursion_a>>();
  container.register_type<scope<unique>, storage<recursion_b>>();

  ASSERT_THROW(container.resolve<recursion_a>(), type_recursion_exception);
}

TEST(recursion_test, container_bindings_default_constructible_cycle) {
  container<recursion_bindings> container;

  ASSERT_THROW(container.resolve<recursion_a &>(), type_recursion_exception);
  ASSERT_THROW(container.resolve<recursion_b &>(), type_recursion_exception);
}

TEST(recursion_test, static_container_detected_constructor_cycle) {
  static_container<recursion_bindings> container;

  ASSERT_THROW(container.resolve<recursion_a &>(), type_recursion_exception);
  ASSERT_THROW(container.resolve<recursion_b &>(), type_recursion_exception);
}

TEST(recursion_test, static_container_detected_constructor_chain) {
  static_container<acyclic_bindings> container;

  top &first = container.resolve<top &>();
  ASSERT_EQ(&first, &container.resolve<top &>());
}
