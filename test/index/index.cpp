//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#include <dingo/container.h>
#include <dingo/runtime/lookup_index.h>
#include <dingo/storage/shared.h>
#include <dingo/storage/unique.h>
#include <dingo/type/type_name.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace dingo {

template <typename Range>
std::vector<int> sorted_processor_ids(const Range &values) {
  std::vector<int> ids;
  for (auto *value : values) {
    ids.push_back(value->id());
  }
  std::sort(ids.begin(), ids.end());
  return ids;
}

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

template <typename T> struct tracked_collection {
  using value_type = T;

  std::vector<T> values;
  std::size_t reserve_count = 0;
};

namespace detail {
template <typename T> struct collection_traits<tracked_collection<T>> {
  static const bool is_collection = true;
  static const bool has_fixed_size_construct = false;
  using resolve_type = T;

  static void reserve(tracked_collection<T> &collection, std::size_t size) {
    collection.reserve_count = size;
    collection.values.reserve(size);
  }

  template <typename U>
  static void add(tracked_collection<T> &collection, U &&value) {
    collection.values.emplace_back(std::forward<U>(value));
  }
};
} // namespace detail

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
      request<fixed_injection_processor &, key<std::size_t, Id>> selected)
      : processor(selected) {}

  fixed_injection_processor &processor;
};

enum class fixed_injection_processor_id { first, second };

template <fixed_injection_processor_id Id> struct fixed_enum_consumer {
  explicit fixed_enum_consumer(request<fixed_injection_processor &,
                                       key<fixed_injection_processor_id, Id>>
                                   selected)
      : processor(selected) {}

  fixed_injection_processor &processor;
};

struct registered_fixed_indexed_consumer {
  explicit registered_fixed_indexed_consumer(
      request<fixed_injection_processor &, key<std::size_t, 1>> selected)
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
// Internal regression hook: custom value selectors currently opt into request
// injection through detail traits. This is not a documented public extension.
template <typename T, ::dingo::external_fixed_string Value>
struct is_key_value<::dingo::external_key_string<T, Value>> : std::true_type {};

template <typename T, ::dingo::external_fixed_string Value>
struct key_value_traits<::dingo::external_key_string<T, Value>> {
  using type = T;

  static T make() {
    constexpr auto size = sizeof(Value.value) - 1;
    return T{std::string_view{Value.value, size}};
  }
};

template <typename T, ::dingo::external_fixed_string Value>
struct is_value_selector<::dingo::external_key_string<T, Value>>
    : std::true_type {
  using type = T;

  static T make() {
    return key_value_traits<::dingo::external_key_string<T, Value>>::make();
  }
};
} // namespace detail
#endif

#ifdef DINGO_ENABLE_FUTURE_KEY_STRING_TESTS
// Future API reference: key_string<std::string, "json"> should construct a
// std::string key and select the associative<std::string, Interface> lookup. It
// must not require transparent lookup or a std::string_view request.
struct string_processor {
  virtual ~string_processor() = default;
  virtual int id() const = 0;
};

template <int Id> struct string_processor_impl : string_processor {
  int id() const override { return Id; }
};

struct string_literal_consumer {
  explicit string_literal_consumer(
      dingo::request<string_processor &, dingo::key_string<std::string, "json">>
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
    using lookup_definition_type = lookups<associative<std::size_t, processor>>;
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
    using lookup_definition_type = lookups<associative<std::size_t, processor>>;
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
    using lookup_definition_type = lookups<associative<std::size_t, processor>>;
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
    using lookup_definition_type =
        lookups<associative<std::size_t, typed_key_cached_processor>>;
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
    using lookup_definition_type = lookups<associative<std::size_t, processor>>;
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

    container
        .template register_type<scope<shared>, storage<processor_impl>,
                                interfaces<processor>, key<processor_key>>();

    ASSERT_GT(state.allocations, 0);
    ASSERT_EQ(
        container.template resolve<processor &>(key<processor_key>{}).id(), 42);
  }

  ASSERT_EQ(state.deallocations, state.allocations);

  test_allocator_state large_state;
  {
    test_allocator<char> allocator(&large_state);
    container<dynamic_container_traits, test_allocator<char>> container(
        allocator);

    container
        .template register_type<scope<shared>, storage<large_processor_impl>,
                                interfaces<processor>, key<processor_key>>();

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
      type_already_registered_exception);

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
      type_already_registered_exception);

  auto processors =
      container.template resolve<tracked_collection<processor *>>();

  ASSERT_EQ(processors.reserve_count, 1U);
  ASSERT_EQ(processors.values.size(), 1U);
  ASSERT_EQ(processors.values[0]->id(), 1);
}

TEST(index_test, failed_normal_unkeyed_registration_preserves_lookup_rows) {
  typed_key_cached_processor_impl::constructions = 0;

  struct traits : dynamic_container_traits {
    using lookup_definition_type = lookups<single<typed_key_cached_processor>>;
  };

  container<traits> container;
  container.template register_type<scope<shared>,
                                   storage<typed_key_cached_processor_impl>,
                                   interfaces<typed_key_cached_processor>>();

  using probe =
      detail::runtime_slot_probe<typename decltype(container)::registry_type>;
  using lookup_entry =
      typename probe::template no_key_lookup_entry<typed_key_cached_processor,
                                                   one>;

  auto &before = container.template resolve<typed_key_cached_processor &>();
  ASSERT_EQ(probe::template lookup_index_row_count<lookup_entry>(
                container.registry(), none_t{}),
            1U);
  ASSERT_EQ(probe::template default_no_key_lookup_index_row_count<
                typed_key_cached_processor>(container.registry()),
            0U);

  EXPECT_THROW((container.template register_type<
                   scope<shared>, storage<typed_key_cached_processor_impl>,
                   interfaces<typed_key_cached_processor>>()),
               type_already_registered_exception);

  auto &after = container.template resolve<typed_key_cached_processor &>();
  ASSERT_EQ(probe::template lookup_index_row_count<lookup_entry>(
                container.registry(), none_t{}),
            1U);
  ASSERT_EQ(probe::template default_no_key_lookup_index_row_count<
                typed_key_cached_processor>(container.registry()),
            0U);
  ASSERT_EQ(&after, &before);
  ASSERT_EQ(after.id(), 42);
  ASSERT_EQ(typed_key_cached_processor_impl::constructions, 1);
}

TEST(index_test, failed_normal_unkeyed_registration_preserves_collection_rows) {
  typed_key_cached_processor_impl::constructions = 0;

  container<> container;
  container.template register_type<scope<shared>,
                                   storage<typed_key_cached_processor_impl>,
                                   interfaces<typed_key_cached_processor>>();

  using probe =
      detail::runtime_slot_probe<typename decltype(container)::registry_type>;
  auto &before = container.template resolve<typed_key_cached_processor &>();
  ASSERT_EQ(probe::template default_no_key_lookup_index_row_count<
                typed_key_cached_processor>(container.registry()),
            1U);

  EXPECT_THROW((container.template register_type<
                   scope<shared>, storage<typed_key_cached_processor_impl>,
                   interfaces<typed_key_cached_processor>>()),
               type_already_registered_exception);

  auto &after = container.template resolve<typed_key_cached_processor &>();
  ASSERT_EQ(probe::template default_no_key_lookup_index_row_count<
                typed_key_cached_processor>(container.registry()),
            1U);
  ASSERT_EQ(&after, &before);

  auto processors =
      container
          .template resolve<tracked_collection<typed_key_cached_processor *>>();
  ASSERT_EQ(processors.reserve_count, 1U);
  ASSERT_EQ(processors.values.size(), 1U);
  ASSERT_EQ(processors.values[0]->id(), 42);
  ASSERT_EQ(typed_key_cached_processor_impl::constructions, 1);
}

TEST(index_test, child_normal_unkeyed_lookup_shadows_parent) {
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

  container<> parent;
  container<dynamic_container_traits, std::allocator<char>, decltype(parent)>
      child(&parent);
  parent.template register_type<scope<shared>, storage<parent_processor>,
                                interfaces<processor>>();

  ASSERT_EQ(child.template resolve<processor &>().id(), 1);

  child.template register_type<scope<shared>, storage<child_processor>,
                               interfaces<processor>>();

  ASSERT_EQ(child.template resolve<processor &>().id(), 2);
}

TEST(index_test, child_normal_unkeyed_collection_does_not_merge_parent) {
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

  container<> parent;
  container<dynamic_container_traits, std::allocator<char>, decltype(parent)>
      child(&parent);
  parent.template register_type<scope<shared>, storage<parent_processor>,
                                interfaces<processor>>();
  child.template register_type<scope<shared>, storage<child_processor>,
                               interfaces<processor>>();

  auto processors = child.template resolve<tracked_collection<processor *>>();

  ASSERT_EQ(processors.reserve_count, 1U);
  ASSERT_EQ(processors.values.size(), 1U);
  ASSERT_EQ(processors.values[0]->id(), 2);
}

TEST(index_test, runtime_keyed_collection_requires_associative_many_lookup) {
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
  container.template register_indexed_type<
      scope<shared>, storage<processor_impl>, interfaces<processor>>(
      std::size_t(0));

  EXPECT_THROW(
      (container.template resolve<std::vector<processor *>>(std::size_t(0))),
      type_not_found_exception);
  ASSERT_EQ(container.template resolve<processor &>(std::size_t(0)).id(), 1);
}

TEST(index_test, implicit_typed_key_registration_reuses_entry_cache) {
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
  container.template register_type<scope<shared>, storage<first_processor>,
                                   interfaces<processor>, key<processor_key>>();

  ASSERT_EQ(container.template resolve<processor &>(key<processor_key>{}).id(),
            1);

  EXPECT_THROW((container.template register_type<
                   scope<shared>, storage<second_processor>,
                   interfaces<processor>, key<processor_key>>()),
               type_index_already_registered_exception);

  auto processors = container.template resolve<tracked_collection<processor *>>(
      key<processor_key>{});

  ASSERT_EQ(processors.reserve_count, 1U);
  ASSERT_EQ(processors.values.size(), 1U);
  ASSERT_EQ(processors.values[0]->id(), 1);
  ASSERT_EQ(container.template resolve<processor &>(key<processor_key>{}).id(),
            1);
}

TEST(index_test,
     failed_implicit_typed_key_registration_preserves_collection_rows) {
  struct processor_key {};

  typed_key_cached_processor_impl::constructions = 0;

  container<> container;
  container.template register_type<
      scope<shared>, storage<typed_key_cached_processor_impl>,
      interfaces<typed_key_cached_processor>, key<processor_key>>();

  using probe =
      detail::runtime_slot_probe<typename decltype(container)::registry_type>;
  auto &before = container.template resolve<typed_key_cached_processor &>(
      key<processor_key>{});
  ASSERT_EQ(
      (probe::template default_typed_key_lookup_index_row_count<
          typed_key_cached_processor, processor_key>(container.registry())),
      1U);

  EXPECT_THROW(
      (container.template register_type<
          scope<shared>, storage<typed_key_cached_processor_impl>,
          interfaces<typed_key_cached_processor>, key<processor_key>>()),
      type_index_already_registered_exception);

  auto &after = container.template resolve<typed_key_cached_processor &>(
      key<processor_key>{});
  ASSERT_EQ(
      (probe::template default_typed_key_lookup_index_row_count<
          typed_key_cached_processor, processor_key>(container.registry())),
      1U);
  ASSERT_EQ(&after, &before);

  auto processors =
      container
          .template resolve<tracked_collection<typed_key_cached_processor *>>(
              key<processor_key>{});
  ASSERT_EQ(processors.reserve_count, 1U);
  ASSERT_EQ(processors.values.size(), 1U);
  ASSERT_EQ(processors.values[0]->id(), 42);
  ASSERT_EQ(typed_key_cached_processor_impl::constructions, 1);
}

TEST(index_test, child_implicit_typed_key_collection_does_not_merge_parent) {
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

  container<> parent;
  container<dynamic_container_traits, std::allocator<char>, decltype(parent)>
      child(&parent);
  parent.template register_type<scope<shared>, storage<parent_processor>,
                                interfaces<processor>, key<processor_key>>();
  child.template register_type<scope<shared>, storage<child_processor>,
                               interfaces<processor>, key<processor_key>>();

  auto processors = child.template resolve<tracked_collection<processor *>>(
      key<processor_key>{});

  ASSERT_EQ(processors.reserve_count, 1U);
  ASSERT_EQ(processors.values.size(), 1U);
  ASSERT_EQ(processors.values[0]->id(), 2);
}

TEST(index_test, runtime_key_one_lookup_uses_equal_distinct_key_object) {
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

TEST(index_test, production_runtime_bindings_use_entry_owner_and_lookup_index) {
  struct no_key_one_processor {};
  struct no_key_many_processor {};
  struct typed_key_one_processor {};
  struct typed_key_many_processor {};
  struct runtime_key_one_processor {};
  struct runtime_key_many_processor {};
  struct processor_key {};

  struct traits : dynamic_container_traits {
    using lookup_definition_type =
        lookups<single<no_key_one_processor>, collection<no_key_many_processor>,
                typed<processor_key, typed_key_one_processor, one>,
                typed<processor_key, typed_key_many_processor, many>,
                associative<std::size_t, runtime_key_one_processor, one>,
                associative<std::size_t, runtime_key_many_processor, many>>;
  };
  using container_type = container<traits, test_allocator<char>>;
  using probe =
      detail::runtime_slot_probe<typename container_type::registry_type>;

  static_assert(probe::runtime_bindings_state_has_lookup_index());
  static_assert(probe::runtime_bindings_state_has_entry_owner());
  static_assert(probe::default_lookup_entry_is_runtime_key_lookup());
  static_assert(probe::default_lookup_index_get_is_reachable());
}

TEST(index_test, no_key_many_lookup_covers_empty_exact_ambiguous) {
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
    using lookup_definition_type = lookups<collection<processor>>;
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

TEST(index_test, typed_key_many_lookup_covers_collection_order) {
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
    using lookup_definition_type =
        lookups<typed<processor_key, processor, many>>;
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

TEST(index_test, runtime_key_many_lookup_uses_value_semantics) {
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

TEST(index_test, runtime_key_many_lookup_resolves_empty_collection) {
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
  struct traits : dynamic_container_traits {
    using lookup_definition_type =
        lookups<associative<comparable_key, processor, many>>;
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
      std::is_same_v<associative<std::size_t, processor>,
                     detail::lookup_definition<
                         processor, runtime_key<std::size_t>, one, ordered>>);

  struct traits : dynamic_container_traits {
    using lookup_definition_type = lookups<associative<std::size_t, processor>>;
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
      std::is_same_v<associative<std::size_t, processor, many>,
                     detail::lookup_definition<
                         processor, runtime_key<std::size_t>, many, ordered>>);

  struct traits : dynamic_container_traits {
    using lookup_definition_type =
        lookups<associative<std::size_t, processor, many>>;
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
    using lookup_definition_type =
        lookups<associative<std::size_t, typed_key_cached_processor>>;
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

TEST(index_test, empty_child_runtime_key_one_lookup_falls_back_to_parent) {
  struct processor {
    virtual ~processor() = default;
    virtual int id() const = 0;
  };
  struct parent_processor : processor {
    int id() const override { return 1; }
  };

  struct traits : dynamic_container_traits {
    using lookup_definition_type = lookups<associative<std::size_t, processor>>;
  };

  container<traits> parent;
  container<traits, std::allocator<char>, decltype(parent)> child(&parent);
  parent.template register_indexed_type<
      scope<shared>, storage<parent_processor>, interfaces<processor>>(
      std::size_t(7));

  ASSERT_EQ(child.template resolve<processor &>(std::size_t(7)).id(), 1);
}

TEST(index_test, child_runtime_key_one_lookup_shadows_parent) {
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
    using lookup_definition_type = lookups<associative<std::size_t, processor>>;
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
     empty_child_runtime_key_many_lookup_collection_falls_back_to_parent) {
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
    using lookup_definition_type =
        lookups<associative<std::size_t, processor, many>>;
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
     child_runtime_key_many_lookup_collection_does_not_merge_parent) {
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
    using lookup_definition_type =
        lookups<associative<std::size_t, processor, many>>;
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

TEST(index_test, child_runtime_key_many_lookup_ambiguity_does_not_fallback) {
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
    using lookup_definition_type =
        lookups<associative<std::size_t, processor, many>>;
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

TEST(index_test, no_key_one_replaces_unkeyed_singular_key) {
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
    using lookup_definition_type = lookups<single<processor>>;
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
    using lookup_definition_type = lookups<single<processor>>;
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
    using lookup_definition_type = lookups<single<processor>>;
  };

  container<traits> container;
  container.template register_type<scope<shared>, storage<processor_impl>,
                                   interfaces<processor>>();

  EXPECT_THROW((container.template resolve<std::vector<processor *>>()),
               type_not_found_exception);
  ASSERT_EQ(container.template resolve<processor &>().id(), 1);
}

TEST(index_test, no_key_many_enumerates_unkeyed_collection_entries) {
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
    using lookup_definition_type = lookups<collection<processor>>;
  };

  container<traits> container;
  container.template register_type<scope<shared>, storage<first_processor>,
                                   interfaces<processor>>();
  container.template register_type<scope<shared>, storage<second_processor>,
                                   interfaces<processor>>();

  auto processors = container.template resolve<std::vector<processor *>>();

  ASSERT_EQ(processors.size(), 2U);
  ASSERT_EQ(sorted_processor_ids(processors), (std::vector<int>{1, 2}));
  EXPECT_THROW((container.template resolve<processor &>()),
               type_ambiguous_exception);
}

TEST(index_test, collection_alias_behaves_like_no_key_many_lookup) {
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

  static_assert(std::is_same_v<collection<processor>, collection<processor>>);

  struct traits : dynamic_container_traits {
    using lookup_definition_type = lookups<collection<processor>>;
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
    using lookup_definition_type = lookups<collection<processor>>;
  };

  container<traits> container;
  container.template register_type<scope<shared>, storage<processor_impl>,
                                   interfaces<processor>>();

  auto processors = container.template resolve<std::vector<processor *>>();

  ASSERT_EQ(processors.size(), 1U);
  ASSERT_EQ(processors[0], &container.template resolve<processor &>());
}

TEST(index_test, no_key_many_returns_same_storage_duplicates_in_order) {
  struct processor {
    virtual ~processor() = default;
    virtual int id() const = 0;
  };
  struct processor_impl : processor {
    int id() const override { return 1; }
  };

  struct traits : dynamic_container_traits {
    using lookup_definition_type = lookups<collection<processor>>;
  };

  container<traits> container;
  container.template register_type<scope<shared>, storage<processor_impl>,
                                   interfaces<processor>>();
  container.template register_type<scope<shared>, storage<processor_impl>,
                                   interfaces<processor>>();

  auto processors = container.template resolve<std::vector<processor *>>();
  ASSERT_EQ(processors.size(), 2U);
  ASSERT_EQ(processors[0]->id(), 1);
  ASSERT_EQ(processors[1]->id(), 1);
  EXPECT_THROW((container.template resolve<processor &>()),
               type_ambiguous_exception);
}

TEST(index_test,
     no_key_many_duplicate_registration_preserves_existing_lookup_rows) {
  struct processor {
    virtual ~processor() = default;
    virtual int id() const = 0;
  };
  struct processor_impl : processor {
    int id() const override { return 1; }
  };

  struct traits : dynamic_container_traits {
    using lookup_definition_type = lookups<collection<processor>>;
  };

  container<traits> container;
  container.template register_type<scope<shared>, storage<processor_impl>,
                                   interfaces<processor>>();
  container.template register_type<scope<shared>, storage<processor_impl>,
                                   interfaces<processor>>();

  auto processors = container.template resolve<std::vector<processor *>>();
  ASSERT_EQ(processors.size(), 2U);
  ASSERT_EQ(processors[0]->id(), 1);
  ASSERT_EQ(processors[1]->id(), 1);
}

TEST(index_test, empty_child_no_key_one_lookup_falls_back_to_parent) {
  struct processor {
    virtual ~processor() = default;
    virtual int id() const = 0;
  };
  struct parent_processor : processor {
    int id() const override { return 1; }
  };

  struct traits : dynamic_container_traits {
    using lookup_definition_type = lookups<single<processor>>;
  };

  container<traits> parent;
  container<traits, std::allocator<char>, decltype(parent)> child(&parent);
  parent.template register_type<scope<shared>, storage<parent_processor>,
                                interfaces<processor>>();

  ASSERT_EQ(child.template resolve<processor &>().id(), 1);
}

TEST(index_test, child_no_key_one_lookup_shadows_parent) {
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
    using lookup_definition_type = lookups<single<processor>>;
  };

  container<traits> parent;
  container<traits, std::allocator<char>, decltype(parent)> child(&parent);
  parent.template register_type<scope<shared>, storage<parent_processor>,
                                interfaces<processor>>();
  child.template register_type<scope<shared>, storage<child_processor>,
                               interfaces<processor>>();

  ASSERT_EQ(child.template resolve<processor &>().id(), 2);
}

TEST(index_test, empty_child_no_key_many_lookup_falls_back_to_parent) {
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
    using lookup_definition_type = lookups<collection<processor>>;
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

TEST(index_test, child_no_key_many_lookup_collection_does_not_merge_parent) {
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
    using lookup_definition_type = lookups<collection<processor>>;
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

TEST(index_test, child_no_key_many_lookup_ambiguity_does_not_fallback) {
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
    using lookup_definition_type = lookups<collection<processor>>;
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

TEST(index_test, typed_key_one_replaces_keyed_singular_key) {
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
    using lookup_definition_type =
        lookups<typed<processor_key, processor, one>>;
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
    using lookup_definition_type =
        lookups<typed<processor_key, processor, one>>;
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

TEST(index_test, typed_key_many_enumerates_keyed_collection_entries) {
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
    using lookup_definition_type =
        lookups<typed<processor_key, processor, many>>;
  };

  container<traits> container;
  container.template register_type<scope<shared>, storage<first_processor>,
                                   interfaces<processor>, key<processor_key>>();
  container.template register_type<scope<shared>, storage<second_processor>,
                                   interfaces<processor>, key<processor_key>>();

  auto processors = container.template resolve<std::vector<processor *>>(
      key<processor_key>{});
  auto tracked = container.template resolve<tracked_collection<processor *>>(
      key<processor_key>{});

  ASSERT_EQ(processors.size(), 2U);
  ASSERT_EQ(sorted_processor_ids(processors), (std::vector<int>{1, 2}));
  ASSERT_EQ(tracked.reserve_count, 2U);
  ASSERT_EQ(tracked.values.size(), 2U);
  ASSERT_EQ(sorted_processor_ids(tracked.values), (std::vector<int>{1, 2}));
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
    using lookup_definition_type =
        lookups<typed<processor_key, processor, many>>;
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

TEST(index_test, typed_key_many_returns_same_storage_duplicates_in_order) {
  struct processor_key {};
  struct processor {
    virtual ~processor() = default;
    virtual int id() const = 0;
  };
  struct processor_impl : processor {
    int id() const override { return 1; }
  };

  struct traits : dynamic_container_traits {
    using lookup_definition_type =
        lookups<typed<processor_key, processor, many>>;
  };

  container<traits> container;
  container.template register_type<scope<shared>, storage<processor_impl>,
                                   interfaces<processor>, key<processor_key>>();
  container.template register_type<scope<shared>, storage<processor_impl>,
                                   interfaces<processor>, key<processor_key>>();

  auto processors = container.template resolve<std::vector<processor *>>(
      key<processor_key>{});
  ASSERT_EQ(processors.size(), 2U);
  ASSERT_EQ(processors[0]->id(), 1);
  ASSERT_EQ(processors[1]->id(), 1);
  EXPECT_THROW((container.template resolve<processor &>(key<processor_key>{})),
               type_ambiguous_exception);
}

TEST(index_test,
     typed_key_many_duplicate_registration_preserves_existing_lookup_rows) {
  struct processor_key {};
  struct processor {
    virtual ~processor() = default;
    virtual int id() const = 0;
  };
  struct processor_impl : processor {
    int id() const override { return 1; }
  };

  struct traits : dynamic_container_traits {
    using lookup_definition_type =
        lookups<typed<processor_key, processor, many>>;
  };

  container<traits> container;
  container.template register_type<scope<shared>, storage<processor_impl>,
                                   interfaces<processor>, key<processor_key>>();
  container.template register_type<scope<shared>, storage<processor_impl>,
                                   interfaces<processor>, key<processor_key>>();

  auto processors = container.template resolve<std::vector<processor *>>(
      key<processor_key>{});
  ASSERT_EQ(processors.size(), 2U);
  ASSERT_EQ(processors[0]->id(), 1);
  ASSERT_EQ(processors[1]->id(), 1);
}

TEST(index_test, empty_child_typed_key_one_lookup_falls_back_to_parent) {
  struct processor_key {};
  struct processor {
    virtual ~processor() = default;
    virtual int id() const = 0;
  };
  struct parent_processor : processor {
    int id() const override { return 1; }
  };

  struct traits : dynamic_container_traits {
    using lookup_definition_type =
        lookups<typed<processor_key, processor, one>>;
  };

  container<traits> parent;
  container<traits, std::allocator<char>, decltype(parent)> child(&parent);
  parent.template register_type<scope<shared>, storage<parent_processor>,
                                interfaces<processor>, key<processor_key>>();

  ASSERT_EQ(child.template resolve<processor &>(key<processor_key>{}).id(), 1);
}

TEST(index_test, child_typed_key_one_lookup_shadows_parent) {
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
    using lookup_definition_type =
        lookups<typed<processor_key, processor, one>>;
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
     empty_child_typed_key_many_lookup_collection_falls_back_to_parent) {
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
    using lookup_definition_type =
        lookups<typed<processor_key, processor, many>>;
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

TEST(index_test, child_typed_key_many_lookup_collection_does_not_merge_parent) {
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
    using lookup_definition_type =
        lookups<typed<processor_key, processor, many>>;
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
     child_typed_key_many_lookup_singular_ambiguity_does_not_fall_back) {
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
    using lookup_definition_type =
        lookups<typed<processor_key, processor, many>>;
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

TEST(index_test, typed_key_one_lookup_reuses_entry_cache) {
  struct processor_key {};

  struct traits : dynamic_container_traits {
    using lookup_definition_type =
        lookups<typed<processor_key, typed_key_cached_processor, one>>;
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

TEST(index_test, failed_typed_key_one_registration_preserves_lookup_rows) {
  struct processor_key {};

  struct traits : dynamic_container_traits {
    using lookup_definition_type =
        lookups<typed<processor_key, typed_key_cached_processor, one>>;
  };

  typed_key_cached_processor_impl::constructions = 0;

  container<traits> container;
  container.template register_type<
      scope<shared>, storage<typed_key_cached_processor_impl>,
      interfaces<typed_key_cached_processor>, key<processor_key>>();

  using probe =
      detail::runtime_slot_probe<typename decltype(container)::registry_type>;
  using lookup_entry = typename probe::template typed_key_lookup_entry<
      typed_key_cached_processor, processor_key, one>;

  auto &before = container.template resolve<typed_key_cached_processor &>(
      key<processor_key>{});
  ASSERT_EQ(probe::template lookup_index_row_count<lookup_entry>(
                container.registry(), none_t{}),
            1U);
  ASSERT_EQ(
      (probe::template default_typed_key_lookup_index_row_count<
          typed_key_cached_processor, processor_key>(container.registry())),
      0U);

  EXPECT_THROW(
      (container.template register_type<
          scope<shared>, storage<typed_key_cached_processor_impl>,
          interfaces<typed_key_cached_processor>, key<processor_key>>()),
      type_index_already_registered_exception);

  auto &after = container.template resolve<typed_key_cached_processor &>(
      key<processor_key>{});
  ASSERT_EQ(probe::template lookup_index_row_count<lookup_entry>(
                container.registry(), none_t{}),
            1U);
  ASSERT_EQ(
      (probe::template default_typed_key_lookup_index_row_count<
          typed_key_cached_processor, processor_key>(container.registry())),
      0U);
  ASSERT_EQ(&after, &before);
  ASSERT_EQ(after.id(), 42);
  ASSERT_EQ(typed_key_cached_processor_impl::constructions, 1);
}

TEST(index_test, exception_message_index_out_of_range_negative) {
  auto e = detail::make_type_index_out_of_range_exception(-1, std::size_t(2));

  std::string expected = "type index out of range: key type ";
  expected += type_name<int>();
  ASSERT_STREQ(e.what(), expected.c_str());
}

TEST(index_test, same_key_type_can_use_different_lookups_per_interface) {
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
    using lookup_definition_type = lookups<associative<std::size_t, processor>,
                                           associative<std::size_t, animal>>;
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
     constructor_injects_dependencies_from_two_associative_lookups) {
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
    pipeline(request<source &, key<std::size_t, 1>> selected_source,
             request<processor &, key<std::size_t, 0>> selected_processor)
        : input(selected_source), parser(selected_processor) {}

    source &input;
    processor &parser;
  };

  struct traits : dynamic_container_traits {
    using lookup_definition_type = lookups<associative<std::size_t, source>,
                                           associative<std::size_t, processor>>;
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
    using lookup_definition_type = lookups<associative<std::size_t, processor>,
                                           associative<std::size_t, animal>>;
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
    using lookup_definition_type = lookups<associative<std::size_t, animal>,
                                           associative<std::string, animal>>;
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

  container.template register_indexed_type<scope<shared>, storage<dog>,
                                           interfaces<animal>>(
      std::string("dog"));

  ASSERT_EQ(container.template resolve<animal &>(std::string("dog")).id(), 11);
}

TEST(index_test, runtime_key_lookup_backend_uses_rebound_allocator) {
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

    container.template register_indexed_type<scope<shared>, storage<dog>,
                                             interfaces<animal>>(small_key{7});

    ASSERT_EQ(small_state.allocations, 4);
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

    container.template register_indexed_type<scope<shared>, storage<dog>,
                                             interfaces<animal>>(
        allocator_visible_key{7});

    ASSERT_EQ(state.allocations, 4);
    ASSERT_GE(state.largest_allocation_bytes, sizeof(allocator_visible_key));
    ASSERT_EQ(
        container.template resolve<animal &>(allocator_visible_key{7}).id(),
        11);
  }

  ASSERT_EQ(state.deallocations, state.allocations);
}

TEST(index_test, lookup_backend_inserts_runtime_key_rows) {
  struct row {
    int id;
  };
  struct processor {};
  using entry =
      detail::lookup_entry<processor, runtime_key<std::size_t>, one, ordered>;

  std::allocator<char> allocator;
  detail::lookup_backend<entry, row *, std::allocator<char>> backend(allocator);

  row first{1};
  row duplicate{2};
  auto inserted = backend.try_emplace(std::size_t{7}, &first);
  auto rejected = backend.try_emplace(std::size_t{7}, &duplicate);

  ASSERT_TRUE(inserted.second);
  ASSERT_FALSE(rejected.second);

  auto found = backend.find(std::size_t{7});
  ASSERT_NE(found, backend.end());
  ASSERT_EQ((*found).second, &first);

  backend.erase(inserted.first);
  ASSERT_EQ(backend.find(std::size_t{7}), backend.end());
}

TEST(index_test, lookup_backend_iterates_many_runtime_key_rows) {
  struct row {
    int id;
  };
  struct processor {};
  using entry = detail::lookup_entry<processor, runtime_key<std::size_t>, many,
                                     unordered>;

  std::allocator<char> allocator;
  detail::lookup_backend<entry, row *, std::allocator<char>> backend(allocator);

  row first{1};
  row second{2};
  auto first_handle = backend.emplace(std::size_t{7}, &first);
  backend.emplace(std::size_t{7}, &second);

  ASSERT_NE(backend.find(std::size_t{7}), backend.end());

  std::vector<int> ids;
  auto [range_first, range_last] = backend.equal_range(std::size_t{7});
  for (auto it = range_first; it != range_last; ++it) {
    ids.push_back((*it).second->id);
  }
  std::sort(ids.begin(), ids.end());
  ASSERT_EQ(ids, (std::vector<int>{1, 2}));

  backend.erase(first_handle);
  ASSERT_NE(backend.find(std::size_t{7}), backend.end());
}

TEST(index_test, lookup_backend_inserts_no_key_rows) {
  struct row {
    int id;
  };
  struct processor {};
  using entry =
      detail::lookup_entry<processor, no_key, one, detail::no_lookup_backend>;

  std::allocator<char> allocator;
  detail::lookup_backend<entry, row *, std::allocator<char>> backend(allocator);

  row value{3};
  row duplicate{4};
  auto inserted = backend.try_emplace(none_t{}, &value);
  auto rejected = backend.try_emplace(none_t{}, &duplicate);

  ASSERT_TRUE(inserted.second);
  ASSERT_FALSE(rejected.second);

  auto found = backend.find(none_t{});
  ASSERT_NE(found, backend.end());
  ASSERT_EQ(*found, &value);

  backend.erase(inserted.first);
  ASSERT_EQ(backend.find(none_t{}), backend.end());
}

TEST(index_test, lookup_backend_inserts_typed_key_rows) {
  struct row {
    int id;
  };
  struct processor {};
  struct selected_key {};
  using entry = detail::lookup_entry<processor, typed_key<selected_key>, one,
                                     detail::no_lookup_backend>;

  std::allocator<char> allocator;
  detail::lookup_backend<entry, row *, std::allocator<char>> backend(allocator);

  row value{4};
  auto inserted = backend.try_emplace(none_t{}, &value);

  ASSERT_TRUE(inserted.second);

  auto found = backend.find(none_t{});
  ASSERT_NE(found, backend.end());
  ASSERT_EQ(*found, &value);

  backend.erase(inserted.first);
  ASSERT_EQ(backend.find(none_t{}), backend.end());
}

TEST(index_test, lookup_backend_iterates_many_static_key_rows) {
  struct row {
    int id;
  };
  struct processor {};
  using entry =
      detail::lookup_entry<processor, no_key, many, detail::no_lookup_backend>;

  std::allocator<char> allocator;
  detail::lookup_backend<entry, row *, std::allocator<char>> backend(allocator);

  row first{5};
  row second{6};
  backend.emplace(none_t{}, &first);
  backend.emplace(none_t{}, &second);

  auto found = backend.find(none_t{});
  ASSERT_NE(found, backend.end());
  ASSERT_EQ(*found, &first);

  std::vector<int> ids;
  auto [range_first, range_last] = backend.equal_range(none_t{});
  for (auto it = range_first; it != range_last; ++it) {
    ids.push_back((*it)->id);
  }
  ASSERT_EQ(ids, (std::vector<int>{5, 6}));

  backend.erase(backend.find(none_t{}));

  auto after_erase = backend.find(none_t{});
  ASSERT_NE(after_erase, backend.end());
  ASSERT_EQ(*after_erase, &second);
}

TEST(index_test, lookup_index_get_returns_lookup_backend) {
  struct row {
    int id;
  };
  struct processor {};
  using runtime_entry =
      detail::lookup_entry<processor, runtime_key<std::size_t>, one, ordered>;
  using no_key_entry =
      detail::lookup_entry<processor, no_key, one, detail::no_lookup_backend>;
  using index_type =
      detail::lookup_index<type_list<runtime_entry, no_key_entry>, row *,
                           std::allocator<char>>;

  static_assert(
      std::is_same_v<
          decltype(std::declval<index_type &>().template get<runtime_entry>()),
          detail::lookup_backend<runtime_entry, row *, std::allocator<char>>
              &>);
  static_assert(
      std::is_same_v<decltype(std::declval<const index_type &>()
                                  .template get<no_key_entry>()),
                     const detail::lookup_backend<no_key_entry, row *,
                                                  std::allocator<char>> &>);

  std::allocator<char> allocator;
  index_type index(allocator);

  row value{5};
  auto inserted =
      index.template get<runtime_entry>().try_emplace(std::size_t{9}, &value);

  ASSERT_TRUE(inserted.second);
  auto found = index.template get<runtime_entry>().find(std::size_t{9});
  ASSERT_NE(found, index.template get<runtime_entry>().end());
  ASSERT_EQ((*found).second, &value);
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

TEST(index_test, associative_many_allows_multiple_values_per_runtime_key) {
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
  ASSERT_EQ(sorted_processor_ids(values), (std::vector<int>{1, 2}));
  EXPECT_THROW((container.template resolve<processor &>(std::size_t(7))),
               type_ambiguous_exception);
  ASSERT_EQ(container.template resolve<processor &>(std::size_t(8)).id(), 3);
  EXPECT_THROW((container.template resolve<processor &>(std::size_t(9))),
               type_not_found_exception);
}

TEST(index_test,
     associative_many_returns_same_storage_duplicates_for_same_runtime_key) {
  struct processor {
    virtual ~processor() = default;
    virtual int id() const = 0;
  };
  struct processor_impl : processor {
    int id() const override { return 1; }
  };

  struct traits : dynamic_container_traits {
    using lookup_definition_type =
        lookups<associative<std::size_t, processor, many>>;
  };

  container<traits> container;
  container.template register_indexed_type<
      scope<shared>, storage<processor_impl>, interfaces<processor>>(
      std::size_t(7));
  container.template register_indexed_type<
      scope<shared>, storage<processor_impl>, interfaces<processor>>(
      std::size_t(7));

  container.template register_indexed_type<
      scope<shared>, storage<processor_impl>, interfaces<processor>>(
      std::size_t(8));

  auto values =
      container.template resolve<std::vector<processor *>>(std::size_t(7));
  ASSERT_EQ(values.size(), 2);
  ASSERT_EQ(values[0]->id(), 1);
  ASSERT_EQ(values[1]->id(), 1);
  EXPECT_THROW((container.template resolve<processor &>(std::size_t(7))),
               type_ambiguous_exception);
  ASSERT_EQ(container.template resolve<processor &>(std::size_t(8)).id(), 1);
}

TEST(index_test,
     runtime_key_many_duplicate_registration_preserves_existing_lookup_rows) {
  struct processor {
    virtual ~processor() = default;
    virtual int id() const = 0;
  };
  struct processor_impl : processor {
    int id() const override { return 1; }
  };

  struct traits : dynamic_container_traits {
    using lookup_definition_type =
        lookups<associative<std::size_t, processor, many>>;
  };

  container<traits> container;
  container.template register_indexed_type<
      scope<shared>, storage<processor_impl>, interfaces<processor>>(
      std::size_t(7));
  container.template register_indexed_type<
      scope<shared>, storage<processor_impl>, interfaces<processor>>(
      std::size_t(7));

  auto values =
      container.template resolve<std::vector<processor *>>(std::size_t(7));
  ASSERT_EQ(values.size(), 2);
  ASSERT_EQ(values[0]->id(), 1);
  ASSERT_EQ(values[1]->id(), 1);
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
    using lookup_definition_type =
        lookups<associative<std::size_t, processor, many>>;
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
TEST(index_test, external_lookup_constructs_string_key_for_indexed_injection) {
  struct processor {
    virtual ~processor() = default;
    virtual int id() const = 0;
  };
  struct json_processor : processor {
    int id() const override { return 7; }
  };
  struct consumer {
    explicit consumer(
        request<processor &, external_key_string<std::string, "json">> selected)
        : selected_processor(selected) {}

    processor &selected_processor;
  };

  struct traits : dynamic_container_traits {
    using lookup_definition_type = lookups<associative<std::string, processor>>;
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
    using lookup_definition_type =
        lookups<associative<std::size_t, fixed_injection_processor>>;
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
    using lookup_definition_type =
        lookups<associative<std::size_t, fixed_injection_processor>>;
  };

  static_container<source, traits> container;

  auto &first = container.template resolve<
      request<fixed_injection_processor &, key<std::size_t, 0>>>();
  auto &second = container.template resolve<
      request<fixed_injection_processor &, key<std::size_t, 1>>>();

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
    using lookup_definition_type =
        lookups<associative<std::size_t, fixed_injection_processor>>;
  };

  static_container<source, traits> container;

  auto &first = container.template resolve<
      request<fixed_injection_processor &, key<std::size_t, 0>>>();
  auto &first_again = container.template resolve<
      request<fixed_injection_processor &, key<std::size_t, 0>>>();
  auto &second = container.template resolve<
      request<fixed_injection_processor &, key<std::size_t, 1>>>();

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
                    dependencies<request<fixed_injection_processor &,
                                         key<std::size_t, 1>>>>>;

  struct traits : static_container_traits<> {
    using lookup_definition_type =
        lookups<associative<std::size_t, fixed_injection_processor>>;
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

TEST(index_test, static_container_resolves_key_value_many_collection) {
  using source = bindings<
      bind<scope<shared>, storage<fixed_injection_processor_impl<0>>,
           interfaces<fixed_injection_processor>, key<std::size_t, 7>>,
      bind<scope<shared>, storage<fixed_injection_processor_impl<1>>,
           interfaces<fixed_injection_processor>, key<std::size_t, 7>>,
      bind<scope<shared>, storage<fixed_injection_processor_impl<2>>,
           interfaces<fixed_injection_processor>, key<std::size_t, 8>>>;

  struct traits : static_container_traits<> {
    using lookup_definition_type =
        lookups<associative<std::size_t, fixed_injection_processor, many>>;
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

TEST(index_test, static_container_resolves_key_value_many_exactly_one) {
  using source = bindings<
      bind<scope<shared>, storage<fixed_injection_processor_impl<1>>,
           interfaces<fixed_injection_processor>, key<std::size_t, 7>>>;

  struct traits : static_container_traits<> {
    using lookup_definition_type =
        lookups<associative<std::size_t, fixed_injection_processor, many>>;
  };

  static_container<source, traits> container;

  auto &processor = container.template resolve<
      request<fixed_injection_processor &, key<std::size_t, 7>>>();

  ASSERT_EQ(processor.id(), 1);
}

TEST(index_test, static_key_value_many_allows_same_storage_different_key) {
  using source = bindings<
      bind<scope<shared>, storage<fixed_injection_processor_impl<9>>,
           interfaces<fixed_injection_processor>, key<std::size_t, 7>>,
      bind<scope<shared>, storage<fixed_injection_processor_impl<9>>,
           interfaces<fixed_injection_processor>, key<std::size_t, 8>>>;

  struct traits : static_container_traits<> {
    using lookup_definition_type =
        lookups<associative<std::size_t, fixed_injection_processor, many>>;
  };

  static_container<source, traits> container;

  auto &first = container.template resolve<
      request<fixed_injection_processor &, key<std::size_t, 7>>>();
  auto &second = container.template resolve<
      request<fixed_injection_processor &, key<std::size_t, 8>>>();

  ASSERT_EQ(first.id(), 9);
  ASSERT_EQ(second.id(), 9);
  ASSERT_NE(&first, &second);
}

TEST(index_test, static_container_key_value_many_parent_fallback) {
  using parent_source = bindings<
      bind<scope<shared>, storage<fixed_injection_processor_impl<1>>,
           interfaces<fixed_injection_processor>, key<std::size_t, 7>>,
      bind<scope<shared>, storage<fixed_injection_processor_impl<2>>,
           interfaces<fixed_injection_processor>, key<std::size_t, 7>>>;
  using child_source = bindings<>;

  struct traits : static_container_traits<> {
    using lookup_definition_type =
        lookups<associative<std::size_t, fixed_injection_processor, many>>;
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

TEST(index_test, static_container_key_value_many_child_does_not_merge) {
  using parent_source = bindings<
      bind<scope<shared>, storage<fixed_injection_processor_impl<1>>,
           interfaces<fixed_injection_processor>, key<std::size_t, 7>>>;
  using child_source = bindings<
      bind<scope<shared>, storage<fixed_injection_processor_impl<2>>,
           interfaces<fixed_injection_processor>, key<std::size_t, 7>>>;

  struct traits : static_container_traits<> {
    using lookup_definition_type =
        lookups<associative<std::size_t, fixed_injection_processor, many>>;
  };

  static_container<parent_source, traits> parent;
  static_container<child_source, decltype(parent)> child(&parent);

  auto processors =
      child.template resolve<std::vector<fixed_injection_processor *>>(
          key<std::size_t, 7>{});

  ASSERT_EQ(processors.size(), 1U);
  ASSERT_EQ(processors[0]->id(), 2);
}

TEST(index_test, static_key_value_resolves_indexed_reference) {
  using source = bindings<
      bind<scope<shared>, storage<fixed_injection_processor_impl<1>>,
           interfaces<fixed_injection_processor>, key<std::size_t, 0>>>;

  struct traits : static_container_traits<> {
    using lookup_definition_type =
        lookups<associative<std::size_t, fixed_injection_processor>>;
  };

  static_container<source, traits> container;

  auto &processor = container.template resolve<
      request<fixed_injection_processor &, key<std::size_t, 0>>>();

  ASSERT_EQ(processor.id(), 1);
}

TEST(index_test, constructor_injects_fixed_indexed_associative_binding) {
  struct traits : dynamic_container_traits {
    using lookup_definition_type =
        lookups<associative<std::size_t, fixed_injection_processor>>;
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
    using lookup_definition_type = lookups<
        associative<fixed_injection_processor_id, fixed_injection_processor>>;
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
    using lookup_definition_type =
        lookups<associative<std::size_t, fixed_injection_processor>>;
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
    using lookup_definition_type =
        lookups<associative<std::size_t, fixed_injection_processor>>;
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
    using lookup_definition_type =
        lookups<associative<std::size_t, fixed_injection_processor>>;
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
    using lookup_definition_type = lookups<
        associative<std::size_t, annotated<fixed_injection_processor,
                                           annotated_index_primary_tag>>,
        associative<std::size_t, annotated<fixed_injection_processor,
                                           annotated_index_replica_tag>>>;
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
    using lookup_definition_type = dingo::lookups<
        dingo::associative<std::string, dingo::interfaces<string_processor>>>;
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
