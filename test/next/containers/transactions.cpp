//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

// A request that starts at a container is a transaction of the container:
// when the outermost one fails, the bindings registered into the container and
// the objects its bindings constructed during it are undone, and the effects
// on other containers are their own. spec: transaction_rules

#include <dingo/container.h>
#include <dingo/factory/callable.h>
#include <dingo/factory/function.h>
#include <dingo/static_container.h>
#include <dingo/storage/shared.h>

#include <gtest/gtest.h>

#include <stdexcept>

namespace {

using dingo::scope;
using dingo::storage;

template <int Id> struct tracked {
  tracked() { ++constructions; }
  ~tracked() { ++destructions; }
  static inline int constructions = 0;
  static inline int destructions = 0;
  static void clear() { constructions = destructions = 0; }
};

struct failure {};
struct trigger {};

TEST(next_transactions, a_failed_request_undoes_its_effects_on_the_container) {
  tracked<1>::clear();
  tracked<2>::clear();
  dingo::container<> parent;
  dingo::container<> child(&parent);
  parent.register_type<scope<dingo::shared>, storage<tracked<1>>>();
  child.register_type<scope<dingo::shared>, storage<failure>>(
      dingo::callable([&child]() -> failure {
        (void)child.resolve<tracked<1> &>();
        child.register_type<scope<dingo::shared>, storage<tracked<2>>>();
        (void)child.resolve<tracked<2> &>();
        throw std::runtime_error("failed");
      }));
  EXPECT_THROW(child.resolve<failure &>(), std::runtime_error);
  // The object of the parent is the parent's, kept; the registration into
  // the child and the object it constructed are undone.
  EXPECT_EQ(tracked<1>::constructions, 1);
  EXPECT_EQ(tracked<1>::destructions, 0);
  EXPECT_EQ(tracked<2>::constructions, 1);
  EXPECT_EQ(tracked<2>::destructions, 1);
  EXPECT_THROW(child.resolve<tracked<2> &>(), dingo::type_not_found_exception);
  EXPECT_EQ(&child.resolve<tracked<1> &>(), &parent.resolve<tracked<1> &>());
}

TEST(next_transactions,
     a_registration_belongs_to_the_transaction_of_its_container) {
  {
    // A registration into the child during a failed request of the child is
    // undone, though the parent's factory made it.
    dingo::container<> parent;
    dingo::container<> child(&parent);
    parent.register_type<scope<dingo::shared>, storage<trigger>>(
        dingo::callable([&child]() -> trigger {
          child.register_type<scope<dingo::shared>, storage<tracked<3>>>();
          throw std::runtime_error("failed");
        }));
    EXPECT_THROW(child.resolve<trigger &>(), std::runtime_error);
    EXPECT_THROW(child.resolve<tracked<3> &>(),
                 dingo::type_not_found_exception);
  }
  {
    // A registration into the child during a failed request of the parent is
    // the child's, kept.
    dingo::container<> parent;
    dingo::container<> child(&parent);
    parent.register_type<scope<dingo::shared>, storage<trigger>>(
        dingo::callable([&child]() -> trigger {
          child.register_type<scope<dingo::shared>, storage<tracked<3>>>();
          throw std::runtime_error("failed");
        }));
    EXPECT_THROW(parent.resolve<trigger &>(), std::runtime_error);
    (void)child.resolve<tracked<3> &>();
  }
}

struct needs_failing {
  needs_failing(tracked<4> &, failure &) {}
};

failure make_failure() { throw std::runtime_error("failed"); }

TEST(next_transactions, a_static_container_destroys_the_objects_of_a_failure) {
  tracked<4>::clear();
  using bindings = dingo::bindings<
      dingo::bind<scope<dingo::shared>, storage<tracked<4>>>,
      dingo::bind<scope<dingo::shared>, storage<failure>,
                  dingo::factory<dingo::function<&make_failure>>>,
      dingo::bind<scope<dingo::shared>, storage<needs_failing>>>;
  dingo::static_container<bindings> container;
  EXPECT_THROW(container.resolve<needs_failing &>(), std::runtime_error);
  EXPECT_EQ(tracked<4>::constructions, 1);
  EXPECT_EQ(tracked<4>::destructions, 1);
  (void)container.resolve<tracked<4> &>();
  EXPECT_EQ(tracked<4>::constructions, 2);
}

} // namespace
