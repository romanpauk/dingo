//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#include "index_common.h"

namespace dingo {

TEST(index_test, exception_message_index_out_of_range_negative) {
  auto e = detail::make_type_index_out_of_range_exception(-1, std::size_t(2));

  std::string expected = "type index out of range: key type ";
  expected += type_name<int>();
  ASSERT_STREQ(e.what(), expected.c_str());
}

TEST(index_test, explicit_lookup_resolves_with_custom_container_allocator) {
  struct animal {
    virtual ~animal() = default;
    virtual int id() const = 0;
  };
  struct dog : animal {
    int id() const override { return 11; }
  };

  struct traits : dynamic_container_traits {
    using lookup_definition_type = lookups<associative<std::string, animal>>;
  };

  test_allocator_state state;
  test_allocator<char> allocator(&state);
  container<traits, test_allocator<char>> container(allocator);

  container
      .template register_type<scope<shared>, storage<dog>, interfaces<animal>>(
          dingo::key_value{std::string("dog")});

  ASSERT_EQ(container.template resolve<animal &>(std::string("dog")).id(), 11);
}

TEST(index_test, key_value_lookup_backend_uses_rebound_allocator) {
  struct small_key {
    explicit small_key(int resolved_value) : value(resolved_value) {}

    bool operator==(const small_key &other) const {
      return value == other.value;
    }
    bool operator<(const small_key &other) const { return value < other.value; }

    int value;
  };
  struct allocator_visible_key {
    explicit allocator_visible_key(int resolved_value)
        : value(resolved_value) {}

    bool operator==(const allocator_visible_key &other) const {
      return value == other.value;
    }
    bool operator<(const allocator_visible_key &other) const {
      return value < other.value;
    }

    int value;
    char padding[4096] = {};
  };
  struct animal {
    virtual ~animal() = default;
    virtual int id() const = 0;
  };
  struct dog : animal {
    int id() const override { return 11; }
  };
  struct small_key_traits : dynamic_container_traits {
    using lookup_definition_type = lookups<associative<small_key, animal>>;
  };

  test_allocator_state small_state;
  {
    test_allocator<char> allocator(&small_state);
    container<small_key_traits, test_allocator<char>> container(allocator);

    container.template register_type<scope<shared>, storage<dog>,
                                     interfaces<animal>>(
        dingo::key_value{small_key{7}});

    ASSERT_GT(small_state.allocations, 0);
    ASSERT_EQ(container.template resolve<animal &>(small_key{7}).id(), 11);
  }

  ASSERT_EQ(small_state.deallocations, small_state.allocations);

  struct large_key_traits : dynamic_container_traits {
    using lookup_definition_type =
        lookups<associative<allocator_visible_key, animal>>;
  };

  test_allocator_state state;
  {
    test_allocator<char> allocator(&state);
    container<large_key_traits, test_allocator<char>> container(allocator);

    container.template register_type<scope<shared>, storage<dog>,
                                     interfaces<animal>>(
        dingo::key_value{allocator_visible_key{7}});

    ASSERT_GT(state.allocations, 0);
    ASSERT_GE(state.largest_allocation_bytes, sizeof(allocator_visible_key));
    ASSERT_EQ(
        container.template resolve<animal &>(allocator_visible_key{7}).id(),
        11);
  }

  ASSERT_EQ(state.deallocations, state.allocations);
}

TEST(index_test, indexed_container_releases_rebound_allocator_storage) {
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

  struct traits : dynamic_container_traits {
    using lookup_definition_type =
        lookups<associative<std::size_t, processor, many>>;
  };

  test_allocator_state state;
  {
    test_allocator<char> allocator(&state);
    container<traits, test_allocator<char>> container(allocator);
    container.template register_type<scope<shared>, storage<first_processor>,
                                     interfaces<processor>>(
        dingo::key_value{std::size_t(0)});
    container.template register_type<scope<shared>, storage<second_processor>,
                                     interfaces<processor>>(
        dingo::key_value{std::size_t(0)});

    auto values =
        container.template resolve<std::vector<processor *>>(std::size_t(0));
    ASSERT_EQ(values.size(), 2);
    ASSERT_GT(state.allocations, 0);
  }

  ASSERT_EQ(state.deallocations, state.allocations);
}

TEST(index_test, associative_many_allows_multiple_values_per_key_value) {
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
  ASSERT_EQ(sorted_processor_ids(values), (std::vector<int>{1, 2}));
  EXPECT_THROW((container.template resolve<processor &>(std::size_t(7))),
               type_ambiguous_exception);
  ASSERT_EQ(container.template resolve<processor &>(std::size_t(8)).id(), 3);
  EXPECT_THROW((container.template resolve<processor &>(std::size_t(9))),
               type_not_found_exception);
}

} // namespace dingo
