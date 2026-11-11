//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#include <gtest/gtest.h>

#include <dingo/runtime_container.h>
#include <dingo/static_container.h>
#include <dingo/storage/shared.h>
#include <dingo/storage/unique.h>

#include "support/containers.h"

#include <memory>
#include <stdexcept>

// Rollback of a container alone and across a parent and a child of each
// kind. The parent binds a shared Q and a shared P made of Q; the child binds
// a shared C made of P. Each container is an atomic unit: a resolution that
// falls back to the parent is an operation of the parent, but a static
// parent's of a runtime child, which joins the child's transaction.

namespace dingo::scenarios {
namespace {

struct rollback_q {
  rollback_q() { ++made; }
  ~rollback_q() { ++destroyed; }

  static inline int made = 0;
  static inline int destroyed = 0;
};

struct rollback_p {
  explicit rollback_p(rollback_q &) {
    if (fail) {
      fail = false;
      throw std::runtime_error("rollback_p");
    }
    ++made;
  }

  static inline bool fail = false;
  static inline int made = 0;
};

struct rollback_c {
  explicit rollback_c(rollback_p &) {
    if (fail) {
      fail = false;
      throw std::runtime_error("rollback_c");
    }
    ++made;
  }

  static inline bool fail = false;
  static inline int made = 0;
};

void reset_rollback() {
  rollback_q::made = rollback_q::destroyed = 0;
  rollback_p::made = rollback_c::made = 0;
  rollback_p::fail = rollback_c::fail = false;
}

using rollback_parent_bindings =
    bindings<bind<scope<shared>, storage<rollback_q>>,
             bind<scope<shared>, storage<rollback_p>>>;
using rollback_child_bindings =
    bindings<bind<scope<shared>, storage<rollback_c>>>;

struct runtime_parent {
  using type = runtime_container<>;

  static void prepare(type &parent) {
    parent.register_type<scope<shared>, storage<rollback_q>>();
    parent.register_type<scope<shared>, storage<rollback_p>>();
  }
};

struct static_parent {
  using type = static_container<rollback_parent_bindings>;

  static void prepare(type &) {}
};

struct runtime_child {
  template <typename Parent>
  using type =
      runtime_container<runtime_container_traits,
                        runtime_container_traits::allocator_type, Parent>;

  template <typename Child> static void prepare(Child &child) {
    child.template register_type<scope<shared>, storage<rollback_c>>();
  }
};

struct static_child {
  template <typename Parent>
  using type = static_container<rollback_child_bindings, Parent>;

  template <typename Child> static void prepare(Child &) {}
};

// A child construction that fails after the parent resolved P leaves P to the
// parent: the parent does not make it again.
template <typename Parent, typename Child> void expect_parent_keeps_result() {
  reset_rollback();
  typename Parent::type parent;
  Parent::prepare(parent);
  typename Child::template type<typename Parent::type> child(&parent);
  Child::prepare(child);

  rollback_c::fail = true;
  EXPECT_THROW(child.template resolve<rollback_c &>(), std::runtime_error);
  EXPECT_EQ(rollback_p::made, 1);

  child.template resolve<rollback_c &>();
  EXPECT_EQ(rollback_p::made, 1);
  EXPECT_EQ(rollback_c::made, 1);
}

// A parent operation that fails after it made Q rolls Q back before the
// exception reaches the child.
template <typename Parent, typename Child> void expect_parent_rolls_back() {
  reset_rollback();
  typename Parent::type parent;
  Parent::prepare(parent);
  typename Child::template type<typename Parent::type> child(&parent);
  Child::prepare(child);

  rollback_p::fail = true;
  EXPECT_THROW(child.template resolve<rollback_p &>(), std::runtime_error);
  EXPECT_EQ(rollback_q::made, 1);
  EXPECT_EQ(rollback_q::destroyed, 1);

  parent.template resolve<rollback_p &>();
  EXPECT_EQ(rollback_q::made, 2);
  EXPECT_EQ(rollback_p::made, 1);
}

// A resolution of a container alone that fails after it made Q rolls Q back.
template <typename Container> void expect_container_rolls_back() {
  reset_rollback();
  typename Container::type container;
  Container::prepare(container);

  rollback_p::fail = true;
  EXPECT_THROW(container.template resolve<rollback_p &>(), std::runtime_error);
  EXPECT_EQ(rollback_q::made, 1);
  EXPECT_EQ(rollback_q::destroyed, 1);

  container.template resolve<rollback_p &>();
  EXPECT_EQ(rollback_q::made, 2);
  EXPECT_EQ(rollback_p::made, 1);
}

// A static parent of a runtime child resolves in the child's transaction: a
// child construction that fails undoes what the parent made for it too.
template <typename Parent, typename Child>
void expect_parent_joins_child_transaction() {
  reset_rollback();
  typename Parent::type parent;
  Parent::prepare(parent);
  typename Child::template type<typename Parent::type> child(&parent);
  Child::prepare(child);

  rollback_c::fail = true;
  EXPECT_THROW(child.template resolve<rollback_c &>(), std::runtime_error);
  EXPECT_EQ(rollback_p::made, 1);

  child.template resolve<rollback_c &>();
  EXPECT_EQ(rollback_p::made, 2);
  EXPECT_EQ(rollback_c::made, 1);
}

// A shared class whose destructor reads the shared class it refers to: a
// rollback resets it first.
struct ordered_dependency {
  ordered_dependency() {}
  std::unique_ptr<int> value = std::make_unique<int>(9);
};

struct ordered_dependent {
  explicit ordered_dependent(ordered_dependency &referred)
      : dependency(referred) {}
  ~ordered_dependent() { seen = *dependency.value; }

  ordered_dependency &dependency;
  static inline int seen = 0;
};

struct ordered_failure {
  ordered_failure(ordered_dependent &, ordered_dependency &) {
    throw std::runtime_error("ordered_failure");
  }
};

template <typename Kind> class rollback_order_test : public ::testing::Test {};
TYPED_TEST_SUITE(rollback_order_test, container_kinds, );

} // namespace

TYPED_TEST(rollback_order_test, resets_what_refers_to_an_instance_first) {
  ordered_dependent::seen = 0;
  TypeParam::template with<
      bind<scope<shared>, storage<ordered_dependency>>,
      bind<scope<shared>, storage<ordered_dependent>,
           dependencies<ordered_dependency &>>,
      bind<scope<unique>, storage<ordered_failure>,
           dependencies<ordered_dependent &, ordered_dependency &>>>(
      [](auto &container) {
        EXPECT_THROW(container.template resolve<ordered_failure>(),
                     std::runtime_error);
        EXPECT_EQ(ordered_dependent::seen, 9);
      });
}

TEST(parent_rollback, runtime_container) {
  expect_container_rolls_back<runtime_parent>();
}

TEST(parent_rollback, static_container) {
  expect_container_rolls_back<static_parent>();
}

TEST(parent_rollback, runtime_parent_runtime_child) {
  expect_parent_keeps_result<runtime_parent, runtime_child>();
  expect_parent_rolls_back<runtime_parent, runtime_child>();
}

TEST(parent_rollback, runtime_parent_static_child) {
  expect_parent_keeps_result<runtime_parent, static_child>();
  expect_parent_rolls_back<runtime_parent, static_child>();
}

TEST(parent_rollback, static_parent_runtime_child) {
  expect_parent_joins_child_transaction<static_parent, runtime_child>();
  expect_parent_rolls_back<static_parent, runtime_child>();
}

TEST(parent_rollback, static_parent_static_child) {
  expect_parent_keeps_result<static_parent, static_child>();
  expect_parent_rolls_back<static_parent, static_child>();
}

} // namespace dingo::scenarios
