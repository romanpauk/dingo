//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

// Lookup declarations and collections in the next implementation: a domain
// declared many holds any number of bindings, which a collection request
// gathers in the order of registration.

#include <dingo/container.h>
#include <dingo/lookup/lookup.h>
#include <dingo/static_container.h>
#include <dingo/storage/shared.h>
#include <dingo/storage/unique.h>
#include <dingo/type/collection_traits.h>

#include <gtest/gtest.h>

#include <map>
#include <memory>
#include <utility>
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

std::vector<int> ids(const std::vector<iface *> &collection) {
  std::vector<int> result;
  for (iface *element : collection) {
    result.push_back(element->id());
  }
  return result;
}

struct many_traits : dingo::dynamic_container_traits {
  using lookup_definition_type =
      dingo::lookups<dingo::collection<iface>,
                     dingo::typed<tag_a, iface, dingo::many>,
                     dingo::associative<int, iface, dingo::many>>;
};

TEST(next_collections, a_domain_declared_many_holds_any_number) {
  dingo::container<many_traits> container;
  container.register_type<scope<dingo::shared>, storage<impl<0>>,
                          dingo::interfaces<iface>>();
  container.register_type<scope<dingo::shared>, storage<impl<1>>,
                          dingo::interfaces<iface>>();
  container.register_type<scope<dingo::shared>, storage<impl<2>>,
                          dingo::interfaces<iface>>(dingo::key_value{7});
  container.register_type<scope<dingo::shared>, storage<impl<3>>,
                          dingo::interfaces<iface>, dingo::key_type<int, 7>>();
  EXPECT_THROW(container.resolve<iface &>(), dingo::type_ambiguous_exception);
  EXPECT_EQ(ids(container.construct_collection<std::vector<iface *>>()),
            (std::vector<int>{0, 1}));
  EXPECT_EQ(ids(container.construct_collection<std::vector<iface *>>(
                dingo::key_value{7})),
            (std::vector<int>{2, 3}));
  EXPECT_EQ(ids(container.resolve<std::vector<iface *>>(7)),
            (std::vector<int>{2, 3}));
  EXPECT_TRUE(
      container
          .construct_collection<std::vector<iface *>>(dingo::key_type<tag_a>())
          .empty());
}

TEST(next_collections, a_collection_of_a_domain_of_one_holds_its_binding) {
  dingo::container<> container;
  container.register_type<scope<dingo::shared>, storage<impl<4>>,
                          dingo::interfaces<iface>>();
  EXPECT_EQ(ids(container.resolve<std::vector<iface *>>()),
            (std::vector<int>{4}));
  EXPECT_TRUE(
      container.resolve<std::vector<iface *>>(dingo::key_value{1}).empty());
}

struct consumer {
  explicit consumer(std::vector<iface *> all) : all(std::move(all)) {}
  std::vector<iface *> all;
};

TEST(next_collections, injects_a_collection) {
  dingo::container<many_traits> container;
  container.register_type<scope<dingo::shared>, storage<impl<5>>,
                          dingo::interfaces<iface>>();
  container.register_type<scope<dingo::shared>, storage<impl<6>>,
                          dingo::interfaces<iface>>();
  EXPECT_EQ(ids(container.construct<consumer>().all), (std::vector<int>{5, 6}));
  int appended = 0;
  container.register_type_collection<scope<dingo::unique>,
                                     storage<std::vector<iface *>>>(
      [&appended](std::vector<iface *> &collection, iface *element) {
        ++appended;
        collection.push_back(element);
      });
  EXPECT_EQ(ids(container.resolve<std::vector<iface *>>()),
            (std::vector<int>{5, 6}));
  EXPECT_EQ(appended, 2);
}

struct many_static_traits : dingo::static_container_traits {
  using lookup_definition_type =
      dingo::lookups<dingo::collection<iface>,
                     dingo::associative<int, iface, dingo::many>>;
};

TEST(next_collections, gathers_static_bindings) {
  using bindings = dingo::bindings<
      dingo::bind<scope<dingo::shared>, storage<impl<0>>,
                  dingo::interfaces<iface>>,
      dingo::bind<scope<dingo::shared>, storage<impl<1>>,
                  dingo::interfaces<iface>>,
      dingo::bind<scope<dingo::shared>, storage<impl<2>>,
                  dingo::interfaces<iface>, dingo::key_type<int, 7>>,
      dingo::bind<scope<dingo::shared>, storage<impl<3>>,
                  dingo::interfaces<iface>, dingo::key_type<int, 7>>>;
  dingo::static_container<bindings, many_static_traits> container;
  EXPECT_EQ(ids(container.construct_collection<std::vector<iface *>>()),
            (std::vector<int>{0, 1}));
  EXPECT_EQ(ids(container.construct_collection<std::vector<iface *>>(
                dingo::key_type<int, 7>())),
            (std::vector<int>{2, 3}));
  EXPECT_EQ(ids(container.resolve<std::vector<iface *>>(7)),
            (std::vector<int>{2, 3}));
  EXPECT_TRUE(container.resolve<std::vector<iface *>>(8).empty());
  EXPECT_EQ(ids(container.construct<consumer>().all), (std::vector<int>{0, 1}));
}

TEST(next_collections, an_empty_static_collection_is_valid) {
  dingo::static_container<dingo::bindings<>> container;
  EXPECT_TRUE(container.construct_collection<std::vector<iface *>>().empty());
}

} // namespace

namespace {

struct collected_tag {};

TEST(next_collections, registers_a_collection_of_a_key) {
  struct traits : dingo::dynamic_container_traits {
    using lookup_definition_type =
        dingo::lookups<dingo::typed<collected_tag, iface, dingo::many>>;
  };
  dingo::container<traits> container;
  container.register_type<scope<dingo::shared>, storage<impl<1>>,
                          dingo::interfaces<iface>,
                          dingo::key_type<collected_tag>>();
  container.register_type<scope<dingo::shared>, storage<impl<2>>,
                          dingo::interfaces<iface>>();
  container.register_type_collection<scope<dingo::unique>,
                                     storage<std::vector<iface *>>>(
      dingo::key_type<collected_tag>{});
  std::vector<iface *> keyed = container.resolve<std::vector<iface *>>();
  ASSERT_EQ(keyed.size(), 1u);
  EXPECT_EQ(keyed[0]->id(), 1);
}

} // namespace

namespace {

using move_only_map = std::map<int, std::unique_ptr<iface>>;

struct move_only_consumer {
  explicit move_only_consumer(move_only_map &&all) : all(std::move(all)) {}
  move_only_consumer(const move_only_consumer &) = delete;
  move_only_consumer(move_only_consumer &&) = default;
  move_only_map all;
};

struct move_only_traits : dingo::dynamic_container_traits {
  using lookup_definition_type = dingo::lookups<dingo::collection<iface>>;
};

TEST(next_collections,
     constructs_from_a_registered_collection_of_move_only_elements) {
  dingo::container<move_only_traits> container;
  container
      .register_type<scope<dingo::unique>, storage<std::unique_ptr<impl<1>>>,
                     dingo::interfaces<iface>>();
  container
      .register_type<scope<dingo::unique>, storage<std::unique_ptr<impl<2>>>,
                     dingo::interfaces<iface>>();
  container
      .register_type_collection<scope<dingo::unique>, storage<move_only_map>>(
          [](auto &collection, auto &&element) {
            const int id = element->id();
            collection.emplace(id, std::forward<decltype(element)>(element));
          });
  move_only_consumer consumer = container.construct<move_only_consumer>();
  ASSERT_EQ(consumer.all.size(), 2u);
  EXPECT_EQ(consumer.all.at(1)->id(), 1);
  EXPECT_EQ(consumer.all.at(2)->id(), 2);
}

TEST(next_collections, collects_move_only_elements_no_binding_stores) {
  dingo::container<move_only_traits> container;
  container
      .register_type<scope<dingo::unique>, storage<std::unique_ptr<impl<3>>>,
                     dingo::interfaces<iface>>();
  std::vector<std::unique_ptr<iface>> all =
      container.resolve<std::vector<std::unique_ptr<iface>>>();
  ASSERT_EQ(all.size(), 1u);
  EXPECT_EQ(all[0]->id(), 3);
}

} // namespace
