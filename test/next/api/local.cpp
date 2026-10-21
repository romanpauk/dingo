//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

// Local bindings of a registration in the next implementation: static
// bindings declared with bindings<...> and runtime bindings registered through
// the proxy a registration returns, which the object of the registration
// resolves its dependencies from before the container's.

#include <dingo/container.h>
#include <dingo/factory/callable.h>
#include <dingo/lookup/lookup.h>
#include <dingo/static_container.h>
#include <dingo/storage/shared.h>

#include <gtest/gtest.h>

#include <vector>

namespace {

using dingo::scope;
using dingo::storage;

struct dependency {
  int value;
};

struct first_service {
  explicit first_service(dependency &used) : value(used.value) {}
  int value;
};

struct second_service {
  explicit second_service(dependency &used) : value(used.value) {}
  int value;
};

TEST(next_local, isolates_the_local_bindings_of_registrations) {
  dingo::container<> container;
  auto first =
      container.register_type<scope<dingo::shared>, storage<first_service>>();
  auto second =
      container.register_type<scope<dingo::shared>, storage<second_service>>();
  first->register_type<scope<dingo::shared>, storage<dependency>>(
      dingo::callable([] { return dependency{11}; }));
  second->register_type<scope<dingo::shared>, storage<dependency>>(
      dingo::callable([] { return dependency{22}; }));
  EXPECT_EQ(container.resolve<first_service &>().value, 11);
  EXPECT_EQ(container.resolve<second_service &>().value, 22);
  EXPECT_THROW(container.resolve<dependency &>(),
               dingo::type_not_found_exception);
}

struct static_dependency {
  static_dependency() {}
  int value = 7;
};

struct child_dependency {
  int value;
};

struct root_dependency {
  int value;
};

struct composed_service {
  composed_service(static_dependency &local_static, child_dependency &local,
                   root_dependency &root)
      : static_result(local_static.value), child_result(local.value),
        root_result(root.value) {}
  int static_result;
  int child_result;
  int root_result;
};

TEST(next_local, composes_static_runtime_and_container_bindings) {
  dingo::container<> container;
  container.register_type<scope<dingo::shared>, storage<root_dependency>>(
      dingo::callable([] { return root_dependency{11}; }));
  auto child = container.register_type<
      scope<dingo::shared>, storage<composed_service>,
      dingo::dependencies<static_dependency &, child_dependency &,
                          root_dependency &>,
      dingo::bindings<
          dingo::bind<scope<dingo::shared>, storage<static_dependency>>>>();
  child->register_type<scope<dingo::shared>, storage<child_dependency>>(
      dingo::callable([] { return child_dependency{13}; }));
  auto &service = container.resolve<composed_service &>();
  EXPECT_EQ(service.static_result, 7);
  EXPECT_EQ(service.child_result, 13);
  EXPECT_EQ(service.root_result, 11);
}

struct value {
  value() {}
  explicit value(int number) : number(number) {}
  int number = 0;
};

value make_static_value() { return value(22); }

struct collection_service {
  explicit collection_service(std::vector<value *> all) {
    for (value *element : all) {
      values.push_back(element->number);
    }
  }
  std::vector<int> values;
};

// Local bindings follow the lookup declarations of the container, here a value
// held by the container, a local runtime, and a local static binding.
struct many_values : dingo::dynamic_container_traits {
  using lookup_definition_type = dingo::lookups<dingo::collection<value>>;
};

// The local bindings of a request shadow the container's for a collection,
// as for one object; the container answers when they name none.
TEST(next_local, shadows_the_collections_of_the_container) {
  dingo::container<many_values> container;
  container.register_type<scope<dingo::shared>, storage<value>>(
      dingo::callable([] { return value(11); }));
  auto child = container.register_type<
      scope<dingo::shared>, storage<collection_service>,
      dingo::bindings<
          dingo::bind<scope<dingo::shared>, storage<value>,
                      dingo::factory<dingo::function<&make_static_value>>>>>();
  child->register_type<scope<dingo::shared>, storage<value>>(
      dingo::callable([] { return value(33); }));
  EXPECT_EQ(container.resolve<collection_service &>().values,
            (std::vector<int>{33, 22}));

  dingo::container<many_values> without_locals;
  without_locals.register_type<scope<dingo::shared>, storage<value>>(
      dingo::callable([] { return value(11); }));
  without_locals
      .register_type<scope<dingo::shared>, storage<collection_service>>();
  EXPECT_EQ(without_locals.resolve<collection_service &>().values,
            (std::vector<int>{11}));
}

struct nested_dependency {
  nested_dependency() {}
  int value = 5;
};

struct nested_service {
  nested_service(static_dependency &outer, nested_dependency &inner)
      : value(outer.value + inner.value) {}
  int value;
};

struct root_service {
  explicit root_service(nested_service &nested) : value(nested.value) {}
  int value;
};

TEST(next_local, nests_local_registrations) {
  dingo::container<> container;
  auto child = container.register_type<
      scope<dingo::shared>, storage<root_service>,
      dingo::bindings<
          dingo::bind<scope<dingo::shared>, storage<static_dependency>>>>();
  auto nested =
      child->register_type<scope<dingo::shared>, storage<nested_service>>();
  nested->register_type<scope<dingo::shared>, storage<nested_dependency>>();
  // The nested binding sees the local bindings of its enclosing registration.
  EXPECT_EQ(container.resolve<root_service &>().value, 12);
}

dependency make_nine() { return dependency{9}; }

TEST(next_local, declares_local_static_bindings_in_a_static_container) {
  using bindings = dingo::bindings<
      dingo::bind<scope<dingo::shared>, storage<first_service>,
                  dingo::bindings<dingo::bind<
                      scope<dingo::shared>, storage<dependency>,
                      dingo::factory<dingo::function<&make_nine>>>>>>;
  dingo::static_container<bindings> container;
  EXPECT_EQ(container.resolve<first_service &>().value, 9);
}

} // namespace
