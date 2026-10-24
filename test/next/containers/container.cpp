//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

// The walking skeleton of the next implementation through its public include
// paths: runtime and static containers of shared and external bindings.

#include <dingo/container.h>
#include <dingo/runtime_container.h>
#include <dingo/static_container.h>
#include <dingo/storage/external.h>
#include <dingo/storage/shared.h>

#include <gtest/gtest.h>

#include <array>
#include <stdexcept>

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

TEST(next_container, borrows_through_wrappers) {
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

namespace {

TEST(next_container, a_runtime_container_moves) {
  dingo::runtime_container<> container;
  container.register_type<scope<dingo::shared>, storage<counted>>();
  counted &first = container.resolve<counted &>();
  dingo::runtime_container<> moved = std::move(container);
  EXPECT_EQ(&moved.resolve<counted &>(), &first);
}

struct alternative_a {
  explicit alternative_a(int init) : value(init) {}
  int value;
};

struct alternative_b {
  explicit alternative_b(float init) : value(init) {}
  float value;
};

TEST(next_container, constructs_by_a_named_factory) {
  dingo::container<> container;
  container.register_type<scope<dingo::external>, storage<int>>(7);
  auto chosen = container.construct<std::variant<alternative_a, alternative_b>,
                                    dingo::constructor<alternative_a>>();
  EXPECT_EQ(std::get<alternative_a>(chosen).value, 7);
}

} // namespace

namespace {

struct constructed_state {
  constructed_state() {}
  int value = 0;
};

TEST(next_container, constructs_by_a_binding_of_the_type) {
  dingo::container<> container;
  container.register_type<scope<dingo::shared>, storage<constructed_state>>();
  container.resolve<constructed_state &>().value = 5;
  EXPECT_EQ(container.construct<constructed_state>().value, 5);
  EXPECT_EQ(&container.construct<constructed_state &>(),
            &container.resolve<constructed_state &>());
  dingo::container<> child(&container);
  EXPECT_EQ(child.construct<constructed_state>().value, 5);
  dingo::container<> unbound;
  EXPECT_EQ(unbound.construct<constructed_state>().value, 0);
}

TEST(next_container, constructs_by_a_static_binding_of_the_type) {
  using bindings = dingo::bindings<
      dingo::bind<scope<dingo::shared>, storage<constructed_state>>>;
  dingo::static_container<bindings> container;
  container.resolve<constructed_state &>().value = 6;
  EXPECT_EQ(container.construct<constructed_state>().value, 6);
}

TEST(next_container, rejects_constructing_an_ambiguously_bound_type) {
  struct traits : dingo::dynamic_container_traits {
    using lookup_definition_type =
        dingo::lookups<dingo::collection<constructed_state>>;
  };
  dingo::container<traits> container;
  container.register_type<scope<dingo::shared>, storage<constructed_state>>();
  container.register_type<scope<dingo::unique>, storage<constructed_state>>();
  EXPECT_THROW(container.construct<constructed_state>(),
               dingo::type_ambiguous_exception);
}

} // namespace

namespace {

struct fails_once {
  static inline int attempts = 0;
  fails_once() {
    if (attempts++ == 0) {
      throw std::runtime_error("first construction fails");
    }
  }
};

TEST(next_container, a_failed_construction_keeps_no_address) {
  fails_once::attempts = 0;
  dingo::container<> container;
  container.register_type<scope<dingo::shared>, storage<fails_once>>();
  EXPECT_THROW(container.resolve<fails_once &>(), std::runtime_error);
  fails_once &constructed = container.resolve<fails_once &>();
  EXPECT_EQ(fails_once::attempts, 2);
  EXPECT_EQ(&container.resolve<fails_once &>(), &constructed);
  EXPECT_EQ(container.resolve<fails_once *>(), &constructed);
  EXPECT_EQ(fails_once::attempts, 2);
}

} // namespace

namespace {

TEST(next_container, an_interface_of_the_stored_type_names_the_binding) {
  using stored = std::variant<std::array<int, 2>, double>;
  stored value;
  dingo::container<> container;
  container.register_type<scope<dingo::external>, storage<stored &>,
                          dingo::interfaces<stored>>(value);
  EXPECT_EQ(&container.resolve<stored &>(), &value);
  dingo::container<> plain;
  plain.register_type<scope<dingo::external>, storage<stored &>>(value);
  EXPECT_EQ(&plain.resolve<stored &>(), &value);
}

} // namespace
