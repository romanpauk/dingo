//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

// The walking skeleton of the next implementation through its public include
// paths: runtime and static containers of shared and external bindings.

#include <dingo/container.h>
#include <dingo/static_container.h>
#include <dingo/storage/external.h>
#include <dingo/storage/shared.h>

#include <gtest/gtest.h>

#include <memory>
#include <optional>
#include <variant>

namespace {

struct counted {
  static inline int constructions = 0;
  static inline int copies = 0;
  counted() { ++constructions; }
  counted(const counted &) { ++copies; }
  int value = 7;
};

struct other {};

struct iface {
  virtual ~iface() = default;
  virtual int run() const = 0;
};

struct impl : iface {
  int run() const override { return 11; }
};

using dingo::scope;
using dingo::storage;

TEST(next_container, borrows_a_shared_leaf) {
  dingo::container<> container;
  container.register_type<scope<dingo::shared>, storage<counted>>();
  counted::constructions = 0;
  counted &first = container.resolve<counted &>();
  EXPECT_EQ(counted::constructions, 1);
  EXPECT_EQ(&container.resolve<const counted &>(), &first);
  EXPECT_EQ(container.resolve<counted *>(), &first);
  EXPECT_EQ(container.resolve<const counted *>(), &first);
  counted::copies = 0;
  counted copy = container.resolve<counted>();
  EXPECT_EQ(counted::copies, 1);
  EXPECT_EQ(copy.value, 7);
  EXPECT_EQ(counted::constructions, 1);
}

TEST(next_container, reports_requests_it_cannot_serve) {
  dingo::container<> container;
  container.register_type<scope<dingo::shared>, storage<counted>>();
  EXPECT_THROW(container.resolve<counted &&>(),
               dingo::type_not_convertible_exception);
  EXPECT_THROW(container.resolve<std::unique_ptr<counted>>(),
               dingo::type_not_convertible_exception);
  EXPECT_THROW(container.resolve<other &>(), dingo::type_not_found_exception);
  // A second binding of the leaf without a declared lookup of many is
  // rejected.
  EXPECT_THROW(
      (container.register_type<scope<dingo::shared>, storage<counted *>>()),
      dingo::lookup_already_registered_exception);
  EXPECT_NO_THROW(container.resolve<counted &>());
}

TEST(next_container, borrows_through_layers) {
  dingo::container<> container;
  container.register_type<scope<dingo::shared>,
                          storage<std::shared_ptr<std::optional<counted>>>>();
  counted &leaf = container.resolve<counted &>();
  EXPECT_EQ(container.resolve<counted *>(), &leaf);
  std::optional<counted> &optional =
      container.resolve<std::optional<counted> &>();
  EXPECT_EQ(&*optional, &leaf);
  auto handle = container.resolve<std::shared_ptr<std::optional<counted>>>();
  EXPECT_EQ(&**handle, &leaf);
  EXPECT_EQ(&container.resolve<std::shared_ptr<std::optional<counted>> &>(),
            &container.resolve<std::shared_ptr<std::optional<counted>> &>());
}

TEST(next_container, borrows_owned_alternatives_and_arrays) {
  dingo::container<> container;
  container
      .register_type<scope<dingo::shared>, storage<std::unique_ptr<counted>>>();
  container
      .register_type<scope<dingo::shared>, storage<std::variant<int, other>>>();
  container.register_type<scope<dingo::shared>, storage<double[3]>>();
  counted &leaf = container.resolve<counted &>();
  EXPECT_EQ(container.resolve<std::unique_ptr<counted> &>().get(), &leaf);
  EXPECT_THROW(container.resolve<std::unique_ptr<counted>>(),
               dingo::type_not_convertible_exception);
  int &alternative = container.resolve<int &>();
  EXPECT_EQ(&std::get<int>(container.resolve<std::variant<int, other> &>()),
            &alternative);
  double (&array)[3] = container.resolve<double (&)[3]>();
  EXPECT_EQ(container.resolve<double *>(), &array[0]);
}

TEST(next_container, refers_to_external_instances) {
  counted instance;
  dingo::container<> container;
  container.register_type<scope<dingo::external>, storage<counted &>>(instance);
  EXPECT_EQ(&container.resolve<counted &>(), &instance);
  EXPECT_EQ(container.resolve<counted *>(), &instance);
}

TEST(next_container, reaches_declared_interfaces) {
  dingo::container<> container;
  container.register_type<scope<dingo::shared>, storage<impl>,
                          dingo::interfaces<iface>>();
  iface &interface = container.resolve<iface &>();
  EXPECT_EQ(interface.run(), 11);
  EXPECT_EQ(container.resolve<iface *>(), &interface);
  EXPECT_THROW(container.resolve<impl &>(), dingo::type_not_found_exception);
}

using static_bindings = dingo::bindings<
    dingo::bind<scope<dingo::shared>, storage<std::shared_ptr<counted>>>,
    dingo::bind<scope<dingo::shared>, storage<impl>, dingo::interfaces<iface>>>;

TEST(next_static_container, serves_the_same_routes) {
  dingo::static_container<static_bindings> container;
  counted &leaf = container.resolve<counted &>();
  EXPECT_EQ(container.resolve<counted *>(), &leaf);
  EXPECT_EQ(container.resolve<std::shared_ptr<counted>>().get(), &leaf);
  EXPECT_EQ(container.resolve<std::shared_ptr<counted> &>().get(), &leaf);
  counted::copies = 0;
  counted copy = container.resolve<counted>();
  EXPECT_EQ(counted::copies, 1);
  EXPECT_EQ(copy.value, 7);
  EXPECT_EQ(container.resolve<iface &>().run(), 11);
  EXPECT_EQ(container.resolve<iface *>(), &container.resolve<iface &>());
}

} // namespace

namespace {

template <typename Container> void check_kept_handle(Container &container) {
  std::shared_ptr<iface> &kept =
      container.template resolve<std::shared_ptr<iface> &>();
  EXPECT_EQ(kept->run(), 11);
  EXPECT_EQ(&container.template resolve<std::shared_ptr<iface> &>(), &kept);
  EXPECT_EQ(container.template resolve<std::shared_ptr<iface> *>(), &kept);
  EXPECT_EQ(kept.get(), container.template resolve<iface *>());
  EXPECT_EQ(kept.use_count(), 2);
}

TEST(next_container, keeps_the_converted_handle_of_an_interface) {
  dingo::container<> container;
  container.register_type<scope<dingo::shared>, storage<std::shared_ptr<impl>>,
                          dingo::interfaces<iface>>();
  check_kept_handle(container);
}

TEST(next_container, keeps_the_converted_handle_of_an_interface_statically) {
  using bindings = dingo::bindings<
      dingo::bind<scope<dingo::shared>, storage<std::shared_ptr<impl>>,
                  dingo::interfaces<iface>>>;
  dingo::static_container<bindings> container;
  check_kept_handle(container);
}

} // namespace

namespace {

TEST(next_container, takes_static_external_instances_in_binding_order) {
  counted first;
  std::shared_ptr<impl> second = std::make_shared<impl>();
  using bindings = dingo::bindings<
      dingo::bind<scope<dingo::external>, storage<counted &>>,
      dingo::bind<scope<dingo::shared>, storage<other>>,
      dingo::bind<scope<dingo::external>, storage<std::shared_ptr<impl>>,
                  dingo::interfaces<iface>>>;
  dingo::static_container<bindings> container(first, second);
  EXPECT_EQ(&container.resolve<counted &>(), &first);
  EXPECT_EQ(container.resolve<iface *>(), second.get());
  EXPECT_EQ(container.resolve<std::shared_ptr<iface>>(), second);
}

} // namespace
