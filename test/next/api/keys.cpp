//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

// Keys in the next implementation: tag keys, fixed keys, and runtime key
// values, in runtime and static containers alike.

#include <dingo/container.h>
#include <dingo/lookup/lookup.h>
#include <dingo/static_container.h>
#include <dingo/storage/shared.h>

#include <gtest/gtest.h>

#include <cstddef>
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

struct tag_a {};
struct tag_b {};

template <typename Key> struct consumer {
  explicit consumer(dingo::dependency<iface &, Key> dependency)
      : selected(dependency) {}
  iface &selected;
};

using fixed_seven = dingo::key_type<int, 7>;
using fixed_eight = dingo::key_type<int, 8>;

TEST(next_keys, selects_by_tag_and_by_value) {
  dingo::container<> container;
  container.register_type<scope<dingo::shared>, storage<impl<0>>,
                          dingo::interfaces<iface>>();
  container.register_type<scope<dingo::shared>, storage<impl<1>>,
                          dingo::interfaces<iface>, dingo::key_type<tag_a>>();
  container.register_type<scope<dingo::shared>, storage<impl<2>>,
                          dingo::interfaces<iface>>(dingo::key_value{7});
  // A fixed key in a runtime registration is stored as its value.
  container.register_type<scope<dingo::shared>, storage<impl<3>>,
                          dingo::interfaces<iface>, fixed_eight>();
  EXPECT_EQ(container.resolve<iface &>().id(), 0);
  EXPECT_EQ(
      container.construct<consumer<dingo::key_type<tag_a>>>().selected.id(), 1);
  EXPECT_EQ(container.resolve<iface &>(7).id(), 2);
  EXPECT_EQ(container.construct<consumer<fixed_seven>>().selected.id(), 2);
  EXPECT_EQ(container.resolve<iface &>(dingo::key_value{8}).id(), 3);
  EXPECT_THROW(container.resolve<iface &>(9), dingo::type_not_found_exception);
  EXPECT_THROW(container.construct<consumer<dingo::key_type<tag_b>>>(),
               dingo::type_not_found_exception);
}

TEST(next_keys, rejects_a_second_binding_of_a_key) {
  dingo::container<> container;
  container.register_type<scope<dingo::shared>, storage<impl<0>>,
                          dingo::interfaces<iface>>(dingo::key_value{7});
  EXPECT_THROW(
      (container.register_type<scope<dingo::shared>, storage<impl<1>>,
                               dingo::interfaces<iface>, fixed_seven>()),
      dingo::lookup_already_registered_exception);
  EXPECT_NO_THROW(
      (container.register_type<scope<dingo::shared>, storage<impl<1>>,
                               dingo::interfaces<iface>>(dingo::key_value{8})));
  EXPECT_EQ(container.resolve<iface &>(8).id(), 1);
}

TEST(next_keys, selects_static_bindings_by_key) {
  using bindings = dingo::bindings<
      dingo::bind<scope<dingo::shared>, storage<impl<0>>,
                  dingo::interfaces<iface>>,
      dingo::bind<scope<dingo::shared>, storage<impl<1>>,
                  dingo::interfaces<iface>, dingo::key_type<tag_a>>,
      dingo::bind<scope<dingo::shared>, storage<impl<2>>,
                  dingo::interfaces<iface>, fixed_seven>,
      dingo::bind<scope<dingo::shared>, storage<impl<3>>,
                  dingo::interfaces<iface>, fixed_eight>>;
  dingo::static_container<bindings> container;
  EXPECT_EQ(container.resolve<iface &>().id(), 0);
  EXPECT_EQ(
      container.construct<consumer<dingo::key_type<tag_a>>>().selected.id(), 1);
  EXPECT_EQ(container.construct<consumer<fixed_eight>>().selected.id(), 3);
  // A runtime key value is compared with the fixed keys when it is resolved.
  EXPECT_EQ(container.resolve<iface &>(7).id(), 2);
  EXPECT_EQ(container.resolve<iface &>(dingo::key_value{8}).id(), 3);
  EXPECT_THROW(container.resolve<iface &>(9), dingo::type_not_found_exception);
}

} // namespace

namespace {

struct dense_traits : dingo::dynamic_container_traits {
  using lookup_definition_type = dingo::lookups<
      dingo::associative<std::size_t, iface, dingo::one, dingo::array<2>>>;
};

TEST(next_keys, a_dense_array_never_stores_a_key_outside_its_bound) {
  dingo::container<dense_traits> container;
  container.register_type<scope<dingo::shared>, storage<impl<1>>,
                          dingo::interfaces<iface>>(
      dingo::key_value{std::size_t(1)});
  EXPECT_THROW((container.register_type<scope<dingo::shared>, storage<impl<2>>,
                                        dingo::interfaces<iface>>(
                   dingo::key_value{std::size_t(2)})),
               dingo::type_index_out_of_range_exception);
  EXPECT_THROW((container.register_type<scope<dingo::shared>, storage<impl<2>>,
                                        dingo::interfaces<iface>,
                                        dingo::key_type<std::size_t, 5>>()),
               dingo::type_index_out_of_range_exception);
  EXPECT_EQ(container.resolve<iface &>(std::size_t(1)).id(), 1);
  EXPECT_THROW(container.resolve<iface &>(std::size_t(2)),
               dingo::type_not_found_exception);
}

enum class channel : int { first, second, third };

struct channel_traits : dingo::dynamic_container_traits {
  using lookup_definition_type = dingo::lookups<
      dingo::associative<channel, iface, dingo::many, dingo::array<2>>>;
};

TEST(next_keys, a_dense_array_indexes_enumeration_keys) {
  dingo::container<channel_traits> container;
  container.register_type<scope<dingo::shared>, storage<impl<1>>,
                          dingo::interfaces<iface>>(
      dingo::key_value{channel::second});
  EXPECT_THROW((container.register_type<scope<dingo::shared>, storage<impl<2>>,
                                        dingo::interfaces<iface>>(
                   dingo::key_value{channel::third})),
               dingo::type_index_out_of_range_exception);
  EXPECT_EQ(container.resolve<std::vector<iface *>>(channel::second).size(),
            1u);
}

} // namespace

namespace {

struct many_traits : dingo::dynamic_container_traits {
  using lookup_definition_type =
      dingo::lookups<dingo::associative<int, iface, dingo::many>>;
};

TEST(next_keys, a_registration_after_a_request_is_found) {
  dingo::container<many_traits> container;
  EXPECT_THROW(container.resolve<iface &>(1), dingo::type_not_found_exception);
  container.register_type<scope<dingo::shared>, storage<impl<1>>,
                          dingo::interfaces<iface>>(dingo::key_value{1});
  EXPECT_EQ(container.resolve<iface &>(1).id(), 1);
  EXPECT_EQ(container.resolve<std::vector<iface *>>(1).size(), 1u);
  container.register_type<scope<dingo::shared>, storage<impl<2>>,
                          dingo::interfaces<iface>>(dingo::key_value{1});
  EXPECT_THROW(container.resolve<iface &>(1), dingo::type_ambiguous_exception);
  std::vector<iface *> both = container.resolve<std::vector<iface *>>(1);
  ASSERT_EQ(both.size(), 2u);
  EXPECT_EQ(both[0]->id(), 1);
  EXPECT_EQ(both[1]->id(), 2);
}

} // namespace
