//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#include <dingo/container.h>
#include <dingo/factory/callable.h>
#include <dingo/factory/constructor.h>
#include <dingo/factory/function.h>
#include <dingo/registration/constructor.h>
#include <dingo/static_container.h>
#include <dingo/storage/shared.h>
#include <dingo/storage/unique.h>

#include "support/containers.h"

#include <gtest/gtest.h>

#include <cstddef>
#include <memory>
#include <vector>

// The kinds of factory a registration can name, the dependencies it can
// declare, and the bindings local to one static registration. Detected
// constructors and constructor<T(Args...)> factories are resolved by
// test/container/dependency_chain.cpp and
// test/registration/registration_traits.cpp.

using namespace dingo;

namespace {
struct marked {
  int marker() const { return 3; }
};

using value_binding = dingo::bind<scope<shared>, storage<marked>>;

struct made_by_function {
  static made_by_function create(marked &dependency) {
    return made_by_function(dependency.marker() + 36);
  }

  int value;

private:
  explicit made_by_function(int init) : value(init) {}
};

struct typedef_constructed {
  DINGO_CONSTRUCTOR(typedef_constructed(marked &dependency))
      : value(dependency.marker() + 30) {}
  // Detection alone would pick the constructor of the higher arity.
  typedef_constructed(marked &, marked &) : value(-1) {}

  int value;
};

struct first {};
struct second {};

struct overloaded_constructors {
  overloaded_constructors(first &, second &) : value(-1) {}
  overloaded_constructors(second &, first &) : value(17) {}

  int value;
};

struct made_by_callable {
  explicit made_by_callable(int init) : value(init) {}

  int value;
};

struct overloaded_factory {
  made_by_callable operator()(marked &dependency) const {
    return made_by_callable(dependency.marker() + 18);
  }
  made_by_callable operator()(int other) const {
    return made_by_callable(other + 100);
  }
};
} // namespace

template <typename T> struct factory_kinds_test : testing::Test {};

TYPED_TEST_SUITE(factory_kinds_test, container_kinds, );

TYPED_TEST(factory_kinds_test, function_factory_resolves_its_parameters) {
  TypeParam::template with<
      value_binding, dingo::bind<scope<shared>, storage<made_by_function>,
                                 factory<function<&made_by_function::create>>>>(
      [](auto &container) {
        EXPECT_EQ(container.template resolve<made_by_function &>().value, 39);
      });
}

TYPED_TEST(factory_kinds_test, constructor_typedef_selects_the_constructor) {
  TypeParam::template with<
      value_binding, dingo::bind<scope<shared>, storage<typedef_constructed>>>(
      [](auto &container) {
        EXPECT_EQ(container.template resolve<typedef_constructed &>().value,
                  33);
      });
}

TYPED_TEST(factory_kinds_test, declared_dependencies_select_the_constructor) {
  TypeParam::template with<
      dingo::bind<scope<shared>, storage<first>>,
      dingo::bind<scope<shared>, storage<second>>,
      dingo::bind<scope<unique>, storage<overloaded_constructors>,
                  dependencies<second &, first &>>>([](auto &container) {
    EXPECT_EQ(container.template resolve<overloaded_constructors>().value, 17);
  });
}

// A static registration cannot hold a callable, so this one is runtime only.
TEST(runtime_factory_kinds_test, callable_factory_resolves_its_parameters) {
  runtime_container<> container;
  container.register_type<scope<shared>, storage<marked>>();
  container.register_type<scope<shared>, storage<made_by_callable>>(
      callable([](marked &dependency) {
        return made_by_callable(dependency.marker() + 12);
      }));
  EXPECT_EQ(container.resolve<made_by_callable &>().value, 15);
}

// A signature given to callable picks one overload of the call operator.
TYPED_TEST(factory_kinds_test, construct_with_callable_of_given_signature) {
  TypeParam::template with<value_binding>([](auto &container) {
    auto made = container.template construct<made_by_callable>(
        callable<made_by_callable(marked &)>(overloaded_factory{}));
    EXPECT_EQ(made.value, 21);
  });
}

namespace {
struct local_dependency {
  local_dependency() : value(4) {}

  int value;
};

local_dependency make_host_dependency() {
  local_dependency made;
  made.value = 40;
  return made;
}

struct uses_local_and_host {
  uses_local_and_host(local_dependency &local, marked &host)
      : value(local.value + host.marker() + 5) {}

  int value;
};

struct uses_shadowed {
  explicit uses_shadowed(local_dependency &dependency)
      : value(dependency.value) {}

  int value;
};

struct element_interface {
  virtual ~element_interface() = default;
  virtual int id() const = 0;
};

template <int Id> struct element : element_interface {
  int id() const override { return Id; }
};

using elements = std::vector<std::shared_ptr<element_interface>>;

struct uses_collection {
  explicit uses_collection(elements all) : count(all.size()), sum(0) {
    for (const auto &item : all)
      sum += item->id();
  }

  std::size_t count;
  int sum;
};

using local_dependency_binding =
    bindings<dingo::bind<scope<shared>, storage<local_dependency>>>;
} // namespace

// A dependency only the local bindings provide is found next to one only the
// host provides, by resolve and construct alike.
TEST(local_bindings_test, local_and_host_dependencies_are_combined) {
  static_container<bindings<
      value_binding, dingo::bind<scope<shared>, storage<uses_local_and_host>,
                                 local_dependency_binding>>>
      container;
  EXPECT_EQ(container.resolve<uses_local_and_host &>().value, 12);
  EXPECT_EQ(container.construct<uses_local_and_host>().value, 12);
}

// A singular dependency both provide is taken from the local bindings.
TEST(local_bindings_test, local_binding_shadows_host_binding) {
  static_container<
      bindings<dingo::bind<scope<shared>, storage<local_dependency>,
                           factory<function<&make_host_dependency>>>,
               dingo::bind<scope<shared>, storage<uses_shadowed>,
                           local_dependency_binding>>>
      container;
  EXPECT_EQ(container.resolve<uses_shadowed &>().value, 4);
  EXPECT_EQ(container.construct<uses_shadowed>().value, 4);
  EXPECT_EQ(container.resolve<local_dependency &>().value, 40);
}

// A collection holds the local entries alone when there are any.
TEST(local_bindings_test, local_collection_entries_shadow_host_entries) {
  static_container<bindings<
      dingo::bind<scope<shared>, storage<std::shared_ptr<element<1>>>,
                  interfaces<element_interface>>,
      dingo::bind<scope<shared>, storage<uses_collection>,
                  bindings<dingo::bind<scope<shared>,
                                       storage<std::shared_ptr<element<0>>>,
                                       interfaces<element_interface>>>,
                  dependencies<elements>>>>
      container;
  auto &resolved = container.resolve<uses_collection &>();
  EXPECT_EQ(resolved.count, 1u);
  EXPECT_EQ(resolved.sum, 0);
  auto constructed = container.construct<uses_collection>();
  EXPECT_EQ(constructed.count, 1u);
  EXPECT_EQ(constructed.sum, 0);
}
