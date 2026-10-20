//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

// Parent containers and auto-construction in the next implementation: a child
// shadows its parent, which a request that names none of the child's bindings
// falls back to, and an unregistered aggregate is constructed last.

#include <dingo/container.h>
#include <dingo/lookup/lookup.h>
#include <dingo/static_container.h>
#include <dingo/storage/shared.h>
#include <dingo/type/auto_constructible.h>

#include <gtest/gtest.h>

#include <vector>

namespace {

using dingo::scope;
using dingo::storage;

struct iface {
  virtual ~iface() = default;
  virtual int id() const = 0;
};

template <int Id> struct impl : iface {
  int id() const override { return Id; }
};

std::vector<int> ids(const std::vector<iface *> &collection) {
  std::vector<int> result;
  for (iface *element : collection) {
    result.push_back(element->id());
  }
  return result;
}

using parent_container = dingo::container<>;
using child_container =
    dingo::container<dingo::dynamic_container_traits,
                     dingo::dynamic_container_traits::allocator_type,
                     parent_container>;

TEST(next_parents, a_child_shadows_its_parent) {
  parent_container parent;
  parent.register_type<scope<dingo::shared>, storage<impl<0>>,
                       dingo::interfaces<iface>>();
  child_container child(&parent);
  EXPECT_EQ(child.resolve<iface &>().id(), 0);
  EXPECT_EQ(ids(child.construct_collection<std::vector<iface *>>()),
            (std::vector<int>{0}));
  child.register_type<scope<dingo::shared>, storage<impl<1>>,
                      dingo::interfaces<iface>>();
  EXPECT_EQ(child.resolve<iface &>().id(), 1);
  EXPECT_EQ(ids(child.construct_collection<std::vector<iface *>>()),
            (std::vector<int>{1}));
  EXPECT_EQ(parent.resolve<iface &>().id(), 0);
}

struct many_traits : dingo::dynamic_container_traits {
  using lookup_definition_type = dingo::lookups<dingo::collection<iface>>;
};

TEST(next_parents, an_ambiguous_parent_is_ambiguous) {
  dingo::container<many_traits> parent;
  parent.register_type<scope<dingo::shared>, storage<impl<0>>,
                       dingo::interfaces<iface>>();
  parent.register_type<scope<dingo::shared>, storage<impl<1>>,
                       dingo::interfaces<iface>>();
  dingo::container<many_traits, many_traits::allocator_type,
                   dingo::container<many_traits>>
      child(&parent);
  EXPECT_THROW(child.resolve<iface &>(), dingo::type_ambiguous_exception);
  EXPECT_EQ(ids(child.construct_collection<std::vector<iface *>>()),
            (std::vector<int>{0, 1}));
}

// An aggregate of an aggregate: auto-constructing it auto-constructs its
// member, the argument of its detected constructor.
struct settings {
  int value() const { return 3; }
};

struct service {
  settings config;
  int value() const { return config.value(); }
};

struct engine {
  explicit engine(service settings) : settings(settings) {}
  service settings;
};

TEST(next_parents, constructs_an_unregistered_aggregate_last) {
  parent_container parent;
  child_container child(&parent);
  EXPECT_EQ(child.resolve<service>().value(), 3);
  EXPECT_EQ(child.construct<engine>().settings.value(), 3);
  const service &kept = child.resolve<const service &>();
  EXPECT_EQ(kept.value(), 3);
  EXPECT_THROW(child.resolve<iface &>(), dingo::type_not_found_exception);
}

TEST(next_parents, a_static_child_falls_back_to_its_parent) {
  parent_container parent;
  parent.register_type<scope<dingo::shared>, storage<impl<5>>,
                       dingo::interfaces<iface>>();
  using bindings =
      dingo::bindings<dingo::bind<scope<dingo::shared>, storage<service>>>;
  dingo::static_container<bindings, parent_container> child(&parent);
  EXPECT_EQ(child.resolve<iface &>().id(), 5);
  EXPECT_EQ(child.resolve<service &>().value(), 3);
}

TEST(next_parents, a_static_container_auto_constructs) {
  dingo::static_container<dingo::bindings<>> container;
  EXPECT_EQ(container.resolve<service>().value(), 3);
  EXPECT_EQ(container.construct<engine>().settings.value(), 3);
}

} // namespace
