//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

// Static bindings of the next implementation: what a static and a mixed
// container serve from the bindings of their type, the dependencies of the
// objects they construct, and the temporaries of their requests.

#include <dingo/container.h>
#include <dingo/factory/function.h>
#include <dingo/lookup/lookup.h>
#include <dingo/registration/constructor.h>
#include <dingo/static_container.h>
#include <dingo/storage/shared.h>
#include <dingo/storage/shared_cyclical.h>
#include <dingo/storage/unique.h>
#include <dingo/type/annotated.h>

#include <gtest/gtest.h>

#include <memory>
#include <optional>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace {

using dingo::scope;
using dingo::storage;

std::optional<int> make_optional() { return 11; }

using nested_optional = std::optional<std::optional<std::variant<int, float>>>;

nested_optional make_nested_optional() {
  return nested_optional{std::in_place, std::in_place, std::in_place_type<int>,
                         17};
}

// spec: behavior_changes.borrowing_is_compositional
TEST(next_static_bindings, borrows_the_value_of_a_borrowable_storage) {
  using bindings = dingo::bindings<dingo::bind<
      scope<dingo::shared>, storage<std::optional<int>>, dingo::interfaces<int>,
      dingo::factory<dingo::function<make_optional>>>>;

  dingo::static_container<bindings> container;
  auto &optional = container.resolve<const std::optional<int> &>();

  EXPECT_EQ(container.resolve<int>(), 11);
  EXPECT_EQ(container.resolve<const int>(), 11);
  EXPECT_EQ(&container.resolve<const int &>(), &*optional);
  EXPECT_EQ(container.resolve<const int *>(), &*optional);
}

TEST(next_static_bindings, borrows_the_leaf_of_a_nested_borrowable_storage) {
  using bindings = dingo::bindings<dingo::bind<
      scope<dingo::shared>, storage<nested_optional>, dingo::interfaces<int>,
      dingo::factory<dingo::function<make_nested_optional>>>>;

  dingo::static_container<bindings> container;
  auto &value = container.resolve<const int &>();

  EXPECT_EQ(container.resolve<int>(), 17);
  EXPECT_EQ(value, 17);
  EXPECT_EQ(container.resolve<const int *>(), &value);
}

struct config {
  config() : value(1) {}
  int value;
};

struct detected_service {
  explicit detected_service(config &init_dependency)
      : dependency(&init_dependency) {}
  config *dependency;
};

struct declared_service {
  DINGO_CONSTRUCTOR(declared_service(config &init_dependency))
      : dependency(&init_dependency) {}
  config *dependency;
};

// The dependencies of a detected constructor are known when the object is
// constructed, and those of a declared one when the container is compiled;
// both are resolved by the container. spec: graph_rules.cycle
TEST(next_static_bindings, resolves_the_dependencies_of_a_constructor) {
  using bindings = dingo::bindings<
      dingo::bind<scope<dingo::shared>, storage<config>>,
      dingo::bind<scope<dingo::unique>, storage<detected_service>>,
      dingo::bind<scope<dingo::unique>, storage<declared_service>>>;

  dingo::static_container<bindings> container;
  config &shared = container.resolve<config &>();

  EXPECT_EQ(shared.value, 1);
  EXPECT_EQ(container.resolve<detected_service>().dependency, &shared);
  EXPECT_EQ(container.resolve<declared_service>().dependency, &shared);
}

struct first_key : std::integral_constant<int, 0> {};
struct second_key : std::integral_constant<int, 1> {};

struct keyed_service {
  explicit keyed_service(
      dingo::dependency<config &, dingo::key_type<first_key>> init_dependency)
      : dependency(&static_cast<config &>(init_dependency)) {}
  config *dependency;
};

TEST(next_static_bindings,
     resolves_a_keyed_dependency_from_the_binding_of_its_key) {
  using bindings =
      dingo::bindings<dingo::bind<scope<dingo::shared>, storage<config>,
                                  dingo::key_type<first_key>>,
                      dingo::bind<scope<dingo::shared>, storage<config>,
                                  dingo::key_type<second_key>>,
                      dingo::bind<scope<dingo::unique>, storage<keyed_service>,
                                  dingo::dependencies<dingo::dependency<
                                      config &, dingo::key_type<first_key>>>>>;

  dingo::static_container<bindings> container;
  config &first = container.resolve<config &>(dingo::key_type<first_key>());
  config &second = container.resolve<config &>(dingo::key_type<second_key>());

  EXPECT_NE(&first, &second);
  EXPECT_EQ(container.resolve<keyed_service>().dependency, &first);
}

struct first_tag {};
struct second_tag {};

struct valued {
  virtual ~valued() = default;
  virtual int value() const = 0;
};

struct first_valued : valued {
  int value() const override { return 11; }
};

struct second_valued : valued {
  int value() const override { return 17; }
};

TEST(next_static_bindings, invokes_a_callable_with_annotated_static_bindings) {
  using bindings = dingo::bindings<
      dingo::bind<scope<dingo::shared>, storage<std::shared_ptr<first_valued>>,
                  dingo::interfaces<dingo::annotated<valued, first_tag>>>,
      dingo::bind<scope<dingo::shared>, storage<std::shared_ptr<second_valued>>,
                  dingo::interfaces<dingo::annotated<valued, second_tag>>>>;

  dingo::container<bindings> container;
  const auto values = container.invoke(
      [](dingo::annotated<std::shared_ptr<valued>, first_tag> first,
         dingo::annotated<std::shared_ptr<valued>, second_tag> second) {
        return std::pair(static_cast<std::shared_ptr<valued>>(first)->value(),
                         static_cast<std::shared_ptr<valued>>(second)->value());
      });

  EXPECT_EQ(values.first, 11);
  EXPECT_EQ(values.second, 17);
}

struct cycle_interface {
  virtual ~cycle_interface() = default;
};

struct cycle_second;

struct cycle_first {
  explicit cycle_first(cycle_interface &init_dependency)
      : dependency(&init_dependency) {}
  cycle_interface *dependency;
};

struct cycle_second : cycle_interface {
  explicit cycle_second(cycle_first &init_dependency)
      : dependency(&init_dependency) {}
  cycle_first *dependency;
};

TEST(next_static_bindings,
     a_mixed_container_resolves_shared_cyclical_interface_handles) {
  using bindings = dingo::bindings<
      dingo::bind<scope<dingo::shared_cyclical>, storage<cycle_first>,
                  dingo::dependencies<cycle_interface &>>,
      dingo::bind<scope<dingo::shared_cyclical>,
                  storage<std::shared_ptr<cycle_second>>,
                  dingo::interfaces<cycle_interface>,
                  dingo::dependencies<cycle_first &>>>;

  dingo::container<bindings> container;
  auto &instance = container.resolve<cycle_first &>();
  auto &view = container.resolve<cycle_interface &>();
  auto &second = static_cast<cycle_second &>(view);

  EXPECT_EQ(instance.dependency, &view);
  EXPECT_EQ(second.dependency, &instance);
  EXPECT_EQ(&container.resolve<cycle_first &>(), &instance);
  EXPECT_EQ(&container.resolve<cycle_interface &>(), &view);
}

struct collection_interface {
  virtual ~collection_interface() = default;
};
struct first_element : collection_interface {};
struct second_element : collection_interface {};

struct consumer {
  explicit consumer(std::vector<collection_interface *> init_values)
      : values(std::move(init_values)) {}
  std::vector<collection_interface *> values;
};

using collection_bindings = dingo::bindings<
    dingo::bind<scope<dingo::shared>, storage<first_element>,
                dingo::interfaces<collection_interface>>,
    dingo::bind<scope<dingo::shared>, storage<second_element>,
                dingo::interfaces<collection_interface>>,
    dingo::bind<scope<dingo::unique>, storage<consumer>,
                dingo::dependencies<std::vector<collection_interface *>>>>;

struct static_many_traits : dingo::static_container_traits {
  using lookup_definition_type =
      dingo::lookups<dingo::collection<collection_interface>>;
};

struct mixed_many_traits : dingo::dynamic_container_traits {
  using lookup_definition_type =
      dingo::lookups<dingo::collection<collection_interface>>;
};

// Two bindings of one interface are held only where its lookup is declared
// many. spec: behavior_changes.duplicates_rejected_in_both_domains
template <typename Container> void check_collection(Container &container) {
  auto values = container.template resolve<consumer>().values;
  ASSERT_EQ(values.size(), 2u);
  EXPECT_NE(dynamic_cast<first_element *>(values[0]), nullptr);
  EXPECT_NE(dynamic_cast<second_element *>(values[1]), nullptr);
  EXPECT_EQ(container.template resolve<consumer>().values, values);
}

TEST(next_static_bindings,
     injects_a_collection_of_static_bindings_of_a_domain_declared_many) {
  dingo::static_container<collection_bindings, static_many_traits> fixed;
  check_collection(fixed);
  dingo::container<collection_bindings, mixed_many_traits> mixed;
  check_collection(mixed);
}

// Objects that count their constructions and destructions.
struct counts {
  static inline int made = 0;
  static inline int destroyed = 0;
  static void reset() { made = destroyed = 0; }
  static int live() { return made - destroyed; }
};

struct leaf {
  leaf() { ++counts::made; }
  leaf(const leaf &) { ++counts::made; }
  leaf(leaf &&) noexcept { ++counts::made; }
  ~leaf() { ++counts::destroyed; }
  int value = 5;
};

struct other_leaf : leaf {
  other_leaf() { value = 6; }
};

struct middle {
  explicit middle(leaf init) : value(init.value + 1) {}
  int value;
};

struct root {
  explicit root(middle init) : value(init.value + 1) {}
  int value;
};

struct pair_root {
  pair_root(leaf left, std::shared_ptr<other_leaf> right)
      : value(left.value + right->value) {}
  int value;
};

// A consumed object is passed by value, never borrowed. spec:
// behavior_changes.consumed_objects_are_never_borrowed
TEST(next_static_bindings,
     constructs_unique_dependencies_and_destroys_their_temporaries) {
  counts::reset();
  {
    using bindings = dingo::bindings<
        dingo::bind<scope<dingo::unique>, storage<leaf>>,
        dingo::bind<scope<dingo::unique>, storage<std::shared_ptr<other_leaf>>>,
        dingo::bind<scope<dingo::unique>, storage<middle>,
                    dingo::dependencies<leaf>>,
        dingo::bind<scope<dingo::unique>, storage<root>,
                    dingo::dependencies<middle>>,
        dingo::bind<scope<dingo::unique>, storage<pair_root>,
                    dingo::dependencies<leaf, std::shared_ptr<other_leaf>>>>;
    dingo::static_container<bindings> container;

    EXPECT_EQ(container.resolve<root>().value, 7);
    EXPECT_EQ(counts::live(), 0);
    EXPECT_EQ(container.resolve<pair_root>().value, 11);
    EXPECT_EQ(counts::live(), 0);
    std::shared_ptr<other_leaf> handle =
        container.resolve<std::shared_ptr<other_leaf>>();
    EXPECT_EQ(handle->value, 6);
    EXPECT_EQ(counts::live(), 1);
  }
  EXPECT_EQ(counts::live(), 0);
}

struct shared_root {
  explicit shared_root(leaf init) : value(init.value) {}
  int value;
};

TEST(next_static_bindings,
     a_shared_object_keeps_nothing_of_its_consumed_dependencies) {
  counts::reset();
  {
    using bindings =
        dingo::bindings<dingo::bind<scope<dingo::unique>, storage<leaf>>,
                        dingo::bind<scope<dingo::shared>, storage<shared_root>,
                                    dingo::dependencies<leaf>>>;
    dingo::static_container<bindings> container;
    shared_root &object = container.resolve<shared_root &>();

    EXPECT_EQ(object.value, 5);
    EXPECT_EQ(counts::live(), 0);
    EXPECT_EQ(&container.resolve<shared_root &>(), &object);
    EXPECT_EQ(counts::live(), 0);
  }
  EXPECT_EQ(counts::live(), 0);
}

TEST(next_static_bindings,
     keeps_the_object_of_an_rvalue_request_until_the_container_is_destroyed) {
  counts::reset();
  {
    dingo::static_container<
        dingo::bindings<dingo::bind<scope<dingo::unique>, storage<leaf>>>>
        container;
    leaf &&kept = container.resolve<leaf &&>();

    EXPECT_EQ(kept.value, 5);
    EXPECT_EQ(counts::live(), 1);
    std::optional<leaf> wrapped = container.resolve<std::optional<leaf>>();
    ASSERT_TRUE(wrapped.has_value());
    EXPECT_EQ(counts::live(), 2);
  }
  EXPECT_EQ(counts::live(), 0);
}

struct failing {
  static inline bool fail = true;
  failing() {
    if (fail) {
      throw std::runtime_error("failed");
    }
  }
};

struct cyclical_node {
  explicit cyclical_node(failing &) { ++counts::made; }
  ~cyclical_node() { ++counts::destroyed; }
};

TEST(next_static_bindings,
     a_failed_shared_cyclical_construction_is_rolled_back_statically) {
  counts::reset();
  failing::fail = true;
  using bindings = dingo::bindings<
      dingo::bind<scope<dingo::shared_cyclical>, storage<cyclical_node>,
                  dingo::dependencies<failing &>>,
      dingo::bind<scope<dingo::shared>, storage<failing>>>;
  dingo::static_container<bindings> container;

  EXPECT_THROW(container.resolve<cyclical_node &>(), std::runtime_error);
  EXPECT_EQ(counts::live(), 0);
  failing::fail = false;
  cyclical_node &node = container.resolve<cyclical_node &>();
  EXPECT_EQ(&container.resolve<cyclical_node &>(), &node);
  EXPECT_EQ(counts::live(), 1);
}

} // namespace
