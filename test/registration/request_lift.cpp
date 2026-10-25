//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#include <dingo/container.h>
#include <dingo/factory/function.h>
#include <dingo/static_container.h>
#include <dingo/storage/external.h>
#include <dingo/storage/shared.h>
#include <dingo/storage/unique.h>

#include "support/custom_wrappers.h"

#include <gtest/gtest.h>

#include <memory>
#include <optional>
#include <utility>

namespace request_lift_test {

struct tracked {
  static inline int constructions = 0;
  static inline int copies = 0;
  static inline int moves = 0;
  static inline int destructions = 0;
  static inline int next_id = 0;

  tracked() : id(++next_id) { ++constructions; }
  tracked(const tracked &other) : id(other.id) { ++copies; }
  tracked(tracked &&other) noexcept : id(other.id) { ++moves; }
  ~tracked() { ++destructions; }

  static void reset() {
    constructions = 0;
    copies = 0;
    moves = 0;
    destructions = 0;
    next_id = 0;
  }

  static void expect_balanced() {
    EXPECT_EQ(destructions, constructions + copies + moves);
  }

  int id;
};

std::optional<tracked> make_optional() {
  return std::optional<tracked>(std::in_place);
}

std::optional<tracked> make_empty_optional() {
  return std::optional<tracked>();
}

std::shared_ptr<tracked> make_shared() { return std::make_shared<tracked>(); }

std::unique_ptr<tracked> make_unique() { return std::make_unique<tracked>(); }

tracked *make_pointer() { return new tracked; }

dingo::test_optional<tracked> make_test_optional() {
  return dingo::test_optional<tracked>(tracked{});
}

TEST(request_lift, lifts_a_value_into_an_optional) {
  tracked::reset();
  {
    dingo::container<> container;
    container
        .register_type<dingo::scope<dingo::shared>, dingo::storage<tracked>>();

    auto value = container.resolve<std::optional<tracked>>();
    ASSERT_TRUE(value.has_value());
    EXPECT_EQ(value->id, 1);
    EXPECT_EQ(container.resolve<tracked>().id, value->id);
  }
  tracked::expect_balanced();
}

TEST(request_lift, lifts_a_shared_handle_into_an_optional) {
  tracked::reset();
  {
    dingo::container<> container;
    container.register_type<dingo::scope<dingo::shared>,
                            dingo::storage<std::shared_ptr<tracked>>,
                            dingo::factory<dingo::function<&make_shared>>>();
    auto expected = container.resolve<std::shared_ptr<tracked>>();

    auto wrapped = container.resolve<std::optional<std::shared_ptr<tracked>>>();
    ASSERT_TRUE(wrapped.has_value());
    EXPECT_EQ(wrapped->get(), expected.get());
  }
  tracked::expect_balanced();
}

TEST(request_lift, lifts_a_consumed_optional_into_a_shared_handle) {
  tracked::reset();
  {
    dingo::container<> container;
    container.register_type<dingo::scope<dingo::unique>,
                            dingo::storage<std::optional<tracked>>,
                            dingo::factory<dingo::function<&make_optional>>>();

    auto value = container.resolve<std::shared_ptr<std::optional<tracked>>>();
    ASSERT_TRUE(value);
    ASSERT_TRUE(*value);
  }
  tracked::expect_balanced();
}

TEST(request_lift, lifts_a_consumed_optional_into_a_unique_handle) {
  tracked::reset();
  {
    dingo::container<> container;
    container.register_type<dingo::scope<dingo::unique>,
                            dingo::storage<std::optional<tracked>>,
                            dingo::factory<dingo::function<&make_optional>>>();

    auto value = container.resolve<std::unique_ptr<std::optional<tracked>>>();
    ASSERT_TRUE(value);
    ASSERT_TRUE(*value);
  }
  tracked::expect_balanced();
}

TEST(request_lift, lifts_a_consumed_value_into_a_shared_handle) {
  tracked::reset();
  {
    dingo::container<> container;
    container
        .register_type<dingo::scope<dingo::unique>, dingo::storage<tracked>>();

    auto value = container.resolve<std::shared_ptr<tracked>>();
    ASSERT_TRUE(value);
    EXPECT_EQ(value->id, 1);
  }
  tracked::expect_balanced();
}

TEST(request_lift, a_direct_stored_wrapper_wins_over_the_lift) {
  tracked::reset();
  {
    dingo::container<> container;
    container.register_type<
        dingo::scope<dingo::unique>, dingo::storage<std::optional<tracked>>,
        dingo::factory<dingo::function<&make_empty_optional>>>();

    auto value = container.resolve<std::optional<tracked>>();
    EXPECT_FALSE(value.has_value());
  }
  tracked::expect_balanced();
}

TEST(request_lift, borrowed_storage_does_not_lift_into_an_owner) {
  dingo::container<> container;
  container
      .register_type<dingo::scope<dingo::shared>, dingo::storage<tracked>>();
  EXPECT_THROW((void)container.resolve<std::shared_ptr<tracked>>(),
               dingo::type_not_convertible_exception);
}

TEST(request_lift, borrowed_pointer_does_not_lift_into_an_owner) {
  tracked value;
  const int id = value.id;
  dingo::container<> container;
  container
      .register_type<dingo::scope<dingo::external>, dingo::storage<tracked *>>(
          &value);
  EXPECT_THROW((void)container.resolve<std::shared_ptr<tracked>>(),
               dingo::type_not_convertible_exception);
  EXPECT_EQ(value.id, id);
}

TEST(request_lift, a_raw_pointer_is_not_lifted_into_a_wrapper) {
  dingo::container<> container;
  container
      .register_type<dingo::scope<dingo::unique>, dingo::storage<tracked *>,
                     dingo::factory<dingo::function<&make_pointer>>>();
  EXPECT_THROW((void)container.resolve<std::optional<tracked *>>(),
               dingo::type_not_convertible_exception);
}

TEST(request_lift, works_with_user_optional_and_handles) {
  tracked::reset();
  {
    dingo::container<> container;
    container
        .register_type<dingo::scope<dingo::shared>, dingo::storage<tracked>>();

    auto optional = container.resolve<dingo::test_optional<tracked>>();
    ASSERT_NE(optional.get(), nullptr);
    EXPECT_EQ(optional.get()->id, 1);
  }
  tracked::expect_balanced();

  tracked::reset();
  {
    dingo::container<> container;
    container
        .register_type<dingo::scope<dingo::unique>, dingo::storage<tracked>>();

    auto shared = container.resolve<dingo::test_shared<tracked>>();
    ASSERT_NE(shared.get(), nullptr);
    auto unique = container.resolve<dingo::test_unique<tracked>>();
    ASSERT_NE(unique.get(), nullptr);
  }
  tracked::expect_balanced();
}

TEST(request_lift, works_with_a_user_handle_of_a_user_optional) {
  tracked::reset();
  {
    dingo::container<> container;
    container
        .register_type<dingo::scope<dingo::unique>,
                       dingo::storage<dingo::test_optional<tracked>>,
                       dingo::factory<dingo::function<&make_test_optional>>>();

    auto value =
        container.resolve<dingo::test_shared<dingo::test_optional<tracked>>>();
    ASSERT_NE(value.get(), nullptr);
    ASSERT_NE(value.get()->get(), nullptr);
  }
  tracked::expect_balanced();
}

TEST(request_lift, user_owner_is_not_lifted_from_borrowed_storage) {
  dingo::container<> container;
  container
      .register_type<dingo::scope<dingo::shared>, dingo::storage<tracked>>();
  EXPECT_THROW((void)container.resolve<dingo::test_shared<tracked>>(),
               dingo::type_not_convertible_exception);
}

TEST(request_lift, works_through_the_static_container) {
  tracked::reset();
  {
    using bindings = dingo::bindings<
        dingo::bind<dingo::scope<dingo::shared>, dingo::storage<tracked>>>;
    dingo::static_container<bindings> container;

    auto optional = container.resolve<std::optional<tracked>>();
    ASSERT_TRUE(optional.has_value());
    EXPECT_EQ(optional->id, 1);
  }
  tracked::expect_balanced();

  tracked::reset();
  {
    using bindings = dingo::bindings<dingo::bind<
        dingo::scope<dingo::unique>, dingo::storage<std::optional<tracked>>,
        dingo::factory<dingo::function<&make_optional>>>>;
    dingo::static_container<bindings> container;

    auto value = container.resolve<std::shared_ptr<std::optional<tracked>>>();
    ASSERT_TRUE(value);
    ASSERT_TRUE(*value);
  }
  tracked::expect_balanced();
}

} // namespace request_lift_test
