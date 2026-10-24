//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

// The unique scope of the next implementation: every request constructs a new
// object and consumes it, moving it out of a wrapper, wrapping it, adopting it,
// or sharing it.

#include <dingo/container.h>
#include <dingo/static_container.h>
#include <dingo/storage/shared.h>
#include <dingo/storage/unique.h>

#include <gtest/gtest.h>

#include <memory>
#include <optional>

namespace {

using dingo::scope;
using dingo::storage;

struct counted {
  static inline int constructions = 0;
  static inline int live = 0;
  counted() {
    ++constructions;
    ++live;
  }
  counted(const counted &) { ++live; }
  counted(counted &&) noexcept { ++live; }
  ~counted() { --live; }
  int value = 7;
};

struct iface {
  virtual ~iface() = default;
  virtual int run() const = 0;
};

struct impl : iface {
  int run() const override { return 11; }
};

TEST(next_unique, constructs_a_value_for_every_request) {
  dingo::container<> container;
  container.register_type<scope<dingo::unique>, storage<counted>>();
  counted::constructions = 0;
  counted first = container.resolve<counted>();
  counted second = container.resolve<counted>();
  EXPECT_EQ(counted::constructions, 2);
  EXPECT_EQ(first.value + second.value, 14);
  EXPECT_THROW(container.resolve<counted &>(),
               dingo::type_not_convertible_exception);
  EXPECT_THROW(container.resolve<counted *>(),
               dingo::type_not_convertible_exception);
  std::optional<counted> wrapped = container.resolve<std::optional<counted>>();
  EXPECT_TRUE(wrapped.has_value());
}

TEST(next_unique, retains_the_object_of_an_rvalue_reference) {
  counted::live = 0;
  {
    dingo::container<> container;
    container.register_type<scope<dingo::unique>, storage<counted>>();
    counted &&first = container.resolve<counted &&>();
    counted &&second = container.resolve<counted &&>();
    EXPECT_NE(&first, &second);
    EXPECT_EQ(first.value, 7);
    EXPECT_EQ(counted::live, 2);
  }
  EXPECT_EQ(counted::live, 0);
}

TEST(next_unique, moves_the_element_out_of_a_layer) {
  dingo::container<> container;
  container
      .register_type<scope<dingo::unique>, storage<std::optional<counted>>>();
  counted value = container.resolve<counted>();
  EXPECT_EQ(value.value, 7);
}

TEST(next_unique, adopts_a_pointer) {
  dingo::container<> container;
  container.register_type<scope<dingo::unique>, storage<impl *>,
                          dingo::interfaces<iface>>();
  std::unique_ptr<iface> owned = container.resolve<std::unique_ptr<iface>>();
  std::shared_ptr<iface> shared = container.resolve<std::shared_ptr<iface>>();
  EXPECT_EQ(owned->run() + shared->run(), 22);
  std::unique_ptr<iface> raw(container.resolve<iface *>());
  EXPECT_EQ(raw->run(), 11);
}

TEST(next_unique, shares_and_upcasts_a_sole_owner_handle) {
  dingo::container<> container;
  container.register_type<scope<dingo::unique>, storage<std::unique_ptr<impl>>,
                          dingo::interfaces<iface>>();
  std::shared_ptr<iface> shared = container.resolve<std::shared_ptr<iface>>();
  std::unique_ptr<iface> owned = container.resolve<std::unique_ptr<iface>>();
  EXPECT_EQ(shared->run() + owned->run(), 22);
  EXPECT_THROW(container.resolve<iface &>(),
               dingo::type_not_convertible_exception);
}

TEST(next_unique, serves_the_same_routes_statically) {
  using bindings =
      dingo::bindings<dingo::bind<scope<dingo::unique>, storage<counted>>,
                      dingo::bind<scope<dingo::unique>, storage<impl *>,
                                  dingo::interfaces<iface>>>;
  dingo::static_container<bindings> container;
  counted::constructions = 0;
  counted value = container.resolve<counted>();
  counted &&retained = container.resolve<counted &&>();
  EXPECT_EQ(counted::constructions, 2);
  EXPECT_EQ(value.value + retained.value, 14);
  EXPECT_TRUE(container.resolve<std::optional<counted>>().has_value());
  EXPECT_EQ(container.resolve<std::unique_ptr<iface>>()->run(), 11);
  EXPECT_EQ(container.resolve<std::shared_ptr<iface>>()->run(), 11);
}

struct engine {
  engine() {}
  int power = 5;
};

struct wheel {
  wheel() {}
  int size = 5;
};

struct car {
  car(std::unique_ptr<engine> init_motor, wheel init_spare)
      : motor(std::move(init_motor)), spare(init_spare) {}
  std::unique_ptr<engine> motor;
  wheel spare;
};

TEST(next_unique, injects_a_new_object_into_each_dependent) {
  dingo::container<> container;
  container.register_type<scope<dingo::unique>, storage<engine *>>();
  container.register_type<scope<dingo::unique>, storage<wheel>>();
  container.register_type<scope<dingo::unique>, storage<car>>();
  car first = container.resolve<car>();
  car second = container.resolve<car>();
  ASSERT_TRUE(first.motor && second.motor);
  EXPECT_NE(first.motor.get(), second.motor.get());
  EXPECT_EQ(first.motor->power + first.spare.size, 10);
}

struct second_value;

struct first_value {
  explicit first_value(second_value &&) {}
};

struct second_value {
  explicit second_value(first_value &&) {}
};

TEST(next_unique, reports_a_recursion) {
  dingo::container<> container;
  container.register_type<scope<dingo::unique>, storage<first_value>>();
  container.register_type<scope<dingo::unique>, storage<second_value>>();
  EXPECT_THROW(container.resolve<first_value>(),
               dingo::type_recursion_exception);
}

} // namespace

namespace {

TEST(next_unique, hands_out_a_new_array) {
  dingo::container<> container;
  container.register_type<scope<dingo::unique>, storage<counted[2]>>();
  counted::constructions = 0;
  counted *first = container.resolve<counted *>();
  counted(*whole)[2] = container.resolve<counted(*)[2]>();
  EXPECT_EQ(counted::constructions, 4);
  EXPECT_NE(first, &(*whole)[0]);
  EXPECT_EQ(first[1].value, 7);
  delete[] first;
  delete[] &(*whole)[0];
  EXPECT_THROW(container.resolve<counted &>(),
               dingo::type_not_convertible_exception);
}

} // namespace
