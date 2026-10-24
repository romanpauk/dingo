//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

// The shared_cyclical scope of the next implementation: an object is published
// before it is constructed, so objects can depend on each other in a cycle.

#include <dingo/container.h>
#include <dingo/static_container.h>
#include <dingo/storage/shared.h>
#include <dingo/storage/shared_cyclical.h>

#include <gtest/gtest.h>

#include <memory>
#include <stdexcept>

namespace {

using dingo::scope;
using dingo::storage;

struct second;

struct first {
  explicit first(second &init_next) : next(init_next) {}
  second &next;
};

struct second {
  second(first &init_previous, std::shared_ptr<first> init_handle)
      : previous(init_previous), handle(std::move(init_handle)) {}
  first &previous;
  std::shared_ptr<first> handle;
};

template <typename Container> void check_cycle(Container &container) {
  second &object = container.template resolve<second &>();
  first &other = container.template resolve<first &>();
  EXPECT_EQ(&object.previous, &other);
  EXPECT_EQ(&other.next, &object);
  EXPECT_EQ(object.handle.get(), &other);
  EXPECT_EQ(&container.template resolve<second &>(), &object);
}

TEST(next_shared_cyclical, resolves_a_cycle) {
  dingo::container<> container;
  container.register_type<scope<dingo::shared_cyclical>,
                          storage<std::shared_ptr<first>>>();
  container.register_type<scope<dingo::shared_cyclical>, storage<second>>();
  check_cycle(container);
}

TEST(next_shared_cyclical, resolves_a_cycle_statically) {
  using bindings = dingo::bindings<
      dingo::bind<scope<dingo::shared_cyclical>,
                  storage<std::shared_ptr<first>>>,
      dingo::bind<scope<dingo::shared_cyclical>, storage<second>>>;
  dingo::static_container<bindings> container;
  check_cycle(container);
}

// A cycle of declared dependencies is legal when every binding in it is
// shared_cyclical; static-container-rejects-a-declared-cycle.cpp checks
// one that is not.
TEST(next_shared_cyclical, resolves_a_declared_cycle_statically) {
  using bindings = dingo::bindings<
      dingo::bind<scope<dingo::shared_cyclical>,
                  storage<std::shared_ptr<first>>,
                  dingo::dependencies<second &>>,
      dingo::bind<scope<dingo::shared_cyclical>, storage<second>,
                  dingo::dependencies<first &, std::shared_ptr<first>>>>;
  dingo::static_container<bindings> container;
  check_cycle(container);
}

TEST(next_shared_cyclical, a_shared_cycle_is_a_recursion) {
  dingo::container<> container;
  container.register_type<scope<dingo::shared>, storage<first>>();
  container.register_type<scope<dingo::shared>, storage<second>>();
  EXPECT_THROW(container.resolve<first &>(), dingo::type_recursion_exception);
}

struct counted {
  static inline int live = 0;
  counted() { ++live; }
  counted(const counted &) = delete;
  ~counted() { --live; }
};

struct failing;

struct holder {
  holder(counted &init_object, failing &init_other)
      : object(init_object), other(init_other) {}
  counted &object;
  failing &other;
};

struct failing {
  static inline bool fail = true;
  explicit failing(holder &) {
    if (fail) {
      throw std::runtime_error("failed");
    }
  }
};

TEST(next_shared_cyclical, resets_the_publication_of_a_failed_cycle) {
  counted::live = 0;
  dingo::container<> container;
  container.register_type<scope<dingo::shared_cyclical>, storage<counted>>();
  container.register_type<scope<dingo::shared_cyclical>,
                          storage<std::shared_ptr<holder>>>();
  container.register_type<scope<dingo::shared_cyclical>, storage<failing>>();
  failing::fail = true;
  EXPECT_THROW(container.resolve<holder &>(), std::runtime_error);
  // The holder, which referred to the failed object, and the counted object
  // it published are reset with it.
  EXPECT_EQ(counted::live, 0);
  // The address of the counted object borrowed during the failed publication
  // is not kept, so a request constructs it again.
  counted &alone = container.resolve<counted &>();
  EXPECT_EQ(counted::live, 1);
  failing::fail = false;
  holder &object = container.resolve<holder &>();
  EXPECT_EQ(&object.object, &alone);
  EXPECT_EQ(&object.other, &container.resolve<failing &>());
  EXPECT_EQ(counted::live, 1);
}

} // namespace
