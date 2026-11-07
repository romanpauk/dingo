//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#include <dingo/container.h>
#include <dingo/storage/shared.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <map>
#include <memory>
#include <vector>

namespace {

struct element_interface {
  virtual ~element_interface() = default;
  virtual int id() const = 0;
};

template <int Id> struct element : element_interface {
  int id() const override { return Id; }
};

struct key_a {};
struct key_b {};

// Not an aggregate: aggregate detection would make it of an int, which no
// binding provides.
struct value_type {
  value_type() {}
  int marker = 3;
};

struct keyed_interface {
  virtual ~keyed_interface() = default;
  virtual int marker() const = 0;
};

struct keyed_implementation : keyed_interface {
  explicit keyed_implementation(value_type &dependency)
      : dependency_(dependency) {}
  int marker() const override { return dependency_.marker; }

private:
  value_type &dependency_;
};

template <typename Range> std::vector<int> sorted_ids(const Range &elements) {
  std::vector<int> ids;
  for (const auto &element : elements) {
    ids.push_back(element->id());
  }
  std::sort(ids.begin(), ids.end());
  return ids;
}

// The arguments of one registration, given to a static container as a
// dingo::bind and to a runtime container as register_type.
template <typename... Args> struct registration {};

template <int Id, typename... Key>
using shared_element =
    registration<dingo::scope<dingo::shared>,
                 dingo::storage<std::shared_ptr<element<Id>>>,
                 dingo::interfaces<element_interface>, Key...>;

template <typename Registration> struct static_binding;
template <typename... Args> struct static_binding<registration<Args...>> {
  using type = dingo::bind<Args...>;
};

struct static_mode {
  template <typename... Registrations> struct with {
    dingo::static_container<
        dingo::bindings<typename static_binding<Registrations>::type...>>
        container;
  };
};

// A runtime container takes several unkeyed or key_a bindings of an interface
// only when its lookups say so.
struct collection_traits : dingo::runtime_container_traits {
  using lookup_definition_type =
      dingo::lookups<dingo::collection<element_interface>,
                     dingo::typed<key_a, element_interface, dingo::many>>;
};

struct runtime_mode {
  template <typename... Registrations> struct with {
    with() { (add(Registrations{}), ...); }

    dingo::runtime_container<collection_traits> container;

  private:
    template <typename... Args> void add(registration<Args...>) {
      container.template register_type<Args...>();
    }
  };
};

template <typename Mode> struct collections_test : testing::Test {};

using modes = testing::Types<static_mode, runtime_mode>;
TYPED_TEST_SUITE(collections_test, modes, );

TYPED_TEST(collections_test, resolves_every_unkeyed_binding_of_an_interface) {
  typename TypeParam::template with<shared_element<0>, shared_element<1>> setup;

  auto elements =
      setup.container
          .template resolve<std::vector<std::shared_ptr<element_interface>>>();

  EXPECT_EQ(sorted_ids(elements), (std::vector<int>{0, 1}));
}

TYPED_TEST(collections_test, resolves_every_binding_of_a_typed_key) {
  typename TypeParam::template with<shared_element<0, dingo::key_type<key_a>>,
                                    shared_element<1, dingo::key_type<key_a>>,
                                    shared_element<2, dingo::key_type<key_b>>>
      setup;

  auto elements =
      setup.container
          .template resolve<std::vector<std::shared_ptr<element_interface>>>(
              dingo::key_type<key_a>{});

  EXPECT_EQ(sorted_ids(elements), (std::vector<int>{0, 1}));
}

TYPED_TEST(collections_test, constructs_a_collection_of_every_binding) {
  typename TypeParam::template with<shared_element<0>, shared_element<1>> setup;

  auto elements = setup.container.template construct_collection<
      std::vector<std::shared_ptr<element_interface>>>();

  EXPECT_EQ(sorted_ids(elements), (std::vector<int>{0, 1}));
}

TYPED_TEST(collections_test,
           constructs_a_collection_through_an_insert_callable) {
  typename TypeParam::template with<shared_element<0>, shared_element<1>> setup;

  auto elements = setup.container.template construct_collection<
      std::map<int, std::shared_ptr<element_interface>>>(
      [](auto &collection, auto &&value) {
        collection.emplace(value->id(), std::move(value));
      });

  ASSERT_EQ(elements.size(), 2u);
  EXPECT_EQ(elements.at(0)->id(), 0);
  EXPECT_EQ(elements.at(1)->id(), 1);
}

TYPED_TEST(collections_test, typed_keys_select_bindings_of_one_interface) {
  typename TypeParam::template with<shared_element<0, dingo::key_type<key_a>>,
                                    shared_element<1, dingo::key_type<key_b>>>
      setup;

  auto &first =
      setup.container.template resolve<std::shared_ptr<element_interface> &>(
          dingo::key_type<key_a>{});
  auto &second =
      setup.container.template resolve<std::shared_ptr<element_interface> &>(
          dingo::key_type<key_b>{});

  EXPECT_EQ(first->id(), 0);
  EXPECT_EQ(second->id(), 1);
}

TYPED_TEST(collections_test, keyed_binding_resolves_its_dependencies_unkeyed) {
  typename TypeParam::template with<
      registration<dingo::scope<dingo::shared>, dingo::storage<value_type>>,
      registration<dingo::scope<dingo::shared>,
                   dingo::storage<std::shared_ptr<keyed_implementation>>,
                   dingo::interfaces<keyed_interface>, dingo::key_type<key_a>>>
      setup;

  auto &instance = setup.container.template resolve<keyed_interface &>(
      dingo::key_type<key_a>{});

  EXPECT_EQ(instance.marker(), 3);
}

template <typename Lookup>
struct runtime_key_traits : dingo::runtime_container_traits {
  using lookup_definition_type = dingo::lookups<Lookup>;
};

template <typename Traits> struct runtime_key_test : testing::Test {};

using runtime_key_traits_types = testing::Types<
    runtime_key_traits<dingo::associative<std::size_t, element_interface>>,
    runtime_key_traits<
        dingo::associative<std::size_t, dingo::interfaces<element_interface>>>>;
TYPED_TEST_SUITE(runtime_key_test, runtime_key_traits_types, );

TYPED_TEST(runtime_key_test, resolves_shared_pointers_by_runtime_key) {
  dingo::runtime_container<TypeParam> container;
  container.template register_type<dingo::scope<dingo::shared>,
                                   dingo::storage<std::shared_ptr<element<0>>>,
                                   dingo::interfaces<element_interface>>(
      dingo::key_value{std::size_t{0}});
  container.template register_type<dingo::scope<dingo::shared>,
                                   dingo::storage<std::shared_ptr<element<1>>>,
                                   dingo::interfaces<element_interface>>(
      dingo::key_value{std::size_t{1}});

  EXPECT_EQ(
      container
          .template resolve<std::shared_ptr<element_interface>>(std::size_t{0})
          ->id(),
      0);
  EXPECT_EQ(
      container
          .template resolve<std::shared_ptr<element_interface>>(std::size_t{1})
          ->id(),
      1);
}

TYPED_TEST(runtime_key_test, typed_keys_resolve_beside_runtime_key_lookups) {
  dingo::runtime_container<TypeParam> container;
  container.template register_type<dingo::scope<dingo::shared>,
                                   dingo::storage<value_type>,
                                   dingo::key_type<key_a>>();

  EXPECT_EQ(
      container.template resolve<value_type>(dingo::key_type<key_a>{}).marker,
      3);
  EXPECT_EQ(
      container.template resolve<value_type &>(dingo::key_type<key_a>{}).marker,
      3);
}

} // namespace
