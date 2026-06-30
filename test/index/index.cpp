//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#include <dingo/container.h>
#include <dingo/storage/shared.h>
#include <dingo/storage/unique.h>
#include <dingo/type/type_name.h>

#include <gtest/gtest.h>

#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace dingo {

struct test_allocator_state {
  int allocations = 0;
  int deallocations = 0;
  std::size_t largest_allocation_bytes = 0;
};

template <typename T> class test_allocator {
public:
  using value_type = T;

  explicit test_allocator(test_allocator_state *state = nullptr)
      : state_(state) {}

  template <typename U>
  test_allocator(const test_allocator<U> &other) noexcept
      : state_(other.state()) {}

  value_type *allocate(std::size_t n) {
    if (state_) {
      ++state_->allocations;
      const auto allocation_bytes = n * sizeof(value_type);
      if (allocation_bytes > state_->largest_allocation_bytes) {
        state_->largest_allocation_bytes = allocation_bytes;
      }
    }
    return static_cast<value_type *>(::operator new(n * sizeof(value_type)));
  }

  void deallocate(value_type *p, std::size_t) noexcept {
    if (state_) {
      ++state_->deallocations;
    }
    ::operator delete(p);
  }

  test_allocator_state *state() const { return state_; }

private:
  test_allocator_state *state_;
};

template <typename T, typename U>
bool operator==(const test_allocator<T> &lhs,
                const test_allocator<U> &rhs) noexcept {
  return lhs.state() == rhs.state();
}

template <typename T, typename U>
bool operator!=(const test_allocator<T> &lhs,
                const test_allocator<U> &rhs) noexcept {
  return !(lhs == rhs);
}

struct typed_key_cached_processor {
  virtual ~typed_key_cached_processor() = default;
  virtual int id() const = 0;
};

struct typed_key_cached_processor_impl : typed_key_cached_processor {
  typed_key_cached_processor_impl() { ++constructions; }

  int id() const override { return 42; }

  inline static int constructions = 0;
};

template <typename Container>
using selector_identity_probe =
    detail::runtime_selector_identity_probe<typename Container::registry_type>;

struct selector_table_test_entry;
using selector_table_test_rtti = rtti<typeid_provider>;
using selector_table_test_allocator = std::allocator<char>;
using selector_table_test_table =
    detail::runtime_selector_table<selector_table_test_rtti,
                                   selector_table_test_allocator,
                                   selector_table_test_entry>;

struct selector_table_test_entry {
  explicit selector_table_test_entry(selector_table_test_allocator &allocator)
      : storage_type(selector_table_test_rtti::template get_type_index<
                     selector_table_test_entry>()),
        selector_identities(
            detail::make_selector_storage_allocator<
                selector_table_test_table::identity_allocator>(allocator)) {}

  typename selector_table_test_rtti::type_index storage_type;
  selector_table_test_table::identity_storage selector_identities;
};

struct fixed_injection_processor {
  virtual ~fixed_injection_processor() = default;
  virtual int id() const = 0;
};

template <int Id>
struct fixed_injection_processor_impl : fixed_injection_processor {
  int id() const override { return Id; }
};

template <auto Id> struct fixed_injection_consumer {
  explicit fixed_injection_consumer(
      indexed<fixed_injection_processor &, key<std::size_t, Id>> selected)
      : processor(selected) {}

  fixed_injection_processor &processor;
};

enum class fixed_injection_processor_id { first, second };

template <fixed_injection_processor_id Id> struct fixed_enum_consumer {
  explicit fixed_enum_consumer(indexed<fixed_injection_processor &,
                                       key<fixed_injection_processor_id, Id>>
                                   selected)
      : processor(selected) {}

  fixed_injection_processor &processor;
};

struct registered_fixed_indexed_consumer {
  explicit registered_fixed_indexed_consumer(
      indexed<fixed_injection_processor &, key<std::size_t, 1>> selected)
      : processor(selected) {}

  fixed_injection_processor &processor;
};

struct selected_type_processor_tag {};

struct selected_type_consumer {
  explicit selected_type_consumer(
      detail::selected<fixed_injection_processor &,
                       detail::type_selector<selected_type_processor_tag>>
          selected_processor)
      : processor(selected_processor) {}

  fixed_injection_processor &processor;
};

struct selected_value_consumer {
  explicit selected_value_consumer(
      detail::selected<fixed_injection_processor &,
                       detail::value_selector<std::size_t, 1>>
          selected_processor)
      : processor(selected_processor) {}

  fixed_injection_processor &processor;
};

struct annotated_index_primary_tag {};
struct annotated_index_replica_tag {};

#if __cplusplus >= 202002L
template <std::size_t N> struct external_fixed_string {
  char value[N];

  constexpr external_fixed_string(const char (&text)[N]) {
    for (std::size_t i = 0; i != N; ++i) {
      value[i] = text[i];
    }
  }

  constexpr bool operator==(const external_fixed_string &) const = default;
};

template <typename T, external_fixed_string Value>
struct external_key_string {};

namespace detail {
template <typename T, ::dingo::external_fixed_string Value>
struct is_key_value<::dingo::external_key_string<T, Value>> : std::true_type {};

template <typename T, ::dingo::external_fixed_string Value>
struct key_selector_value<::dingo::external_key_string<T, Value>> {
  using type = T;

  static T make() {
    constexpr auto size = sizeof(Value.value) - 1;
    return T{std::string_view{Value.value, size}};
  }
};
} // namespace detail
#endif

#ifdef DINGO_ENABLE_FUTURE_KEY_STRING_TESTS
// Future API reference: key_string<std::string, "json"> should construct a
// std::string key and select the (Interface, std::string) index. It must not
// require transparent lookup or a std::string_view index.
struct string_processor {
  virtual ~string_processor() = default;
  virtual int id() const = 0;
};

template <int Id> struct string_processor_impl : string_processor {
  int id() const override { return Id; }
};

struct string_literal_consumer {
  explicit string_literal_consumer(
      dingo::indexed<string_processor &, dingo::key_string<std::string, "json">>
          selected)
      : processor(selected) {}

  string_processor &processor;
};
#endif

TEST(index_test, same_storage_at_different_runtime_keys_is_distinct) {
  struct processor {
    virtual ~processor() = default;
    virtual int id() const = 0;
  };
  struct processor_impl : processor {
    int id() const override { return id_value; }

    int id_value = 0;
  };

  struct traits : dynamic_container_traits {
    using index_definition_type =
        selectors<associative<processor, std::size_t>>;
  };

  container<traits> container;
  container.template register_indexed_type<
      scope<shared>, storage<std::shared_ptr<processor_impl>>,
      interfaces<processor>>(std::size_t(0));
  container.template register_indexed_type<
      scope<shared>, storage<std::shared_ptr<processor_impl>>,
      interfaces<processor>>(std::size_t(1));

  auto &first = container.template resolve<processor &>(std::size_t(0));
  auto &second = container.template resolve<processor &>(std::size_t(1));

  ASSERT_NE(&first, &second);
  ASSERT_EQ(&first, container.template resolve<processor *>(std::size_t(0)));
  ASSERT_EQ(&second, container.template resolve<processor *>(std::size_t(1)));
}

TEST(index_test,
     same_runtime_key_rejects_different_storage_for_associative_one) {
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
    using index_definition_type =
        selectors<associative<processor, std::size_t>>;
  };

  container<traits> container;
  container.template register_indexed_type<
      scope<shared>, storage<first_processor>, interfaces<processor>>(
      std::size_t(0));

  EXPECT_THROW(
      (container.template register_indexed_type<
          scope<shared>, storage<second_processor>, interfaces<processor>>(
          std::size_t(0))),
      type_index_already_registered_exception);
}

TEST(index_test, same_runtime_key_rejects_same_storage_for_associative_one) {
  struct processor {
    virtual ~processor() = default;
    virtual int id() const = 0;
  };
  struct processor_impl : processor {
    int id() const override { return 1; }
  };

  struct traits : dynamic_container_traits {
    using index_definition_type =
        selectors<associative<processor, std::size_t>>;
  };

  container<traits> container;
  container.template register_indexed_type<
      scope<shared>, storage<processor_impl>, interfaces<processor>>(
      std::size_t(0));

  EXPECT_THROW(
      (container.template register_indexed_type<
          scope<shared>, storage<processor_impl>, interfaces<processor>>(
          std::size_t(0))),
      type_index_already_registered_exception);
}

TEST(index_test,
     failed_runtime_key_one_registration_preserves_existing_entry_cache) {
  struct second_processor : typed_key_cached_processor {
    int id() const override { return 2; }
  };

  struct traits : dynamic_container_traits {
    using index_definition_type =
        selectors<associative<typed_key_cached_processor, std::size_t>>;
  };

  typed_key_cached_processor_impl::constructions = 0;

  container<traits> container;
  container.template register_indexed_type<
      scope<shared>, storage<typed_key_cached_processor_impl>,
      interfaces<typed_key_cached_processor>>(std::size_t(7));

  auto &before =
      container.template resolve<typed_key_cached_processor &>(std::size_t(7));
  EXPECT_THROW((container.template register_indexed_type<
                   scope<shared>, storage<second_processor>,
                   interfaces<typed_key_cached_processor>>(std::size_t(7))),
               type_index_already_registered_exception);
  auto &after =
      container.template resolve<typed_key_cached_processor &>(std::size_t(7));

  ASSERT_EQ(&after, &before);
  ASSERT_EQ(after.id(), 42);
  ASSERT_EQ(typed_key_cached_processor_impl::constructions, 1);
}

TEST(index_test, unkeyed_resolve_does_not_select_indexed_binding) {
  struct processor {
    virtual ~processor() = default;
    virtual int id() const = 0;
  };
  struct processor_impl : processor {
    int id() const override { return 1; }
  };

  struct traits : dynamic_container_traits {
    using index_definition_type =
        selectors<associative<processor, std::size_t>>;
  };

  container<traits> container;
  container.template register_indexed_type<
      scope<shared>, storage<processor_impl>, interfaces<processor>>(
      std::size_t(0));

  EXPECT_THROW((container.template resolve<processor &>()),
               type_not_found_exception);
  ASSERT_EQ(container.template resolve<processor &>(std::size_t(0)).id(), 1);
}

TEST(index_test, normal_unkeyed_shared_registration_reuses_entry_cache) {
  typed_key_cached_processor_impl::constructions = 0;

  container<> container;
  container.template register_type<scope<shared>,
                                   storage<typed_key_cached_processor_impl>,
                                   interfaces<typed_key_cached_processor>>();

  auto &first = container.template resolve<typed_key_cached_processor &>();
  auto &second = container.template resolve<typed_key_cached_processor &>();

  ASSERT_EQ(&second, &first);
  ASSERT_EQ(first.id(), 42);
  ASSERT_EQ(typed_key_cached_processor_impl::constructions, 1);
}

TEST(index_test, runtime_keyed_collection_requires_associative_many_selector) {
  struct processor {
    virtual ~processor() = default;
    virtual int id() const = 0;
  };
  struct processor_impl : processor {
    int id() const override { return 1; }
  };

  struct traits : dynamic_container_traits {
    using index_definition_type =
        selectors<associative<processor, std::size_t>>;
  };

  container<traits> container;
  container.template register_indexed_type<
      scope<shared>, storage<processor_impl>, interfaces<processor>>(
      std::size_t(0));

  EXPECT_THROW(
      (container.template resolve<std::vector<processor *>>(std::size_t(0))),
      type_not_found_exception);
  ASSERT_EQ(container.template resolve<processor &>(std::size_t(0)).id(), 1);
}

TEST(index_test, legacy_typed_key_registration_reuses_entry_cache) {
  struct processor_key {};

  typed_key_cached_processor_impl::constructions = 0;

  container<> container;
  container.template register_type<
      scope<shared>, storage<typed_key_cached_processor_impl>,
      interfaces<typed_key_cached_processor>, key<processor_key>>();

  auto &first = container.template resolve<typed_key_cached_processor &>(
      key<processor_key>{});
  auto &second = container.template resolve<typed_key_cached_processor &>(
      key<processor_key>{});

  ASSERT_EQ(&second, &first);
  ASSERT_EQ(first.id(), 42);
  ASSERT_EQ(typed_key_cached_processor_impl::constructions, 1);
}

TEST(index_test, legacy_normal_unkeyed_registration_records_no_key_one) {
  runtime_container<> container;
  container.template register_type<scope<shared>,
                                   storage<typed_key_cached_processor_impl>,
                                   interfaces<typed_key_cached_processor>>();

  using probe = selector_identity_probe<decltype(container)>;
  ASSERT_TRUE((probe::template selected_no_key_identity_matches<
               typed_key_cached_processor, typed_key_cached_processor, one>(
      container.registry())));
}

TEST(index_test, explicit_collection_registration_records_no_key_many) {
  struct traits : dynamic_container_traits {
    using index_definition_type =
        selectors<collection<typed_key_cached_processor>>;
  };

  runtime_container<traits> container;
  container.template register_type<scope<shared>,
                                   storage<typed_key_cached_processor_impl>,
                                   interfaces<typed_key_cached_processor>>();

  using probe = selector_identity_probe<decltype(container)>;
  ASSERT_TRUE((probe::template selected_no_key_identity_matches<
               typed_key_cached_processor, typed_key_cached_processor, many>(
      container.registry())));
}

TEST(index_test, legacy_typed_key_registration_records_typed_key_one) {
  struct processor_key {};

  runtime_container<> container;
  container.template register_type<
      scope<shared>, storage<typed_key_cached_processor_impl>,
      interfaces<typed_key_cached_processor>, key<processor_key>>();

  using probe = selector_identity_probe<decltype(container)>;
  ASSERT_TRUE(
      (probe::template selected_typed_key_identity_matches<
          typed_key_cached_processor, typed_key_cached_processor, processor_key,
          one>(container.registry(), key<processor_key>{})));
}

TEST(index_test, explicit_typed_key_many_registration_records_typed_key_many) {
  struct processor_key {};
  struct traits : dynamic_container_traits {
    using index_definition_type = selectors<
        selector<typed_key_cached_processor, typed_key<processor_key>, many>>;
  };

  runtime_container<traits> container;
  container.template register_type<
      scope<shared>, storage<typed_key_cached_processor_impl>,
      interfaces<typed_key_cached_processor>, key<processor_key>>();

  using probe = selector_identity_probe<decltype(container)>;
  ASSERT_TRUE(
      (probe::template selected_typed_key_identity_matches<
          typed_key_cached_processor, typed_key_cached_processor, processor_key,
          many>(container.registry(), key<processor_key>{})));
}

TEST(index_test, associative_one_registration_records_runtime_key_one) {
  struct traits : dynamic_container_traits {
    using index_definition_type =
        selectors<associative<typed_key_cached_processor, std::size_t>>;
  };

  runtime_container<traits> container;
  container.template register_indexed_type<
      scope<shared>, storage<typed_key_cached_processor_impl>,
      interfaces<typed_key_cached_processor>>(std::size_t(7));

  using probe = selector_identity_probe<decltype(container)>;
  ASSERT_TRUE((probe::template selected_runtime_key_identity_matches<
               typed_key_cached_processor, typed_key_cached_processor,
               std::size_t, one>(container.registry(), std::size_t(7))));
}

TEST(index_test, runtime_key_one_lookup_uses_equal_distinct_key_object) {
  struct comparable_key {
    int value;
    bool operator==(const comparable_key &other) const {
      return value == other.value;
    }
  };
  struct traits : dynamic_container_traits {
    using index_definition_type =
        selectors<associative<typed_key_cached_processor, comparable_key>>;
  };

  typed_key_cached_processor_impl::constructions = 0;

  runtime_container<traits> container;
  comparable_key registration_key{7};
  container.template register_indexed_type<
      scope<shared>, storage<typed_key_cached_processor_impl>,
      interfaces<typed_key_cached_processor>>(registration_key);

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

TEST(index_test, associative_many_registration_records_runtime_key_many) {
  struct traits : dynamic_container_traits {
    using index_definition_type =
        selectors<associative<typed_key_cached_processor, std::size_t, many>>;
  };

  runtime_container<traits> container;
  container.template register_indexed_type<
      scope<shared>, storage<typed_key_cached_processor_impl>,
      interfaces<typed_key_cached_processor>>(std::size_t(7));

  using probe = selector_identity_probe<decltype(container)>;
  ASSERT_TRUE((probe::template selected_runtime_key_identity_matches<
               typed_key_cached_processor, typed_key_cached_processor,
               std::size_t, many>(container.registry(), std::size_t(7))));
}

TEST(index_test, selector_table_counts_multi_identity_entry_once) {
  struct processor {};
  struct processor_key {};

  selector_table_test_allocator allocator;
  selector_table_test_table table(allocator);
  selector_table_test_entry entry(allocator);
  entry.selector_identities.emplace_back(
      selector_table_test_table::template make_no_key_identity<processor,
                                                               many>());
  entry.selector_identities.emplace_back(
      selector_table_test_table::template make_typed_key_identity<
          processor, processor_key, many>());

  ASSERT_TRUE(table.insert(entry));

  auto matches_processor = [](const auto &identity) {
    return selector_table_test_table::template matches_no_key<processor, many>(
               identity) ||
           selector_table_test_table::template matches_typed_key<
               processor, processor_key, many>(identity);
  };
  bool ambiguous = false;
  auto *selected = table.find_singular(matches_processor, ambiguous);
  ASSERT_FALSE(ambiguous);
  ASSERT_EQ(selected, &entry);
  ASSERT_EQ(table.count(matches_processor), 1U);

  std::size_t visits = 0;
  ASSERT_EQ(table.for_each(matches_processor,
                           [&](auto &selected_entry) {
                             ++visits;
                             ASSERT_EQ(&selected_entry, &entry);
                           }),
            1U);
  ASSERT_EQ(visits, 1U);

  table.erase(entry);
  ASSERT_EQ(table.count(matches_processor), 0U);
}

TEST(index_test, no_key_many_selector_covers_empty_exact_ambiguous) {
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
    using index_definition_type = selectors<collection<processor>>;
  };

  runtime_container<traits> empty_container;

  auto empty_processors =
      empty_container.template resolve<std::vector<processor *>>();
  ASSERT_TRUE(empty_processors.empty());
  EXPECT_THROW((empty_container.template resolve<processor &>()),
               type_not_found_exception);

  runtime_container<traits> exact_container;
  exact_container.template register_type<
      scope<shared>, storage<first_processor>, interfaces<processor>>();
  auto exact_processors =
      exact_container.template resolve<std::vector<processor *>>();
  ASSERT_EQ(exact_processors.size(), 1U);
  ASSERT_EQ(exact_processors[0]->id(), 1);
  ASSERT_EQ(exact_container.template resolve<processor &>().id(), 1);

  runtime_container<traits> ambiguous_container;
  ambiguous_container.template register_type<
      scope<shared>, storage<first_processor>, interfaces<processor>>();
  ambiguous_container.template register_type<
      scope<shared>, storage<second_processor>, interfaces<processor>>();
  auto ambiguous_processors =
      ambiguous_container.template resolve<std::vector<processor *>>();
  ASSERT_EQ(ambiguous_processors.size(), 2U);
  ASSERT_EQ(ambiguous_processors[0]->id(), 1);
  ASSERT_EQ(ambiguous_processors[1]->id(), 2);
  EXPECT_THROW((ambiguous_container.template resolve<processor &>()),
               type_ambiguous_exception);
}

TEST(index_test, typed_key_many_selector_covers_collection_order) {
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
  struct traits : dynamic_container_traits {
    using index_definition_type =
        selectors<selector<processor, typed_key<processor_key>, many>>;
  };

  runtime_container<traits> container;

  auto empty_processors = container.template resolve<std::vector<processor *>>(
      key<processor_key>{});
  ASSERT_TRUE(empty_processors.empty());
  EXPECT_THROW((container.template resolve<processor &>(key<processor_key>{})),
               type_not_found_exception);

  container.template register_type<scope<shared>, storage<first_processor>,
                                   interfaces<processor>, key<processor_key>>();
  container.template register_type<scope<shared>, storage<second_processor>,
                                   interfaces<processor>, key<processor_key>>();

  auto processors = container.template resolve<std::vector<processor *>>(
      key<processor_key>{});
  ASSERT_EQ(processors.size(), 2U);
  ASSERT_EQ(processors[0]->id(), 1);
  ASSERT_EQ(processors[1]->id(), 2);
  EXPECT_THROW((container.template resolve<processor &>(key<processor_key>{})),
               type_ambiguous_exception);
}

TEST(index_test, runtime_key_many_selector_uses_value_semantics) {
  struct comparable_key {
    int value;
    bool operator==(const comparable_key &other) const {
      return value == other.value;
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
    using index_definition_type =
        selectors<associative<processor, comparable_key, many>>;
  };

  runtime_container<traits> container;

  container.template register_indexed_type<
      scope<shared>, storage<first_processor>, interfaces<processor>>(
      comparable_key{7});
  container.template register_indexed_type<
      scope<shared>, storage<second_processor>, interfaces<processor>>(
      comparable_key{7});
  container.template register_indexed_type<
      scope<shared>, storage<third_processor>, interfaces<processor>>(
      comparable_key{8});

  auto values =
      container.template resolve<std::vector<processor *>>(comparable_key{7});
  ASSERT_EQ(values.size(), 2);
  ASSERT_EQ(values[0]->id(), 1);
  ASSERT_EQ(values[1]->id(), 2);
  EXPECT_THROW((container.template resolve<processor &>(comparable_key{7})),
               type_ambiguous_exception);
  ASSERT_EQ(container.template resolve<processor &>(comparable_key{8}).id(), 3);
}

TEST(index_test, runtime_key_many_selector_resolves_empty_collection) {
  struct comparable_key {
    int value;
    bool operator==(const comparable_key &other) const {
      return value == other.value;
    }
  };
  struct processor {
    virtual ~processor() = default;
    virtual int id() const = 0;
  };
  struct traits : dynamic_container_traits {
    using index_definition_type =
        selectors<associative<processor, comparable_key, many>>;
  };

  runtime_container<traits> container;

  auto processors =
      container.template resolve<std::vector<processor *>>(comparable_key{7});
  ASSERT_TRUE(processors.empty());
  EXPECT_THROW((container.template resolve<processor &>(comparable_key{7})),
               type_not_found_exception);
}

TEST(index_test, associative_alias_behaves_like_runtime_key_one) {
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
      std::is_same_v<associative<processor, std::size_t>,
                     selector<processor, runtime_key<std::size_t>, one>>);
  static_assert(std::is_same_v<unique_index<processor, std::size_t>,
                               associative<processor, std::size_t>>);

  struct traits : dynamic_container_traits {
    using index_definition_type =
        selectors<associative<processor, std::size_t>>;
  };

  container<traits> container;
  container.template register_indexed_type<
      scope<shared>, storage<first_processor>, interfaces<processor>>(
      std::size_t(7));

  ASSERT_EQ(container.template resolve<processor &>(std::size_t(7)).id(), 1);
  EXPECT_THROW(
      (container.template register_indexed_type<
          scope<shared>, storage<second_processor>, interfaces<processor>>(
          std::size_t(7))),
      type_index_already_registered_exception);
  EXPECT_THROW(
      (container.template resolve<std::vector<processor *>>(std::size_t(7))),
      type_not_found_exception);
}

TEST(index_test, associative_many_alias_behaves_like_runtime_key_many) {
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
      std::is_same_v<associative<processor, std::size_t, many>,
                     selector<processor, runtime_key<std::size_t>, many>>);
  static_assert(std::is_same_v<multi_index<processor, std::size_t>,
                               associative<processor, std::size_t, many>>);

  struct traits : dynamic_container_traits {
    using index_definition_type =
        selectors<associative<processor, std::size_t, many>>;
  };

  container<traits> container;
  container.template register_indexed_type<
      scope<shared>, storage<first_processor>, interfaces<processor>>(
      std::size_t(7));
  container.template register_indexed_type<
      scope<shared>, storage<second_processor>, interfaces<processor>>(
      std::size_t(7));
  container.template register_indexed_type<
      scope<shared>, storage<third_processor>, interfaces<processor>>(
      std::size_t(8));

  auto values =
      container.template resolve<std::vector<processor *>>(std::size_t(7));

  ASSERT_EQ(values.size(), 2);
  ASSERT_EQ(values[0]->id(), 1);
  ASSERT_EQ(values[1]->id(), 2);
  EXPECT_THROW((container.template resolve<processor &>(std::size_t(7))),
               type_ambiguous_exception);
  ASSERT_EQ(container.template resolve<processor &>(std::size_t(8)).id(), 3);
}

TEST(index_test,
     runtime_key_one_same_storage_at_different_keys_has_distinct_cache_state) {
  struct traits : dynamic_container_traits {
    using index_definition_type =
        selectors<associative<typed_key_cached_processor, std::size_t>>;
  };

  typed_key_cached_processor_impl::constructions = 0;

  container<traits> container;
  container.template register_indexed_type<
      scope<shared>, storage<typed_key_cached_processor_impl>,
      interfaces<typed_key_cached_processor>>(std::size_t(7));
  container.template register_indexed_type<
      scope<shared>, storage<typed_key_cached_processor_impl>,
      interfaces<typed_key_cached_processor>>(std::size_t(8));

  auto &first_key_first =
      container.template resolve<typed_key_cached_processor &>(std::size_t(7));
  auto &first_key_second =
      container.template resolve<typed_key_cached_processor &>(std::size_t(7));
  auto &second_key =
      container.template resolve<typed_key_cached_processor &>(std::size_t(8));

  ASSERT_EQ(&first_key_second, &first_key_first);
  ASSERT_NE(&second_key, &first_key_first);
  ASSERT_EQ(typed_key_cached_processor_impl::constructions, 2);
}

TEST(index_test, empty_child_runtime_key_one_selector_falls_back_to_parent) {
  struct processor {
    virtual ~processor() = default;
    virtual int id() const = 0;
  };
  struct parent_processor : processor {
    int id() const override { return 1; }
  };

  struct traits : dynamic_container_traits {
    using index_definition_type =
        selectors<associative<processor, std::size_t>>;
  };

  container<traits> parent;
  container<traits, std::allocator<char>, decltype(parent)> child(&parent);
  parent.template register_indexed_type<
      scope<shared>, storage<parent_processor>, interfaces<processor>>(
      std::size_t(7));

  ASSERT_EQ(child.template resolve<processor &>(std::size_t(7)).id(), 1);
}

TEST(index_test, child_runtime_key_one_selector_shadows_parent) {
  struct processor {
    virtual ~processor() = default;
    virtual int id() const = 0;
  };
  struct parent_processor : processor {
    int id() const override { return 1; }
  };
  struct child_processor : processor {
    int id() const override { return 2; }
  };

  struct traits : dynamic_container_traits {
    using index_definition_type =
        selectors<associative<processor, std::size_t>>;
  };

  container<traits> parent;
  container<traits, std::allocator<char>, decltype(parent)> child(&parent);
  parent.template register_indexed_type<
      scope<shared>, storage<parent_processor>, interfaces<processor>>(
      std::size_t(7));
  child.template register_indexed_type<scope<shared>, storage<child_processor>,
                                       interfaces<processor>>(std::size_t(7));

  ASSERT_EQ(child.template resolve<processor &>(std::size_t(7)).id(), 2);
}

TEST(index_test,
     empty_child_runtime_key_many_selector_collection_falls_back_to_parent) {
  struct processor {
    virtual ~processor() = default;
    virtual int id() const = 0;
  };
  struct first_parent_processor : processor {
    int id() const override { return 1; }
  };
  struct second_parent_processor : processor {
    int id() const override { return 2; }
  };

  struct traits : dynamic_container_traits {
    using index_definition_type =
        selectors<associative<processor, std::size_t, many>>;
  };

  container<traits> parent;
  container<traits, std::allocator<char>, decltype(parent)> child(&parent);
  parent.template register_indexed_type<
      scope<shared>, storage<first_parent_processor>, interfaces<processor>>(
      std::size_t(7));
  parent.template register_indexed_type<
      scope<shared>, storage<second_parent_processor>, interfaces<processor>>(
      std::size_t(7));

  auto values =
      child.template resolve<std::vector<processor *>>(std::size_t(7));

  ASSERT_EQ(values.size(), 2);
  ASSERT_EQ(values[0]->id(), 1);
  ASSERT_EQ(values[1]->id(), 2);
  EXPECT_THROW((child.template resolve<processor &>(std::size_t(7))),
               type_ambiguous_exception);
}

TEST(index_test,
     child_runtime_key_many_selector_collection_does_not_merge_parent) {
  struct processor {
    virtual ~processor() = default;
    virtual int id() const = 0;
  };
  struct parent_processor : processor {
    int id() const override { return 1; }
  };
  struct child_processor : processor {
    int id() const override { return 2; }
  };

  struct traits : dynamic_container_traits {
    using index_definition_type =
        selectors<associative<processor, std::size_t, many>>;
  };

  container<traits> parent;
  container<traits, std::allocator<char>, decltype(parent)> child(&parent);
  parent.template register_indexed_type<
      scope<shared>, storage<parent_processor>, interfaces<processor>>(
      std::size_t(7));
  child.template register_indexed_type<scope<shared>, storage<child_processor>,
                                       interfaces<processor>>(std::size_t(7));

  auto values =
      child.template resolve<std::vector<processor *>>(std::size_t(7));

  ASSERT_EQ(values.size(), 1);
  ASSERT_EQ(values[0]->id(), 2);
  ASSERT_EQ(child.template resolve<processor &>(std::size_t(7)).id(), 2);
}

TEST(index_test, child_runtime_key_many_selector_ambiguity_does_not_fallback) {
  struct processor {
    virtual ~processor() = default;
    virtual int id() const = 0;
  };
  struct parent_processor : processor {
    int id() const override { return 1; }
  };
  struct first_child_processor : processor {
    int id() const override { return 2; }
  };
  struct second_child_processor : processor {
    int id() const override { return 3; }
  };

  struct traits : dynamic_container_traits {
    using index_definition_type =
        selectors<associative<processor, std::size_t, many>>;
  };

  container<traits> parent;
  container<traits, std::allocator<char>, decltype(parent)> child(&parent);
  parent.template register_indexed_type<
      scope<shared>, storage<parent_processor>, interfaces<processor>>(
      std::size_t(7));
  child.template register_indexed_type<
      scope<shared>, storage<first_child_processor>, interfaces<processor>>(
      std::size_t(7));
  child.template register_indexed_type<
      scope<shared>, storage<second_child_processor>, interfaces<processor>>(
      std::size_t(7));

  auto values =
      child.template resolve<std::vector<processor *>>(std::size_t(7));

  ASSERT_EQ(values.size(), 2);
  ASSERT_EQ(values[0]->id(), 2);
  ASSERT_EQ(values[1]->id(), 3);
  EXPECT_THROW((child.template resolve<processor &>(std::size_t(7))),
               type_ambiguous_exception);
}

TEST(index_test, no_key_one_replaces_unkeyed_singular_identity) {
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
    using index_definition_type = selectors<selector<processor, no_key, one>>;
  };

  container<traits> container;
  container.template register_type<scope<shared>, storage<first_processor>,
                                   interfaces<processor>>();

  EXPECT_THROW(
      (container.template register_type<
          scope<shared>, storage<second_processor>, interfaces<processor>>()),
      type_already_registered_exception);
  ASSERT_EQ(container.template resolve<processor &>().id(), 1);
}

TEST(index_test, no_key_one_rejects_same_storage_duplicate) {
  struct processor {
    virtual ~processor() = default;
    virtual int id() const = 0;
  };
  struct processor_impl : processor {
    int id() const override { return 1; }
  };

  struct traits : dynamic_container_traits {
    using index_definition_type = selectors<selector<processor, no_key, one>>;
  };

  container<traits> container;

  EXPECT_THROW((container.template resolve<processor &>()),
               type_not_found_exception);

  container.template register_type<scope<shared>, storage<processor_impl>,
                                   interfaces<processor>>();

  EXPECT_THROW(
      (container.template register_type<scope<shared>, storage<processor_impl>,
                                        interfaces<processor>>()),
      type_already_registered_exception);
}

TEST(index_test, no_key_one_does_not_satisfy_collection_lookup) {
  struct processor {
    virtual ~processor() = default;
    virtual int id() const = 0;
  };
  struct processor_impl : processor {
    int id() const override { return 1; }
  };

  struct traits : dynamic_container_traits {
    using index_definition_type = selectors<selector<processor, no_key, one>>;
  };

  container<traits> container;
  container.template register_type<scope<shared>, storage<processor_impl>,
                                   interfaces<processor>>();

  EXPECT_THROW((container.template resolve<std::vector<processor *>>()),
               type_not_found_exception);
  ASSERT_EQ(container.template resolve<processor &>().id(), 1);
}

TEST(index_test, no_key_many_enumerates_unkeyed_collection_in_order) {
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
    using index_definition_type = selectors<collection<processor>>;
  };

  container<traits> container;
  container.template register_type<scope<shared>, storage<first_processor>,
                                   interfaces<processor>>();
  container.template register_type<scope<shared>, storage<second_processor>,
                                   interfaces<processor>>();

  auto processors = container.template resolve<std::vector<processor *>>();

  ASSERT_EQ(processors.size(), 2U);
  ASSERT_EQ(processors[0]->id(), 1);
  ASSERT_EQ(processors[1]->id(), 2);
  EXPECT_THROW((container.template resolve<processor &>()),
               type_ambiguous_exception);
}

TEST(index_test, collection_alias_behaves_like_no_key_many_selector) {
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
      std::is_same_v<collection<processor>, selector<processor, no_key, many>>);

  struct traits : dynamic_container_traits {
    using index_definition_type = selectors<collection<processor>>;
  };

  container<traits> container;
  container.template register_type<scope<shared>, storage<first_processor>,
                                   interfaces<processor>>();
  container.template register_type<scope<shared>, storage<second_processor>,
                                   interfaces<processor>>();

  auto processors = container.template resolve<std::vector<processor *>>();

  ASSERT_EQ(processors.size(), 2U);
  ASSERT_EQ(processors[0]->id(), 1);
  ASSERT_EQ(processors[1]->id(), 2);
  EXPECT_THROW((container.template resolve<processor &>()),
               type_ambiguous_exception);
}

TEST(index_test, no_key_many_singular_resolve_requires_exactly_one_binding) {
  struct processor {
    virtual ~processor() = default;
    virtual int id() const = 0;
  };
  struct processor_impl : processor {
    int id() const override { return 1; }
  };

  struct traits : dynamic_container_traits {
    using index_definition_type = selectors<collection<processor>>;
  };

  container<traits> container;
  container.template register_type<scope<shared>, storage<processor_impl>,
                                   interfaces<processor>>();

  auto processors = container.template resolve<std::vector<processor *>>();

  ASSERT_EQ(processors.size(), 1U);
  ASSERT_EQ(processors[0], &container.template resolve<processor &>());
}

TEST(index_test, no_key_many_rejects_same_storage_duplicate) {
  struct processor {
    virtual ~processor() = default;
    virtual int id() const = 0;
  };
  struct processor_impl : processor {
    int id() const override { return 1; }
  };

  struct traits : dynamic_container_traits {
    using index_definition_type = selectors<collection<processor>>;
  };

  container<traits> container;
  container.template register_type<scope<shared>, storage<processor_impl>,
                                   interfaces<processor>>();

  EXPECT_THROW(
      (container.template register_type<scope<shared>, storage<processor_impl>,
                                        interfaces<processor>>()),
      type_already_registered_exception);
}

TEST(index_test,
     failed_no_key_many_registration_preserves_existing_selector_rows) {
  struct processor {
    virtual ~processor() = default;
    virtual int id() const = 0;
  };
  struct processor_impl : processor {
    int id() const override { return 1; }
  };

  struct traits : dynamic_container_traits {
    using index_definition_type = selectors<collection<processor>>;
  };

  container<traits> container;
  container.template register_type<scope<shared>, storage<processor_impl>,
                                   interfaces<processor>>();

  EXPECT_THROW(
      (container.template register_type<scope<shared>, storage<processor_impl>,
                                        interfaces<processor>>()),
      type_already_registered_exception);

  ASSERT_EQ(container.template resolve<processor &>().id(), 1);
  auto processors = container.template resolve<std::vector<processor *>>();
  ASSERT_EQ(processors.size(), 1U);
  ASSERT_EQ(processors[0]->id(), 1);
}

TEST(index_test, empty_child_no_key_one_selector_falls_back_to_parent) {
  struct processor {
    virtual ~processor() = default;
    virtual int id() const = 0;
  };
  struct parent_processor : processor {
    int id() const override { return 1; }
  };

  struct traits : dynamic_container_traits {
    using index_definition_type = selectors<selector<processor, no_key, one>>;
  };

  container<traits> parent;
  container<traits, std::allocator<char>, decltype(parent)> child(&parent);
  parent.template register_type<scope<shared>, storage<parent_processor>,
                                interfaces<processor>>();

  ASSERT_EQ(child.template resolve<processor &>().id(), 1);
}

TEST(index_test, child_no_key_one_selector_shadows_parent) {
  struct processor {
    virtual ~processor() = default;
    virtual int id() const = 0;
  };
  struct parent_processor : processor {
    int id() const override { return 1; }
  };
  struct child_processor : processor {
    int id() const override { return 2; }
  };

  struct traits : dynamic_container_traits {
    using index_definition_type = selectors<selector<processor, no_key, one>>;
  };

  container<traits> parent;
  container<traits, std::allocator<char>, decltype(parent)> child(&parent);
  parent.template register_type<scope<shared>, storage<parent_processor>,
                                interfaces<processor>>();
  child.template register_type<scope<shared>, storage<child_processor>,
                               interfaces<processor>>();

  ASSERT_EQ(child.template resolve<processor &>().id(), 2);
}

TEST(index_test, empty_child_no_key_many_selector_falls_back_to_parent) {
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
    using index_definition_type = selectors<collection<processor>>;
  };

  container<traits> parent;
  container<traits, std::allocator<char>, decltype(parent)> child(&parent);
  parent.template register_type<scope<shared>, storage<first_processor>,
                                interfaces<processor>>();
  parent.template register_type<scope<shared>, storage<second_processor>,
                                interfaces<processor>>();

  auto processors = child.template resolve<std::vector<processor *>>();

  ASSERT_EQ(processors.size(), 2U);
  ASSERT_EQ(processors[0]->id(), 1);
  ASSERT_EQ(processors[1]->id(), 2);
}

TEST(index_test, child_no_key_many_selector_collection_does_not_merge_parent) {
  struct processor {
    virtual ~processor() = default;
    virtual int id() const = 0;
  };
  struct parent_processor : processor {
    int id() const override { return 1; }
  };
  struct child_processor : processor {
    int id() const override { return 2; }
  };

  struct traits : dynamic_container_traits {
    using index_definition_type = selectors<collection<processor>>;
  };

  container<traits> parent;
  container<traits, std::allocator<char>, decltype(parent)> child(&parent);
  parent.template register_type<scope<shared>, storage<parent_processor>,
                                interfaces<processor>>();
  child.template register_type<scope<shared>, storage<child_processor>,
                               interfaces<processor>>();

  auto processors = child.template resolve<std::vector<processor *>>();

  ASSERT_EQ(processors.size(), 1U);
  ASSERT_EQ(processors[0]->id(), 2);
  ASSERT_EQ(child.template resolve<processor &>().id(), 2);
}

TEST(index_test, child_no_key_many_selector_ambiguity_does_not_fallback) {
  struct processor {
    virtual ~processor() = default;
    virtual int id() const = 0;
  };
  struct parent_processor : processor {
    int id() const override { return 1; }
  };
  struct first_child_processor : processor {
    int id() const override { return 2; }
  };
  struct second_child_processor : processor {
    int id() const override { return 3; }
  };

  struct traits : dynamic_container_traits {
    using index_definition_type = selectors<collection<processor>>;
  };

  container<traits> parent;
  container<traits, std::allocator<char>, decltype(parent)> child(&parent);
  parent.template register_type<scope<shared>, storage<parent_processor>,
                                interfaces<processor>>();
  child.template register_type<scope<shared>, storage<first_child_processor>,
                               interfaces<processor>>();
  child.template register_type<scope<shared>, storage<second_child_processor>,
                               interfaces<processor>>();

  auto processors = child.template resolve<std::vector<processor *>>();

  ASSERT_EQ(processors.size(), 2U);
  ASSERT_EQ(processors[0]->id(), 2);
  ASSERT_EQ(processors[1]->id(), 3);
  EXPECT_THROW((child.template resolve<processor &>()),
               type_ambiguous_exception);
}

TEST(index_test, typed_key_one_replaces_keyed_singular_identity) {
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

  struct traits : dynamic_container_traits {
    using index_definition_type =
        selectors<selector<processor, typed_key<processor_key>, one>>;
  };

  container<traits> container;
  container.template register_type<scope<shared>, storage<first_processor>,
                                   interfaces<processor>, key<processor_key>>();

  EXPECT_THROW((container.template register_type<
                   scope<shared>, storage<second_processor>,
                   interfaces<processor>, key<processor_key>>()),
               type_index_already_registered_exception);
  ASSERT_EQ(container.template resolve<processor &>(key<processor_key>{}).id(),
            1);
}

TEST(index_test, typed_key_one_does_not_satisfy_collection_lookup) {
  struct processor_key {};
  struct processor {
    virtual ~processor() = default;
    virtual int id() const = 0;
  };
  struct processor_impl : processor {
    int id() const override { return 1; }
  };

  struct traits : dynamic_container_traits {
    using index_definition_type =
        selectors<selector<processor, typed_key<processor_key>, one>>;
  };

  container<traits> container;
  container.template register_type<scope<shared>, storage<processor_impl>,
                                   interfaces<processor>, key<processor_key>>();

  EXPECT_THROW((container.template resolve<std::vector<processor *>>(
                   key<processor_key>{})),
               type_not_found_exception);
  ASSERT_EQ(container.template resolve<processor &>(key<processor_key>{}).id(),
            1);
}

TEST(index_test, typed_key_many_enumerates_keyed_collection_in_order) {
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

  struct traits : dynamic_container_traits {
    using index_definition_type =
        selectors<selector<processor, typed_key<processor_key>, many>>;
  };

  container<traits> container;
  container.template register_type<scope<shared>, storage<first_processor>,
                                   interfaces<processor>, key<processor_key>>();
  container.template register_type<scope<shared>, storage<second_processor>,
                                   interfaces<processor>, key<processor_key>>();

  auto processors = container.template resolve<std::vector<processor *>>(
      key<processor_key>{});

  ASSERT_EQ(processors.size(), 2U);
  ASSERT_EQ(processors[0]->id(), 1);
  ASSERT_EQ(processors[1]->id(), 2);
  EXPECT_THROW((container.template resolve<processor &>(key<processor_key>{})),
               type_ambiguous_exception);
}

TEST(index_test, typed_key_many_singular_resolve_requires_exactly_one_binding) {
  struct processor_key {};
  struct processor {
    virtual ~processor() = default;
    virtual int id() const = 0;
  };
  struct processor_impl : processor {
    int id() const override { return 1; }
  };

  struct traits : dynamic_container_traits {
    using index_definition_type =
        selectors<selector<processor, typed_key<processor_key>, many>>;
  };

  container<traits> container;
  EXPECT_THROW((container.template resolve<processor &>(key<processor_key>{})),
               type_not_found_exception);

  container.template register_type<scope<shared>, storage<processor_impl>,
                                   interfaces<processor>, key<processor_key>>();

  auto processors = container.template resolve<std::vector<processor *>>(
      key<processor_key>{});

  ASSERT_EQ(processors.size(), 1U);
  ASSERT_EQ(processors[0],
            &container.template resolve<processor &>(key<processor_key>{}));
}

TEST(index_test, typed_key_many_rejects_same_storage_duplicate) {
  struct processor_key {};
  struct processor {
    virtual ~processor() = default;
    virtual int id() const = 0;
  };
  struct processor_impl : processor {
    int id() const override { return 1; }
  };

  struct traits : dynamic_container_traits {
    using index_definition_type =
        selectors<selector<processor, typed_key<processor_key>, many>>;
  };

  container<traits> container;
  container.template register_type<scope<shared>, storage<processor_impl>,
                                   interfaces<processor>, key<processor_key>>();

  EXPECT_THROW(
      (container.template register_type<scope<shared>, storage<processor_impl>,
                                        interfaces<processor>,
                                        key<processor_key>>()),
      type_index_already_registered_exception);
}

TEST(index_test,
     failed_typed_key_many_registration_preserves_existing_selector_rows) {
  struct processor_key {};
  struct processor {
    virtual ~processor() = default;
    virtual int id() const = 0;
  };
  struct processor_impl : processor {
    int id() const override { return 1; }
  };

  struct traits : dynamic_container_traits {
    using index_definition_type =
        selectors<selector<processor, typed_key<processor_key>, many>>;
  };

  container<traits> container;
  container.template register_type<scope<shared>, storage<processor_impl>,
                                   interfaces<processor>, key<processor_key>>();

  EXPECT_THROW(
      (container.template register_type<scope<shared>, storage<processor_impl>,
                                        interfaces<processor>,
                                        key<processor_key>>()),
      type_index_already_registered_exception);

  ASSERT_EQ(container.template resolve<processor &>(key<processor_key>{}).id(),
            1);
  auto processors = container.template resolve<std::vector<processor *>>(
      key<processor_key>{});
  ASSERT_EQ(processors.size(), 1U);
  ASSERT_EQ(processors[0]->id(), 1);
}

TEST(index_test, empty_child_typed_key_one_selector_falls_back_to_parent) {
  struct processor_key {};
  struct processor {
    virtual ~processor() = default;
    virtual int id() const = 0;
  };
  struct parent_processor : processor {
    int id() const override { return 1; }
  };

  struct traits : dynamic_container_traits {
    using index_definition_type =
        selectors<selector<processor, typed_key<processor_key>, one>>;
  };

  container<traits> parent;
  container<traits, std::allocator<char>, decltype(parent)> child(&parent);
  parent.template register_type<scope<shared>, storage<parent_processor>,
                                interfaces<processor>, key<processor_key>>();

  ASSERT_EQ(child.template resolve<processor &>(key<processor_key>{}).id(), 1);
}

TEST(index_test, child_typed_key_one_selector_shadows_parent) {
  struct processor_key {};
  struct processor {
    virtual ~processor() = default;
    virtual int id() const = 0;
  };
  struct parent_processor : processor {
    int id() const override { return 1; }
  };
  struct child_processor : processor {
    int id() const override { return 2; }
  };

  struct traits : dynamic_container_traits {
    using index_definition_type =
        selectors<selector<processor, typed_key<processor_key>, one>>;
  };

  container<traits> parent;
  container<traits, std::allocator<char>, decltype(parent)> child(&parent);
  parent.template register_type<scope<shared>, storage<parent_processor>,
                                interfaces<processor>, key<processor_key>>();
  child.template register_type<scope<shared>, storage<child_processor>,
                               interfaces<processor>, key<processor_key>>();

  ASSERT_EQ(child.template resolve<processor &>(key<processor_key>{}).id(), 2);
}

TEST(index_test,
     empty_child_typed_key_many_selector_collection_falls_back_to_parent) {
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

  struct traits : dynamic_container_traits {
    using index_definition_type =
        selectors<selector<processor, typed_key<processor_key>, many>>;
  };

  container<traits> parent;
  container<traits, std::allocator<char>, decltype(parent)> child(&parent);
  parent.template register_type<scope<shared>, storage<first_processor>,
                                interfaces<processor>, key<processor_key>>();
  parent.template register_type<scope<shared>, storage<second_processor>,
                                interfaces<processor>, key<processor_key>>();

  auto processors =
      child.template resolve<std::vector<processor *>>(key<processor_key>{});

  ASSERT_EQ(processors.size(), 2U);
  ASSERT_EQ(processors[0]->id(), 1);
  ASSERT_EQ(processors[1]->id(), 2);
}

TEST(index_test,
     child_typed_key_many_selector_collection_does_not_merge_parent) {
  struct processor_key {};
  struct processor {
    virtual ~processor() = default;
    virtual int id() const = 0;
  };
  struct parent_processor : processor {
    int id() const override { return 1; }
  };
  struct first_child_processor : processor {
    int id() const override { return 2; }
  };
  struct second_child_processor : processor {
    int id() const override { return 3; }
  };

  struct traits : dynamic_container_traits {
    using index_definition_type =
        selectors<selector<processor, typed_key<processor_key>, many>>;
  };

  container<traits> parent;
  container<traits, std::allocator<char>, decltype(parent)> child(&parent);
  parent.template register_type<scope<shared>, storage<parent_processor>,
                                interfaces<processor>, key<processor_key>>();
  child.template register_type<scope<shared>, storage<first_child_processor>,
                               interfaces<processor>, key<processor_key>>();
  child.template register_type<scope<shared>, storage<second_child_processor>,
                               interfaces<processor>, key<processor_key>>();

  auto processors =
      child.template resolve<std::vector<processor *>>(key<processor_key>{});

  ASSERT_EQ(processors.size(), 2U);
  ASSERT_EQ(processors[0]->id(), 2);
  ASSERT_EQ(processors[1]->id(), 3);
}

TEST(index_test,
     child_typed_key_many_selector_singular_ambiguity_does_not_fall_back) {
  struct processor_key {};
  struct processor {
    virtual ~processor() = default;
    virtual int id() const = 0;
  };
  struct parent_processor : processor {
    int id() const override { return 1; }
  };
  struct first_child_processor : processor {
    int id() const override { return 2; }
  };
  struct second_child_processor : processor {
    int id() const override { return 3; }
  };

  struct traits : dynamic_container_traits {
    using index_definition_type =
        selectors<selector<processor, typed_key<processor_key>, many>>;
  };

  container<traits> parent;
  container<traits, std::allocator<char>, decltype(parent)> child(&parent);
  parent.template register_type<scope<shared>, storage<parent_processor>,
                                interfaces<processor>, key<processor_key>>();
  child.template register_type<scope<shared>, storage<first_child_processor>,
                               interfaces<processor>, key<processor_key>>();
  child.template register_type<scope<shared>, storage<second_child_processor>,
                               interfaces<processor>, key<processor_key>>();

  EXPECT_THROW((child.template resolve<processor &>(key<processor_key>{})),
               type_ambiguous_exception);
}

TEST(index_test, typed_key_one_selector_reuses_entry_cache) {
  struct processor_key {};

  struct traits : dynamic_container_traits {
    using index_definition_type = selectors<
        selector<typed_key_cached_processor, typed_key<processor_key>, one>>;
  };

  typed_key_cached_processor_impl::constructions = 0;

  container<traits> container;
  container.template register_type<
      scope<shared>, storage<typed_key_cached_processor_impl>,
      interfaces<typed_key_cached_processor>, key<processor_key>>();

  auto &first = container.template resolve<typed_key_cached_processor &>(
      key<processor_key>{});
  auto &second = container.template resolve<typed_key_cached_processor &>(
      key<processor_key>{});

  ASSERT_EQ(&second, &first);
  ASSERT_EQ(first.id(), 42);
  ASSERT_EQ(typed_key_cached_processor_impl::constructions, 1);
}

TEST(index_test, exception_message_index_out_of_range_negative) {
  auto e = detail::make_type_index_out_of_range_exception(-1, std::size_t(2));

  std::string expected = "type index out of range: key type ";
  expected += type_name<int>();
  ASSERT_STREQ(e.what(), expected.c_str());
}

TEST(index_test, same_key_type_can_use_different_selectors_per_interface) {
  struct processor {
    virtual ~processor() = default;
    virtual int id() const = 0;
  };
  struct animal {
    virtual ~animal() = default;
    virtual int id() const = 0;
  };
  struct processor_impl : processor {
    int id() const override { return 2; }
  };
  struct animal_impl : animal {
    int id() const override { return 15; }
  };

  struct traits : dynamic_container_traits {
    using index_definition_type = selectors<associative<processor, std::size_t>,
                                            associative<animal, std::size_t>>;
  };

  container<traits> container;
  container.template register_indexed_type<
      scope<shared>, storage<processor_impl>, interfaces<processor>>(
      std::size_t(2));
  container.template register_indexed_type<scope<shared>, storage<animal_impl>,
                                           interfaces<animal>>(std::size_t(15));

  ASSERT_EQ(container.template resolve<processor &>(std::size_t(2)).id(), 2);
  ASSERT_EQ(container.template resolve<animal &>(std::size_t(15)).id(), 15);
}

TEST(index_test,
     constructor_injects_dependencies_from_two_associative_selectors) {
  struct source {
    virtual ~source() = default;
    virtual int source_id() const = 0;
  };
  struct processor {
    virtual ~processor() = default;
    virtual int processor_id() const = 0;
  };
  struct file_source : source {
    int source_id() const override { return 1; }
  };
  struct network_source : source {
    int source_id() const override { return 2; }
  };
  struct json_processor : processor {
    int processor_id() const override { return 10; }
  };
  struct xml_processor : processor {
    int processor_id() const override { return 20; }
  };
  struct pipeline {
    pipeline(indexed<source &, key<std::size_t, 1>> selected_source,
             indexed<processor &, key<std::size_t, 0>> selected_processor)
        : input(selected_source), parser(selected_processor) {}

    source &input;
    processor &parser;
  };

  struct traits : dynamic_container_traits {
    using index_definition_type =
        selectors<associative<source, std::size_t>,
                  associative<processor, std::size_t>>;
  };

  container<traits> container;
  container.template register_indexed_type<scope<shared>, storage<file_source>,
                                           interfaces<source>>(std::size_t{0});
  container.template register_indexed_type<
      scope<shared>, storage<network_source>, interfaces<source>>(
      std::size_t{1});
  container.template register_indexed_type<
      scope<shared>, storage<json_processor>, interfaces<processor>>(
      std::size_t{0});
  container.template register_indexed_type<
      scope<shared>, storage<xml_processor>, interfaces<processor>>(
      std::size_t{1});

  auto configured_pipeline = container.template construct<pipeline>();

  ASSERT_EQ(configured_pipeline.input.source_id(), 2);
  ASSERT_EQ(configured_pipeline.parser.processor_id(), 10);
}

TEST(index_test, multi_interface_registration_inserts_each_interface_index) {
  struct processor {
    virtual ~processor() = default;
    virtual int processor_id() const = 0;
  };
  struct animal {
    virtual ~animal() = default;
    virtual int animal_id() const = 0;
  };
  struct processor_animal : processor, animal {
    int processor_id() const override { return 1; }
    int animal_id() const override { return 2; }
  };

  struct traits : dynamic_container_traits {
    using index_definition_type = selectors<associative<processor, std::size_t>,
                                            associative<animal, std::size_t>>;
  };

  container<traits> container;
  container.template register_indexed_type<
      scope<shared>, storage<processor_animal>, interfaces<processor, animal>>(
      std::size_t(2));

  ASSERT_EQ(
      container.template resolve<processor &>(std::size_t(2)).processor_id(),
      1);
  ASSERT_EQ(container.template resolve<animal &>(std::size_t(2)).animal_id(),
            2);
}

TEST(index_test, same_interface_can_use_multiple_key_types) {
  struct animal {
    virtual ~animal() = default;
    virtual int id() const = 0;
  };
  struct dog : animal {
    int id() const override { return 1; }
  };
  struct cat : animal {
    int id() const override { return 2; }
  };

  struct traits : dynamic_container_traits {
    using index_definition_type = selectors<associative<animal, std::size_t>,
                                            associative<animal, std::string>>;
  };

  container<traits> container;
  container.template register_indexed_type<scope<shared>, storage<dog>,
                                           interfaces<animal>>(std::size_t(1));
  container.template register_indexed_type<scope<shared>, storage<cat>,
                                           interfaces<animal>>(
      std::string("cat"));

  ASSERT_EQ(container.template resolve<animal &>(std::size_t(1)).id(), 1);
  ASSERT_EQ(container.template resolve<animal &>(std::string("cat")).id(), 2);
}

TEST(index_test, explicit_selector_resolves_with_custom_container_allocator) {
  struct animal {
    virtual ~animal() = default;
    virtual int id() const = 0;
  };
  struct dog : animal {
    int id() const override { return 11; }
  };

  struct traits : dynamic_container_traits {
    using index_definition_type = selectors<associative<animal, std::string>>;
  };

  test_allocator_state state;
  test_allocator<char> allocator(&state);
  container<traits, test_allocator<char>> container(allocator);

  container.template register_indexed_type<scope<shared>, storage<dog>,
                                           interfaces<animal>>(
      std::string("dog"));

  ASSERT_EQ(container.template resolve<animal &>(std::string("dog")).id(), 11);
}

TEST(index_test, runtime_key_selector_payload_uses_rebound_allocator) {
  struct allocator_visible_key {
    explicit allocator_visible_key(int resolved_value)
        : value(resolved_value) {}

    bool operator==(const allocator_visible_key &other) const {
      return value == other.value;
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

  struct traits : dynamic_container_traits {
    using index_definition_type =
        selectors<associative<animal, allocator_visible_key>>;
  };

  test_allocator_state state;
  {
    test_allocator<char> allocator(&state);
    container<traits, test_allocator<char>> container(allocator);

    container.template register_indexed_type<scope<shared>, storage<dog>,
                                             interfaces<animal>>(
        allocator_visible_key{7});

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
    using index_definition_type =
        selectors<associative<processor, std::size_t, many>>;
  };

  test_allocator_state state;
  {
    test_allocator<char> allocator(&state);
    container<traits, test_allocator<char>> container(allocator);
    container.template register_indexed_type<
        scope<shared>, storage<first_processor>, interfaces<processor>>(
        std::size_t(0));
    container.template register_indexed_type<
        scope<shared>, storage<second_processor>, interfaces<processor>>(
        std::size_t(0));

    auto values =
        container.template resolve<std::vector<processor *>>(std::size_t(0));
    ASSERT_EQ(values.size(), 2);
    ASSERT_GT(state.allocations, 0);
  }

  ASSERT_EQ(state.deallocations, state.allocations);
}

TEST(index_test,
     associative_many_allows_multiple_values_per_runtime_key_in_order) {
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
    using index_definition_type =
        selectors<associative<processor, std::size_t, many>>;
  };

  container<traits> container;
  container.template register_indexed_type<
      scope<shared>, storage<first_processor>, interfaces<processor>>(
      std::size_t(7));
  container.template register_indexed_type<
      scope<shared>, storage<second_processor>, interfaces<processor>>(
      std::size_t(7));
  container.template register_indexed_type<
      scope<shared>, storage<third_processor>, interfaces<processor>>(
      std::size_t(8));

  auto values =
      container.template resolve<std::vector<processor *>>(std::size_t(7));

  ASSERT_EQ(values.size(), 2);
  ASSERT_EQ(values[0]->id(), 1);
  ASSERT_EQ(values[1]->id(), 2);
  EXPECT_THROW((container.template resolve<processor &>(std::size_t(7))),
               type_ambiguous_exception);
  ASSERT_EQ(container.template resolve<processor &>(std::size_t(8)).id(), 3);
  EXPECT_THROW((container.template resolve<processor &>(std::size_t(9))),
               type_not_found_exception);
}

TEST(index_test, associative_many_rejects_same_storage_for_same_runtime_key) {
  struct processor {
    virtual ~processor() = default;
    virtual int id() const = 0;
  };
  struct processor_impl : processor {
    int id() const override { return 1; }
  };

  struct traits : dynamic_container_traits {
    using index_definition_type =
        selectors<associative<processor, std::size_t, many>>;
  };

  container<traits> container;
  container.template register_indexed_type<
      scope<shared>, storage<processor_impl>, interfaces<processor>>(
      std::size_t(7));

  EXPECT_THROW(
      (container.template register_indexed_type<
          scope<shared>, storage<processor_impl>, interfaces<processor>>(
          std::size_t(7))),
      type_index_already_registered_exception);

  container.template register_indexed_type<
      scope<shared>, storage<processor_impl>, interfaces<processor>>(
      std::size_t(8));
  ASSERT_EQ(container.template resolve<processor &>(std::size_t(8)).id(), 1);
}

TEST(index_test,
     failed_runtime_key_many_registration_preserves_existing_selector_rows) {
  struct processor {
    virtual ~processor() = default;
    virtual int id() const = 0;
  };
  struct processor_impl : processor {
    int id() const override { return 1; }
  };

  struct traits : dynamic_container_traits {
    using index_definition_type =
        selectors<associative<processor, std::size_t, many>>;
  };

  container<traits> container;
  container.template register_indexed_type<
      scope<shared>, storage<processor_impl>, interfaces<processor>>(
      std::size_t(7));

  EXPECT_THROW(
      (container.template register_indexed_type<
          scope<shared>, storage<processor_impl>, interfaces<processor>>(
          std::size_t(7))),
      type_index_already_registered_exception);

  ASSERT_EQ(container.template resolve<processor &>(std::size_t(7)).id(), 1);
  auto values =
      container.template resolve<std::vector<processor *>>(std::size_t(7));
  ASSERT_EQ(values.size(), 1);
  ASSERT_EQ(values[0]->id(), 1);
}

TEST(index_test,
     associative_many_single_match_shares_instance_between_lookups) {
  struct processor {
    virtual ~processor() = default;
    virtual int id() const = 0;
  };
  struct processor_impl : processor {
    int id() const override { return 1; }
  };

  struct traits : dynamic_container_traits {
    using index_definition_type =
        selectors<associative<processor, std::size_t, many>>;
  };

  container<traits> container;
  container.template register_indexed_type<
      scope<shared>, storage<std::shared_ptr<processor_impl>>,
      interfaces<processor>>(std::size_t(7));

  auto &selected = container.template resolve<processor &>(std::size_t(7));
  auto values =
      container.template resolve<std::vector<processor *>>(std::size_t(7));

  ASSERT_EQ(values.size(), 1);
  ASSERT_EQ(values[0], &selected);
  ASSERT_EQ(&selected, container.template resolve<processor *>(std::size_t(7)));
}

#if __cplusplus >= 202002L
TEST(index_test,
     external_selector_constructs_string_key_for_indexed_injection) {
  struct processor {
    virtual ~processor() = default;
    virtual int id() const = 0;
  };
  struct json_processor : processor {
    int id() const override { return 7; }
  };
  struct consumer {
    explicit consumer(
        indexed<processor &, external_key_string<std::string, "json">> selected)
        : selected_processor(selected) {}

    processor &selected_processor;
  };

  struct traits : dynamic_container_traits {
    using index_definition_type =
        selectors<associative<processor, std::string>>;
  };

  container<traits> container;
  container.template register_indexed_type<
      scope<shared>, storage<json_processor>, interfaces<processor>>(
      std::string{"json"});

  auto selected = container.template construct<consumer>();

  ASSERT_EQ(selected.selected_processor.id(), 7);
}
#endif

TEST(index_test, constructor_injects_fixed_indexed_associative_bindings) {
  struct traits : dynamic_container_traits {
    using index_definition_type =
        selectors<associative<fixed_injection_processor, std::size_t>>;
  };

  container<traits> container;
  container.template register_indexed_type<
      scope<shared>, storage<fixed_injection_processor_impl<0>>,
      interfaces<fixed_injection_processor>>(std::size_t(0));
  container.template register_indexed_type<
      scope<shared>, storage<fixed_injection_processor_impl<1>>,
      interfaces<fixed_injection_processor>>(std::size_t(1));

  auto first_consumer =
      container.template construct<fixed_injection_consumer<0>>();
  auto second_consumer =
      container.template construct<fixed_injection_consumer<1>>();

  ASSERT_EQ(first_consumer.processor.id(), 0);
  ASSERT_EQ(second_consumer.processor.id(), 1);
}

TEST(index_test, static_container_resolves_fixed_indexed_associative_bindings) {
  using source = bindings<
      bind<scope<shared>, storage<fixed_injection_processor_impl<0>>,
           interfaces<fixed_injection_processor>, key<std::size_t, 0>>,
      bind<scope<shared>, storage<fixed_injection_processor_impl<1>>,
           interfaces<fixed_injection_processor>, key<std::size_t, 1>>>;

  struct traits : static_container_traits<> {
    using index_definition_type =
        selectors<associative<fixed_injection_processor, std::size_t>>;
  };

  static_container<source, traits> container;

  auto &first = container.template resolve<
      indexed<fixed_injection_processor &, key<std::size_t, 0>>>();
  auto &second = container.template resolve<
      indexed<fixed_injection_processor &, key<std::size_t, 1>>>();

  ASSERT_EQ(first.id(), 0);
  ASSERT_EQ(second.id(), 1);
}

TEST(index_test,
     static_container_keeps_fixed_associative_cache_entries_distinct) {
  using source = bindings<
      bind<scope<shared>, storage<fixed_injection_processor_impl<7>>,
           interfaces<fixed_injection_processor>, key<std::size_t, 0>>,
      bind<scope<shared>, storage<fixed_injection_processor_impl<7>>,
           interfaces<fixed_injection_processor>, key<std::size_t, 1>>>;

  struct traits : static_container_traits<> {
    using index_definition_type =
        selectors<associative<fixed_injection_processor, std::size_t>>;
  };

  static_container<source, traits> container;

  auto &first = container.template resolve<
      indexed<fixed_injection_processor &, key<std::size_t, 0>>>();
  auto &first_again = container.template resolve<
      indexed<fixed_injection_processor &, key<std::size_t, 0>>>();
  auto &second = container.template resolve<
      indexed<fixed_injection_processor &, key<std::size_t, 1>>>();

  ASSERT_EQ(&first, &first_again);
  ASSERT_NE(&first, &second);
  ASSERT_EQ(first.id(), 7);
  ASSERT_EQ(second.id(), 7);
}

TEST(index_test, static_container_injects_fixed_indexed_dependency) {
  using source =
      bindings<bind<scope<shared>, storage<fixed_injection_processor_impl<1>>,
                    interfaces<fixed_injection_processor>, key<std::size_t, 1>>,
               bind<scope<unique>, storage<fixed_injection_consumer<1>>,
                    dependencies<indexed<fixed_injection_processor &,
                                         key<std::size_t, 1>>>>>;

  struct traits : static_container_traits<> {
    using index_definition_type =
        selectors<associative<fixed_injection_processor, std::size_t>>;
  };

  static_container<source, traits> container;

  auto consumer = container.template resolve<fixed_injection_consumer<1>>();

  ASSERT_EQ(consumer.processor.id(), 1);
}

TEST(index_test, static_container_preserves_typed_key_resolution) {
  struct static_typed_key {};
  using source = bindings<
      bind<scope<shared>, storage<fixed_injection_processor_impl<4>>,
           interfaces<fixed_injection_processor>, key<static_typed_key>>>;

  static_container<source> container;

  auto &processor = container.template resolve<fixed_injection_processor &>(
      key<static_typed_key>{});

  ASSERT_EQ(processor.id(), 4);
}

TEST(index_test, static_container_resolves_fixed_runtime_key_many_collection) {
  using source = bindings<
      bind<scope<shared>, storage<fixed_injection_processor_impl<0>>,
           interfaces<fixed_injection_processor>, key<std::size_t, 7>>,
      bind<scope<shared>, storage<fixed_injection_processor_impl<1>>,
           interfaces<fixed_injection_processor>, key<std::size_t, 7>>,
      bind<scope<shared>, storage<fixed_injection_processor_impl<2>>,
           interfaces<fixed_injection_processor>, key<std::size_t, 8>>>;

  struct traits : static_container_traits<> {
    using index_definition_type =
        selectors<associative<fixed_injection_processor, std::size_t, many>>;
  };

  static_container<source, traits> container;

  auto processors =
      container.template resolve<std::vector<fixed_injection_processor *>>(
          key<std::size_t, 7>{});
  auto constructed = container.template construct_collection<
      std::vector<fixed_injection_processor *>>(key<std::size_t, 7>{});

  ASSERT_EQ(
      (container.template count_collection<
          std::vector<fixed_injection_processor *>, key<std::size_t, 7>>()),
      2U);
  ASSERT_EQ(processors.size(), 2U);
  ASSERT_EQ(processors[0]->id(), 0);
  ASSERT_EQ(processors[1]->id(), 1);
  ASSERT_EQ(constructed.size(), 2U);
  ASSERT_EQ(constructed[0]->id(), 0);
  ASSERT_EQ(constructed[1]->id(), 1);
}

TEST(index_test, static_container_resolves_fixed_runtime_key_many_exactly_one) {
  using source = bindings<
      bind<scope<shared>, storage<fixed_injection_processor_impl<1>>,
           interfaces<fixed_injection_processor>, key<std::size_t, 7>>>;

  struct traits : static_container_traits<> {
    using index_definition_type =
        selectors<associative<fixed_injection_processor, std::size_t, many>>;
  };

  static_container<source, traits> container;

  auto &processor = container.template resolve<
      indexed<fixed_injection_processor &, key<std::size_t, 7>>>();

  ASSERT_EQ(processor.id(), 1);
}

TEST(index_test,
     static_fixed_runtime_key_many_allows_same_storage_different_key) {
  using source = bindings<
      bind<scope<shared>, storage<fixed_injection_processor_impl<9>>,
           interfaces<fixed_injection_processor>, key<std::size_t, 7>>,
      bind<scope<shared>, storage<fixed_injection_processor_impl<9>>,
           interfaces<fixed_injection_processor>, key<std::size_t, 8>>>;

  struct traits : static_container_traits<> {
    using index_definition_type =
        selectors<associative<fixed_injection_processor, std::size_t, many>>;
  };

  static_container<source, traits> container;

  auto &first = container.template resolve<
      indexed<fixed_injection_processor &, key<std::size_t, 7>>>();
  auto &second = container.template resolve<
      indexed<fixed_injection_processor &, key<std::size_t, 8>>>();

  ASSERT_EQ(first.id(), 9);
  ASSERT_EQ(second.id(), 9);
  ASSERT_NE(&first, &second);
}

TEST(index_test, static_container_fixed_runtime_key_many_parent_fallback) {
  using parent_source = bindings<
      bind<scope<shared>, storage<fixed_injection_processor_impl<1>>,
           interfaces<fixed_injection_processor>, key<std::size_t, 7>>,
      bind<scope<shared>, storage<fixed_injection_processor_impl<2>>,
           interfaces<fixed_injection_processor>, key<std::size_t, 7>>>;
  using child_source = bindings<>;

  struct traits : static_container_traits<> {
    using index_definition_type =
        selectors<associative<fixed_injection_processor, std::size_t, many>>;
  };

  static_container<parent_source, traits> parent;
  static_container<child_source, decltype(parent)> child(&parent);

  auto processors =
      child.template resolve<std::vector<fixed_injection_processor *>>(
          key<std::size_t, 7>{});

  ASSERT_EQ(processors.size(), 2U);
  ASSERT_EQ(processors[0]->id(), 1);
  ASSERT_EQ(processors[1]->id(), 2);
}

TEST(index_test, static_container_fixed_runtime_key_many_child_does_not_merge) {
  using parent_source = bindings<
      bind<scope<shared>, storage<fixed_injection_processor_impl<1>>,
           interfaces<fixed_injection_processor>, key<std::size_t, 7>>>;
  using child_source = bindings<
      bind<scope<shared>, storage<fixed_injection_processor_impl<2>>,
           interfaces<fixed_injection_processor>, key<std::size_t, 7>>>;

  struct traits : static_container_traits<> {
    using index_definition_type =
        selectors<associative<fixed_injection_processor, std::size_t, many>>;
  };

  static_container<parent_source, traits> parent;
  static_container<child_source, decltype(parent)> child(&parent);

  auto processors =
      child.template resolve<std::vector<fixed_injection_processor *>>(
          key<std::size_t, 7>{});

  ASSERT_EQ(processors.size(), 1U);
  ASSERT_EQ(processors[0]->id(), 2);
}

TEST(index_test, static_fixed_runtime_key_resolves_indexed_reference) {
  using source = bindings<
      bind<scope<shared>, storage<fixed_injection_processor_impl<1>>,
           interfaces<fixed_injection_processor>, key<std::size_t, 0>>>;

  struct traits : static_container_traits<> {
    using index_definition_type =
        selectors<associative<fixed_injection_processor, std::size_t>>;
  };

  static_container<source, traits> container;

  auto &processor = container.template resolve<
      indexed<fixed_injection_processor &, key<std::size_t, 0>>>();

  ASSERT_EQ(processor.id(), 1);
}

TEST(index_test, constructor_injects_fixed_indexed_associative_binding) {
  struct traits : dynamic_container_traits {
    using index_definition_type =
        selectors<associative<fixed_injection_processor, std::size_t>>;
  };

  container<traits> container;
  container.template register_indexed_type<
      scope<shared>, storage<fixed_injection_processor_impl<1>>,
      interfaces<fixed_injection_processor>>(std::size_t(1));

  auto consumer = container.template construct<fixed_injection_consumer<1>>();

  ASSERT_EQ(consumer.processor.id(), 1);
}

TEST(index_test, constructor_injects_fixed_indexed_enum_key) {
  struct traits : dynamic_container_traits {
    using index_definition_type = selectors<
        associative<fixed_injection_processor, fixed_injection_processor_id>>;
  };

  container<traits> container;
  container.template register_indexed_type<
      scope<shared>, storage<fixed_injection_processor_impl<1>>,
      interfaces<fixed_injection_processor>>(
      fixed_injection_processor_id::first);

  auto consumer = container.template construct<
      fixed_enum_consumer<fixed_injection_processor_id::first>>();

  ASSERT_EQ(consumer.processor.id(), 1);
}

TEST(index_test, constructor_fixed_indexed_missing_key_throws_type_not_found) {
  struct traits : dynamic_container_traits {
    using index_definition_type =
        selectors<associative<fixed_injection_processor, std::size_t>>;
  };

  container<traits> container;
  container.template register_indexed_type<
      scope<shared>, storage<fixed_injection_processor_impl<0>>,
      interfaces<fixed_injection_processor>>(std::size_t(0));

  EXPECT_THROW((container.template construct<fixed_injection_consumer<1>>()),
               type_not_found_exception);
}

TEST(index_test, registered_consumer_resolves_fixed_indexed_dependency) {
  struct traits : dynamic_container_traits {
    using index_definition_type =
        selectors<associative<fixed_injection_processor, std::size_t>>;
  };

  container<traits> container;
  container.template register_indexed_type<
      scope<shared>, storage<fixed_injection_processor_impl<1>>,
      interfaces<fixed_injection_processor>>(std::size_t(1));
  container.template register_type<
      scope<shared>, storage<registered_fixed_indexed_consumer>>();

  auto &consumer =
      container.template resolve<registered_fixed_indexed_consumer &>();

  ASSERT_EQ(consumer.processor.id(), 1);
}

TEST(index_test, selected_type_selector_registers_and_injects_keyed_binding) {
  container<> container;
  container.template register_type<
      scope<shared>, storage<fixed_injection_processor_impl<3>>,
      interfaces<detail::selected<
          fixed_injection_processor,
          detail::type_selector<selected_type_processor_tag>>>>();
  container
      .template register_type<scope<shared>, storage<selected_type_consumer>>();

  auto &consumer = container.template resolve<selected_type_consumer &>();

  ASSERT_EQ(consumer.processor.id(), 3);
}

TEST(index_test, selected_value_selector_injects_indexed_binding) {
  struct traits : dynamic_container_traits {
    using index_definition_type =
        selectors<associative<fixed_injection_processor, std::size_t>>;
  };

  container<traits> container;
  container.template register_indexed_type<
      scope<shared>, storage<fixed_injection_processor_impl<5>>,
      interfaces<fixed_injection_processor>>(std::size_t(1));
  container.template register_type<scope<shared>,
                                   storage<selected_value_consumer>>();

  auto &consumer = container.template resolve<selected_value_consumer &>();

  ASSERT_EQ(consumer.processor.id(), 5);
}

TEST(index_test, annotated_interfaces_keep_independent_indexes) {
  using primary_processor =
      annotated<fixed_injection_processor &, annotated_index_primary_tag>;
  using replica_processor =
      annotated<fixed_injection_processor &, annotated_index_replica_tag>;

  struct traits : dynamic_container_traits {
    using index_definition_type =
        selectors<associative<annotated<fixed_injection_processor,
                                        annotated_index_primary_tag>,
                              std::size_t>,
                  associative<annotated<fixed_injection_processor,
                                        annotated_index_replica_tag>,
                              std::size_t>>;
  };

  container<traits> container;
  container.template register_indexed_type<
      scope<shared>, storage<fixed_injection_processor_impl<11>>,
      interfaces<
          annotated<fixed_injection_processor, annotated_index_primary_tag>>>(
      std::size_t(1));
  container.template register_indexed_type<
      scope<shared>, storage<fixed_injection_processor_impl<22>>,
      interfaces<
          annotated<fixed_injection_processor, annotated_index_replica_tag>>>(
      std::size_t(3));

  primary_processor primary =
      container.template resolve<primary_processor>(std::size_t(1));
  replica_processor replica =
      container.template resolve<replica_processor>(std::size_t(3));

  fixed_injection_processor &primary_ref = primary;
  fixed_injection_processor &replica_ref = replica;

  ASSERT_EQ(primary_ref.id(), 11);
  ASSERT_EQ(replica_ref.id(), 22);
}

#ifdef DINGO_ENABLE_FUTURE_KEY_STRING_TESTS
TEST(index_test,
     DISABLED_constructor_injects_string_literal_key_into_string_index) {
  struct traits : dingo::dynamic_container_traits {
    using index_definition_type = dingo::selectors<
        dingo::associative<dingo::interfaces<string_processor>, std::string>>;
  };

  dingo::container<traits> container;
  container.template register_indexed_type<
      dingo::scope<dingo::shared>, dingo::storage<string_processor_impl<7>>,
      dingo::interfaces<string_processor>>(std::string{"json"});

  auto consumer = container.template construct<string_literal_consumer>();

  ASSERT_EQ(consumer.processor.id(), 7);
}
#endif

} // namespace dingo
