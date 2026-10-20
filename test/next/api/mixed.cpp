//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

// Mixed containers of the next implementation: static bindings and runtime
// registrations share one container, its lookup declarations, and its
// selection.

#include <dingo/container.h>
#include <dingo/lookup/lookup.h>
#include <dingo/storage/external.h>
#include <dingo/storage/shared.h>

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

struct other {
  other() {}
};

std::vector<int> ids(const std::vector<iface *> &collection) {
  std::vector<int> result;
  for (iface *element : collection) {
    result.push_back(element->id());
  }
  return result;
}

using static_part = dingo::bindings<
    dingo::bind<scope<dingo::shared>, storage<impl<0>>,
                dingo::interfaces<iface>>,
    dingo::bind<scope<dingo::shared>, storage<impl<1>>,
                dingo::interfaces<iface>, dingo::key_type<int, 7>>>;

TEST(next_mixed, selects_among_static_and_runtime_bindings) {
  dingo::container<static_part> container;
  container.register_type<scope<dingo::shared>, storage<other>>();
  container.register_type<scope<dingo::shared>, storage<impl<2>>,
                          dingo::interfaces<iface>>(dingo::key_value{8});
  EXPECT_EQ(container.resolve<iface &>().id(), 0);
  EXPECT_EQ(container.resolve<iface &>(7).id(), 1);
  EXPECT_EQ(container.resolve<iface &>(8).id(), 2);
  EXPECT_NO_THROW(container.resolve<other &>());
}

TEST(next_mixed, rejects_a_runtime_binding_of_a_static_key) {
  dingo::container<static_part> container;
  EXPECT_THROW((container.register_type<scope<dingo::shared>, storage<impl<2>>,
                                        dingo::interfaces<iface>>()),
               dingo::lookup_already_registered_exception);
  EXPECT_THROW(
      (container.register_type<scope<dingo::shared>, storage<impl<2>>,
                               dingo::interfaces<iface>>(dingo::key_value{7})),
      dingo::lookup_already_registered_exception);
}

struct many_traits : dingo::dynamic_container_traits {
  using lookup_definition_type = dingo::lookups<dingo::collection<iface>>;
};

TEST(next_mixed, declares_lookups_in_its_traits) {
  dingo::container<static_part, many_traits> container;
  container.register_type<scope<dingo::shared>, storage<impl<3>>,
                          dingo::interfaces<iface>>();
  EXPECT_THROW(container.resolve<iface &>(), dingo::type_ambiguous_exception);
  // Runtime bindings come before static ones.
  EXPECT_EQ(ids(container.construct_collection<std::vector<iface *>>()),
            (std::vector<int>{3, 0}));
}

TEST(next_mixed, falls_back_to_its_parent) {
  dingo::container<> parent;
  parent.register_type<scope<dingo::shared>, storage<other>>();
  using bindings =
      dingo::bindings<dingo::bind<scope<dingo::shared>, storage<impl<4>>,
                                  dingo::interfaces<iface>>>;
  dingo::container<bindings, dingo::container<>> child(&parent);
  EXPECT_EQ(child.resolve<iface &>().id(), 4);
  EXPECT_EQ(&child.resolve<other &>(), &parent.resolve<other &>());
}

TEST(next_mixed, takes_the_instances_of_external_static_bindings) {
  other instance;
  using bindings =
      dingo::bindings<dingo::bind<scope<dingo::external>, storage<other &>>>;
  dingo::container<bindings> container(instance);
  EXPECT_EQ(&container.resolve<other &>(), &instance);
}

} // namespace

namespace {

struct many_mixed_traits : dingo::dynamic_container_traits {
  using lookup_definition_type =
      dingo::lookups<dingo::associative<int, iface, dingo::many>>;
};

TEST(next_mixed, two_static_bindings_of_a_key_are_ambiguous) {
  using bindings = dingo::bindings<
      dingo::bind<scope<dingo::shared>, storage<impl<1>>,
                  dingo::interfaces<iface>, dingo::key_type<int, 7>>,
      dingo::bind<scope<dingo::shared>, storage<impl<2>>,
                  dingo::interfaces<iface>, dingo::key_type<int, 7>>>;
  dingo::container<bindings, many_mixed_traits> container;
  EXPECT_THROW(container.resolve<iface &>(dingo::key_type<int, 7>{}),
               dingo::type_ambiguous_exception);
  EXPECT_THROW(container.resolve<iface &>(7), dingo::type_ambiguous_exception);
}

} // namespace
