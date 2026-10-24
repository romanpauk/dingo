//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

// A cycle through the detected constructors of default-constructible types is
// a recursion: detection selects the constructor of the largest arity, not the
// default one that would break the cycle.

#include <dingo/container.h>
#include <dingo/static_container.h>
#include <dingo/storage/shared.h>
#include <dingo/storage/unique.h>

#include <gtest/gtest.h>

namespace {

using dingo::scope;
using dingo::storage;

struct recursion_b;

struct recursion_a {
  recursion_a() {}
  recursion_a(recursion_b &) {}
};

struct recursion_b {
  recursion_b() {}
  recursion_b(recursion_a &) {}
};

using recursion_bindings =
    dingo::bindings<dingo::bind<scope<dingo::shared>, storage<recursion_a>>,
                    dingo::bind<scope<dingo::shared>, storage<recursion_b>>>;

// A consumed object is never borrowed, so the cycle of unique bindings takes
// its arguments by value.
struct unique_b;

struct unique_a {
  unique_a() {}
  unique_a(unique_b &&) {}
};

struct unique_b {
  unique_b() {}
  unique_b(unique_a &&) {}
};

struct leaf {};

struct middle {
  middle(leaf &) {}
};

struct top {
  top(middle &) {}
};

using acyclic_bindings =
    dingo::bindings<dingo::bind<scope<dingo::shared>, storage<leaf>>,
                    dingo::bind<scope<dingo::shared>, storage<middle>>,
                    dingo::bind<scope<dingo::shared>, storage<top>>>;

TEST(next_recursion, a_cycle_of_default_constructible_types_is_a_recursion) {
  dingo::container<> container;
  container.register_type<scope<dingo::shared>, storage<recursion_a>>();
  container.register_type<scope<dingo::shared>, storage<recursion_b>>();

  ASSERT_THROW(container.resolve<recursion_a &>(),
               dingo::type_recursion_exception);
  ASSERT_THROW(container.resolve<recursion_b &>(),
               dingo::type_recursion_exception);
}

TEST(next_recursion, a_unique_cycle_of_default_constructible_types) {
  dingo::container<> container;
  container.register_type<scope<dingo::unique>, storage<unique_a>>();
  container.register_type<scope<dingo::unique>, storage<unique_b>>();

  ASSERT_THROW(container.resolve<unique_a>(), dingo::type_recursion_exception);
}

TEST(next_recursion, a_bound_cycle_of_default_constructible_types) {
  dingo::container<recursion_bindings> container;

  ASSERT_THROW(container.resolve<recursion_a &>(),
               dingo::type_recursion_exception);
  ASSERT_THROW(container.resolve<recursion_b &>(),
               dingo::type_recursion_exception);
}

TEST(next_recursion, a_static_cycle_of_default_constructible_types) {
  dingo::static_container<recursion_bindings> container;

  ASSERT_THROW(container.resolve<recursion_a &>(),
               dingo::type_recursion_exception);
  ASSERT_THROW(container.resolve<recursion_b &>(),
               dingo::type_recursion_exception);
}

TEST(next_recursion, a_static_chain_of_detected_constructors_is_no_recursion) {
  dingo::static_container<acyclic_bindings> container;

  top &first = container.resolve<top &>();
  ASSERT_EQ(&first, &container.resolve<top &>());
}

} // namespace
