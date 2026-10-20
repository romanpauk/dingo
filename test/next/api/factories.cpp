//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

// The factories of the next implementation: declared constructors, functions,
// callable objects, the constructor annotation, and construction of objects no
// binding stores.

#include <dingo/container.h>
#include <dingo/factory/callable.h>
#include <dingo/factory/constructor.h>
#include <dingo/factory/function.h>
#include <dingo/registration/constructor.h>
#include <dingo/static_container.h>
#include <dingo/storage/external.h>
#include <dingo/storage/shared.h>
#include <dingo/storage/unique.h>

#include <gtest/gtest.h>

#include <memory>

namespace {

using dingo::scope;
using dingo::storage;

struct overloaded_constructor {
  explicit overloaded_constructor(int) : chosen(1) {}
  explicit overloaded_constructor(double) : chosen(2) {}
  int chosen;
};

struct generic_constructor {
  template <typename First, typename Second>
  generic_constructor(First first, Second second)
      : sum(static_cast<double>(first) + static_cast<double>(second)) {}
  double sum;
};

TEST(next_factories, constructs_by_the_declared_constructor) {
  dingo::container<> container;
  container.register_type<scope<dingo::external>, storage<double>>(1.5);
  container.register_type<scope<dingo::external>, storage<int>>(2);
  container.register_type<scope<dingo::external>, storage<float>>(0.5f);
  container.register_type<
      scope<dingo::unique>, storage<overloaded_constructor>,
      dingo::factory<dingo::constructor<overloaded_constructor(double)>>>();
  container.register_type<
      scope<dingo::unique>, storage<generic_constructor>,
      dingo::factory<dingo::constructor<generic_constructor(int, float)>>>();
  EXPECT_EQ(container.resolve<overloaded_constructor>().chosen, 2);
  EXPECT_EQ(container.resolve<generic_constructor>().sum, 2.5);
}

struct iface {
  virtual ~iface() = default;
  virtual int run() const = 0;
};

struct impl : iface {
  int run() const override { return 11; }
};

TEST(next_factories, constructs_the_declared_leaf_inside_the_stored_layers) {
  dingo::container<> container;
  container.register_type<scope<dingo::shared>, storage<std::unique_ptr<iface>>,
                          dingo::factory<dingo::constructor<impl>>>();
  EXPECT_EQ(container.resolve<iface &>().run(), 11);
}

struct with_factory {
  static with_factory make(int value) { return with_factory(value); }
  int value;

private:
  explicit with_factory(int value) : value(value) {}
};

std::unique_ptr<iface> make_iface() { return std::make_unique<impl>(); }

TEST(next_factories, calls_a_function) {
  dingo::container<> container;
  container.register_type<scope<dingo::external>, storage<int>>(4);
  container
      .register_type<scope<dingo::unique>, storage<with_factory>,
                     dingo::factory<dingo::function<&with_factory::make>>>();
  // The stored leaf is abstract: the declared factory makes the object.
  container.register_type<scope<dingo::shared>, storage<std::unique_ptr<iface>>,
                          dingo::factory<dingo::function<&make_iface>>>();
  EXPECT_EQ(container.resolve<with_factory>().value, 4);
  EXPECT_EQ(container.resolve<iface &>().run(), 11);
}

struct product {
  int value;
};

struct overloaded_callable {
  product operator()(int value) const { return product{value * 3}; }
  product operator()(float value) const {
    return product{static_cast<int>(value)};
  }
};

TEST(next_factories, calls_a_callable_object) {
  dingo::container<> container;
  container.register_type<scope<dingo::external>, storage<int>>(2);
  int calls = 0;
  container.register_type<scope<dingo::unique>, storage<product>>(
      dingo::callable([&calls](int value) {
        ++calls;
        return product{value * 2};
      }));
  EXPECT_EQ(container.resolve<product>().value, 4);
  EXPECT_EQ(container.resolve<product>().value, 4);
  EXPECT_EQ(calls, 2);
  EXPECT_EQ(container
                .construct<product>(
                    dingo::callable<product(int)>(overloaded_callable{}))
                .value,
            6);
}

struct annotated {
  annotated(int, double) : chosen(1) {}
  DINGO_CONSTRUCTOR(annotated(int value, float scale))
      : chosen(2 + static_cast<int>(scale) + value) {}
  int chosen;
};

TEST(next_factories, constructs_by_the_annotated_constructor) {
  dingo::container<> container;
  container.register_type<scope<dingo::external>, storage<int>>(5);
  container.register_type<scope<dingo::external>, storage<float>>(1.5f);
  container.register_type<scope<dingo::unique>, storage<annotated>>();
  EXPECT_EQ(container.resolve<annotated>().chosen, 8);
}

struct unbound {
  explicit unbound(int &value) : value(value) {}
  int &value;
};

TEST(next_factories, constructs_an_object_no_binding_stores) {
  int value = 3;
  dingo::container<> container;
  container.register_type<scope<dingo::external>, storage<int &>>(value);
  EXPECT_EQ(&container.construct<unbound>().value, &value);
  container.register_type<scope<dingo::unique>, storage<long>>();
  EXPECT_EQ(container.resolve<long>(), 0);
}

TEST(next_factories, serves_declared_factories_statically) {
  using bindings = dingo::bindings<
      dingo::bind<scope<dingo::shared>, storage<std::unique_ptr<iface>>,
                  dingo::factory<dingo::function<&make_iface>>>,
      dingo::bind<
          scope<dingo::unique>, storage<overloaded_constructor>,
          dingo::factory<dingo::constructor<overloaded_constructor(double)>>>,
      dingo::bind<scope<dingo::unique>, storage<double>>>;
  dingo::static_container<bindings> container;
  EXPECT_EQ(container.resolve<iface &>().run(), 11);
  EXPECT_EQ(container.resolve<overloaded_constructor>().chosen, 2);
}

struct overloaded_operator {
  int operator()(int value) const { return value * 2; }
  float operator()(float value) const { return value; }
};

TEST(next_factories, invokes_a_callable_with_resolved_arguments) {
  dingo::container<> container;
  container.register_type<scope<dingo::external>, storage<int>>(7);
  EXPECT_EQ(container.invoke([](int value) { return value + 1; }), 8);
  EXPECT_EQ(container.invoke<int(int)>(overloaded_operator{}), 14);
  using bindings =
      dingo::bindings<dingo::bind<scope<dingo::shared>, storage<double>>>;
  dingo::static_container<bindings> fixed;
  EXPECT_EQ(fixed.invoke([](double &value) { return value; }), 0.0);
}

} // namespace
