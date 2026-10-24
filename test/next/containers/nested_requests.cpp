//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

// Every request of a container is a transaction: when it fails, the objects
// it and the requests it made constructed are destroyed and their slots forget
// them, whether or not a request that made it catches the failure; when it
// returns, the request that made it keeps them until it returns or fails.
// spec: transaction_rules

#include <dingo/container.h>
#include <dingo/factory/callable.h>
#include <dingo/storage/shared.h>

#include <gtest/gtest.h>

#include <stdexcept>

namespace {

using dingo::scope;
using dingo::storage;

template <int Id> struct tracked {
  tracked() { ++constructions; }
  tracked(const tracked &) = delete;
  tracked &operator=(const tracked &) = delete;
  ~tracked() { ++destructions; }
  static inline int constructions = 0;
  static inline int destructions = 0;
  static void clear() { constructions = destructions = 0; }
};

struct failure {};
struct outer {};

TEST(next_nested_requests,
     a_failed_request_forgets_the_objects_it_constructed) {
  tracked<1>::clear();
  dingo::container<> container;
  container.register_type<scope<dingo::shared>, storage<tracked<1>>>();
  container.register_type<scope<dingo::shared>, storage<failure>>(
      dingo::callable([&container]() -> failure {
        (void)container.resolve<tracked<1> &>();
        throw std::runtime_error("failed");
      }));
  EXPECT_THROW(container.resolve<failure &>(), std::runtime_error);
  EXPECT_EQ(tracked<1>::constructions, 1);
  EXPECT_EQ(tracked<1>::destructions, 1);

  // The slot constructs the object again, and keeps it.
  tracked<1> &object = container.resolve<tracked<1> &>();
  EXPECT_EQ(tracked<1>::constructions, 2);
  EXPECT_EQ(&container.resolve<tracked<1> &>(), &object);
  EXPECT_EQ(tracked<1>::destructions, 1);
}

struct needs_tracked {
  explicit needs_tracked(tracked<2> & /*dependency*/) {
    throw std::runtime_error("failed");
  }
};

TEST(next_nested_requests,
     a_failure_caught_inside_a_request_undoes_its_effects) {
  tracked<2>::clear();
  dingo::container<> container;
  container.register_type<scope<dingo::shared>, storage<tracked<2>>>();
  container.register_type<scope<dingo::shared>, storage<needs_tracked>>();
  container.register_type<scope<dingo::shared>, storage<outer>>(
      dingo::callable([&container]() -> outer {
        EXPECT_THROW(container.resolve<needs_tracked &>(), std::runtime_error);
        // The failed request destroyed the object it constructed.
        EXPECT_EQ(tracked<2>::destructions, 1);
        return outer{};
      }));
  (void)container.resolve<outer &>();
  EXPECT_EQ(tracked<2>::constructions, 1);
  EXPECT_EQ(tracked<2>::destructions, 1);
  // The slot constructs the object again, and keeps it.
  (void)container.resolve<tracked<2> &>();
  EXPECT_EQ(tracked<2>::constructions, 2);
  EXPECT_EQ(tracked<2>::destructions, 1);
}

// A request that returns keeps its effects for the request that made it,
// whose failure undoes them.
TEST(next_nested_requests,
     a_failure_undoes_the_effects_of_the_requests_that_returned) {
  tracked<3>::clear();
  dingo::container<> container;
  container.register_type<scope<dingo::shared>, storage<tracked<3>>>();
  container.register_type<scope<dingo::shared>, storage<failure>>(
      dingo::callable([&container]() -> failure {
        (void)container.resolve<tracked<3> &>();
        EXPECT_EQ(tracked<3>::destructions, 0);
        throw std::runtime_error("failed");
      }));
  EXPECT_THROW(container.resolve<failure &>(), std::runtime_error);
  EXPECT_EQ(tracked<3>::constructions, 1);
  EXPECT_EQ(tracked<3>::destructions, 1);
}

} // namespace
