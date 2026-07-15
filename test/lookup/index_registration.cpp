//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#include "index_common.h"

namespace dingo {

TEST(index_test,
     normal_unkeyed_singular_registration_uses_allocator_owned_storage) {
  struct processor {
    virtual ~processor() = default;
    virtual int id() const = 0;
  };
  struct processor_impl : processor {
    int id() const override { return 42; }
  };
  struct large_processor_impl : processor {
    char payload[1024] = {};

    int id() const override { return 7; }
  };

  test_allocator_state state;
  {
    test_allocator<char> allocator(&state);
    container<dynamic_container_traits, test_allocator<char>> container(
        allocator);

    container.template register_type<scope<shared>, storage<processor_impl>,
                                     interfaces<processor>>();

    ASSERT_GT(state.allocations, 0);
    ASSERT_EQ(container.template resolve<processor &>().id(), 42);
  }

  ASSERT_EQ(state.deallocations, state.allocations);

  test_allocator_state large_state;
  {
    test_allocator<char> allocator(&large_state);
    container<dynamic_container_traits, test_allocator<char>> container(
        allocator);

    container.template register_type<
        scope<shared>, storage<large_processor_impl>, interfaces<processor>>();

    ASSERT_GT(large_state.allocations, 0);
    ASSERT_GE(large_state.largest_allocation_bytes,
              sizeof(large_processor_impl));
  }

  ASSERT_EQ(large_state.deallocations, large_state.allocations);
}

TEST(index_test, shared_resolution_without_local_bindings_avoids_allocation) {
  struct service {};

  test_allocator_state state;
  {
    test_allocator<char> allocator(&state);
    container<dynamic_container_traits, test_allocator<char>> container(
        allocator);
    container.template register_type<scope<shared>, storage<service>>();
    const auto allocations_after_registration = state.allocations;

    (void)container.template resolve<service &>();

    EXPECT_EQ(state.allocations, allocations_after_registration);
  }

  EXPECT_EQ(state.deallocations, state.allocations);
}

TEST(index_test, typed_key_singular_registration_uses_allocator_owned_storage) {
  struct processor_key {};
  struct processor {
    virtual ~processor() = default;
    virtual int id() const = 0;
  };
  struct processor_impl : processor {
    int id() const override { return 42; }
  };
  struct large_processor_impl : processor {
    char payload[1024] = {};

    int id() const override { return 7; }
  };

  test_allocator_state state;
  {
    test_allocator<char> allocator(&state);
    container<dynamic_container_traits, test_allocator<char>> container(
        allocator);

    container.template register_type<scope<shared>, storage<processor_impl>,
                                     interfaces<processor>,
                                     key_type<processor_key>>();

    ASSERT_GT(state.allocations, 0);
    ASSERT_EQ(
        container.template resolve<processor &>(key_type<processor_key>{}).id(),
        42);
  }

  ASSERT_EQ(state.deallocations, state.allocations);

  test_allocator_state large_state;
  {
    test_allocator<char> allocator(&large_state);
    container<dynamic_container_traits, test_allocator<char>> container(
        allocator);

    container.template register_type<
        scope<shared>, storage<large_processor_impl>, interfaces<processor>,
        key_type<processor_key>>();

    ASSERT_GT(large_state.allocations, 0);
    ASSERT_GE(large_state.largest_allocation_bytes,
              sizeof(large_processor_impl));
  }

  ASSERT_EQ(large_state.deallocations, large_state.allocations);
}

TEST(index_test, normal_unkeyed_duplicate_storage_rejects_no_key_one_row) {
  struct processor {
    virtual ~processor() = default;
    virtual int id() const = 0;
  };
  struct first_processor : processor {
    int id() const override { return 1; }
  };
  struct second_processor : processor {
    int id() const override { return 2; }
  };

  container<> container;
  container.template register_type<scope<shared>, storage<first_processor>,
                                   interfaces<processor>>();

  ASSERT_EQ(container.template resolve<processor &>().id(), 1);

  EXPECT_THROW(
      (container.template register_type<
          scope<shared>, storage<second_processor>, interfaces<processor>>()),
      lookup_already_registered_exception);

  auto processors =
      container.template resolve<tracked_collection<processor *>>();
  ASSERT_EQ(processors.reserve_count, 1U);
  ASSERT_EQ(processors.values.size(), 1U);
  ASSERT_EQ(processors.values[0]->id(), 1);
  ASSERT_EQ(container.template resolve<processor &>().id(), 1);
}

TEST(index_test, normal_unkeyed_collection_uses_no_key_one_rows) {
  struct processor {
    virtual ~processor() = default;
    virtual int id() const = 0;
  };
  struct first_processor : processor {
    int id() const override { return 1; }
  };
  struct second_processor : processor {
    int id() const override { return 2; }
  };

  container<> container;
  container.template register_type<scope<shared>, storage<first_processor>,
                                   interfaces<processor>>();
  EXPECT_THROW(
      (container.template register_type<
          scope<shared>, storage<second_processor>, interfaces<processor>>()),
      lookup_already_registered_exception);

  auto processors =
      container.template resolve<tracked_collection<processor *>>();

  ASSERT_EQ(processors.reserve_count, 1U);
  ASSERT_EQ(processors.values.size(), 1U);
  ASSERT_EQ(processors.values[0]->id(), 1);
}

TEST(index_test, failed_normal_unkeyed_registration_preserves_binding) {
  typed_key_cached_processor_impl::constructions = 0;

  struct traits : dynamic_container_traits {
    using lookup_definition_type = lookups<single<typed_key_cached_processor>>;
  };

  container<traits> container;
  container.template register_type<scope<shared>,
                                   storage<typed_key_cached_processor_impl>,
                                   interfaces<typed_key_cached_processor>>();

  auto &before = container.template resolve<typed_key_cached_processor &>();

  EXPECT_THROW((container.template register_type<
                   scope<shared>, storage<typed_key_cached_processor_impl>,
                   interfaces<typed_key_cached_processor>>()),
               lookup_already_registered_exception);

  auto &after = container.template resolve<typed_key_cached_processor &>();
  ASSERT_EQ(&after, &before);
  ASSERT_EQ(after.id(), 42);

  EXPECT_THROW((container.template resolve<
                   tracked_collection<typed_key_cached_processor *>>()),
               type_not_found_exception);
  ASSERT_EQ(typed_key_cached_processor_impl::constructions, 1);
}

TEST(index_test, failed_normal_unkeyed_registration_preserves_collection_rows) {
  typed_key_cached_processor_impl::constructions = 0;

  container<> container;
  container.template register_type<scope<shared>,
                                   storage<typed_key_cached_processor_impl>,
                                   interfaces<typed_key_cached_processor>>();

  auto &before = container.template resolve<typed_key_cached_processor &>();

  EXPECT_THROW((container.template register_type<
                   scope<shared>, storage<typed_key_cached_processor_impl>,
                   interfaces<typed_key_cached_processor>>()),
               lookup_already_registered_exception);

  auto &after = container.template resolve<typed_key_cached_processor &>();
  ASSERT_EQ(&after, &before);

  auto processors =
      container
          .template resolve<tracked_collection<typed_key_cached_processor *>>();
  ASSERT_EQ(processors.reserve_count, 1U);
  ASSERT_EQ(processors.values.size(), 1U);
  ASSERT_EQ(processors.values[0]->id(), 42);
  ASSERT_EQ(typed_key_cached_processor_impl::constructions, 1);
}

TEST(index_test, key_valueed_collection_requires_associative_many_lookup) {
  struct processor {
    virtual ~processor() = default;
    virtual int id() const = 0;
  };
  struct processor_impl : processor {
    int id() const override { return 1; }
  };

  struct traits : dynamic_container_traits {
    using lookup_definition_type = lookups<associative<std::size_t, processor>>;
  };

  container<traits> container;
  container.template register_type<scope<shared>, storage<processor_impl>,
                                   interfaces<processor>>(
      dingo::key_value{std::size_t(0)});

  EXPECT_THROW(
      (container.template resolve<std::vector<processor *>>(std::size_t(0))),
      type_not_found_exception);
  ASSERT_EQ(container.template resolve<processor &>(std::size_t(0)).id(), 1);
}

TEST(index_test,
     implicit_typed_key_duplicate_storage_rejects_typed_key_one_row) {
  struct processor_key {};
  struct processor {
    virtual ~processor() = default;
    virtual int id() const = 0;
  };
  struct first_processor : processor {
    int id() const override { return 1; }
  };
  struct second_processor : processor {
    int id() const override { return 2; }
  };

  container<> container;
  container
      .template register_type<scope<shared>, storage<first_processor>,
                              interfaces<processor>, key_type<processor_key>>();

  ASSERT_EQ(
      container.template resolve<processor &>(key_type<processor_key>{}).id(),
      1);

  EXPECT_THROW((container.template register_type<
                   scope<shared>, storage<second_processor>,
                   interfaces<processor>, key_type<processor_key>>()),
               lookup_already_registered_exception);

  auto processors = container.template resolve<tracked_collection<processor *>>(
      key_type<processor_key>{});

  ASSERT_EQ(processors.reserve_count, 1U);
  ASSERT_EQ(processors.values.size(), 1U);
  ASSERT_EQ(processors.values[0]->id(), 1);
  ASSERT_EQ(
      container.template resolve<processor &>(key_type<processor_key>{}).id(),
      1);
}

TEST(index_test,
     failed_implicit_typed_key_registration_preserves_collection_rows) {
  struct processor_key {};

  typed_key_cached_processor_impl::constructions = 0;

  container<> container;
  container.template register_type<
      scope<shared>, storage<typed_key_cached_processor_impl>,
      interfaces<typed_key_cached_processor>, key_type<processor_key>>();

  auto &before = container.template resolve<typed_key_cached_processor &>(
      key_type<processor_key>{});

  EXPECT_THROW(
      (container.template register_type<
          scope<shared>, storage<typed_key_cached_processor_impl>,
          interfaces<typed_key_cached_processor>, key_type<processor_key>>()),
      lookup_already_registered_exception);

  auto &after = container.template resolve<typed_key_cached_processor &>(
      key_type<processor_key>{});
  ASSERT_EQ(&after, &before);

  auto processors =
      container
          .template resolve<tracked_collection<typed_key_cached_processor *>>(
              key_type<processor_key>{});
  ASSERT_EQ(processors.reserve_count, 1U);
  ASSERT_EQ(processors.values.size(), 1U);
  ASSERT_EQ(processors.values[0]->id(), 42);
  ASSERT_EQ(typed_key_cached_processor_impl::constructions, 1);
}

TEST(index_test, key_value_one_lookup_uses_equal_distinct_key_object) {
  struct comparable_key {
    int value;
    bool operator==(const comparable_key &other) const {
      return value == other.value;
    }
    bool operator<(const comparable_key &other) const {
      return value < other.value;
    }
  };
  struct traits : dynamic_container_traits {
    using lookup_definition_type =
        lookups<associative<comparable_key, typed_key_cached_processor>>;
  };

  typed_key_cached_processor_impl::constructions = 0;

  runtime_container<traits> container;
  comparable_key registration_key{7};
  container.template register_type<scope<shared>,
                                   storage<typed_key_cached_processor_impl>,
                                   interfaces<typed_key_cached_processor>>(
      dingo::key_value{registration_key});

  comparable_key first_lookup_key{7};
  comparable_key second_lookup_key{7};
  auto &first = container.template resolve<typed_key_cached_processor &>(
      first_lookup_key);
  auto &second = container.template resolve<typed_key_cached_processor &>(
      second_lookup_key);

  ASSERT_EQ(&second, &first);
  ASSERT_EQ(first.id(), 42);
  ASSERT_EQ(typed_key_cached_processor_impl::constructions, 1);
}

TEST(index_test, key_value_many_lookup_uses_value_semantics) {
  struct comparable_key {
    int value;
    bool operator==(const comparable_key &other) const {
      return value == other.value;
    }
    bool operator<(const comparable_key &other) const {
      return value < other.value;
    }
  };
  struct processor {
    virtual ~processor() = default;
    virtual int id() const = 0;
  };
  struct first_processor : processor {
    int id() const override { return 1; }
  };
  struct second_processor : processor {
    int id() const override { return 2; }
  };
  struct third_processor : processor {
    int id() const override { return 3; }
  };
  struct traits : dynamic_container_traits {
    using lookup_definition_type =
        lookups<associative<comparable_key, processor, many>>;
  };

  runtime_container<traits> container;

  container.template register_type<scope<shared>, storage<first_processor>,
                                   interfaces<processor>>(
      dingo::key_value{comparable_key{7}});
  container.template register_type<scope<shared>, storage<second_processor>,
                                   interfaces<processor>>(
      dingo::key_value{comparable_key{7}});
  container.template register_type<scope<shared>, storage<third_processor>,
                                   interfaces<processor>>(
      dingo::key_value{comparable_key{8}});

  auto values =
      container.template resolve<std::vector<processor *>>(comparable_key{7});
  ASSERT_EQ(values.size(), 2);
  ASSERT_EQ(values[0]->id(), 1);
  ASSERT_EQ(values[1]->id(), 2);
  EXPECT_THROW((container.template resolve<processor &>(comparable_key{7})),
               type_ambiguous_exception);
  ASSERT_EQ(container.template resolve<processor &>(comparable_key{8}).id(), 3);
}

TEST(index_test, associative_alias_behaves_like_key_value_one) {
  struct processor {
    virtual ~processor() = default;
    virtual int id() const = 0;
  };
  struct first_processor : processor {
    int id() const override { return 1; }
  };
  struct second_processor : processor {
    int id() const override { return 2; }
  };

  static_assert(
      std::is_same_v<associative<std::size_t, processor>,
                     detail::lookup_definition<
                         processor, detail::lookup_key<key_value<std::size_t>>,
                         one, ordered>>);

  struct traits : dynamic_container_traits {
    using lookup_definition_type = lookups<associative<std::size_t, processor>>;
  };

  container<traits> container;
  container.template register_type<scope<shared>, storage<first_processor>,
                                   interfaces<processor>>(
      dingo::key_value{std::size_t(7)});

  ASSERT_EQ(container.template resolve<processor &>(std::size_t(7)).id(), 1);
  EXPECT_THROW(
      (container.template register_type<
          scope<shared>, storage<second_processor>, interfaces<processor>>(
          dingo::key_value{std::size_t(7)})),
      lookup_already_registered_exception);
  EXPECT_THROW(
      (container.template resolve<std::vector<processor *>>(std::size_t(7))),
      type_not_found_exception);
}

TEST(index_test, associative_many_alias_behaves_like_key_value_many) {
  struct processor {
    virtual ~processor() = default;
    virtual int id() const = 0;
  };
  struct first_processor : processor {
    int id() const override { return 1; }
  };
  struct second_processor : processor {
    int id() const override { return 2; }
  };
  struct third_processor : processor {
    int id() const override { return 3; }
  };

  static_assert(
      std::is_same_v<associative<std::size_t, processor, many>,
                     detail::lookup_definition<
                         processor, detail::lookup_key<key_value<std::size_t>>,
                         many, ordered>>);

  struct traits : dynamic_container_traits {
    using lookup_definition_type =
        lookups<associative<std::size_t, processor, many>>;
  };

  container<traits> container;
  container.template register_type<scope<shared>, storage<first_processor>,
                                   interfaces<processor>>(
      dingo::key_value{std::size_t(7)});
  container.template register_type<scope<shared>, storage<second_processor>,
                                   interfaces<processor>>(
      dingo::key_value{std::size_t(7)});
  container.template register_type<scope<shared>, storage<third_processor>,
                                   interfaces<processor>>(
      dingo::key_value{std::size_t(8)});

  auto values =
      container.template resolve<std::vector<processor *>>(std::size_t(7));

  ASSERT_EQ(values.size(), 2);
  ASSERT_EQ(values[0]->id(), 1);
  ASSERT_EQ(values[1]->id(), 2);
  EXPECT_THROW((container.template resolve<processor &>(std::size_t(7))),
               type_ambiguous_exception);
  ASSERT_EQ(container.template resolve<processor &>(std::size_t(8)).id(), 3);
}

} // namespace dingo
