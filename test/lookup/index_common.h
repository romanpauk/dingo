//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/container.h>
#include <dingo/rtti/typeid_provider.h>
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

struct test_ordered_base_backend {
  template <typename Key, typename Value, typename Cardinality,
            typename Allocator>
  using storage = ordered::storage<Key, Value, Cardinality, Allocator>;
};

template <typename Backend> struct base_one_test_traits {
  template <typename> using rebind_t = base_one_test_traits<Backend>;

  using tag_type = none_t;
  using rtti_type = rtti<typeid_provider>;
  using allocator_type = std::allocator<char>;
  using lookup_definition_type = lookups<base<one, Backend>>;
};

template <typename Backend> struct base_many_test_traits {
  template <typename> using rebind_t = base_many_test_traits<Backend>;

  using tag_type = none_t;
  using rtti_type = rtti<typeid_provider>;
  using allocator_type = std::allocator<char>;
  using lookup_definition_type = lookups<base<many, Backend>>;
};

template <typename Range>
std::vector<int> sorted_processor_ids(const Range &values) {
  std::vector<int> ids;
  for (auto *value : values) {
    ids.push_back(value->id());
  }
  std::sort(ids.begin(), ids.end());
  return ids;
}

template <typename Backend>
void expect_base_one_backend_resolves_implicit_static_keys() {
  struct no_key_processor {
    virtual ~no_key_processor() {}
    virtual int id() const { return 0; }
  };
  struct no_key_processor_impl : no_key_processor {
    int id() const override { return 1; }
  };
  struct processor_key {};
  struct typed_key_processor {
    virtual ~typed_key_processor() {}
    virtual int id() const { return 0; }
  };
  struct typed_key_processor_impl : typed_key_processor {
    int id() const override { return 2; }
  };

  container<base_one_test_traits<Backend>> container;
  container
      .template register_type<scope<shared>, storage<no_key_processor_impl>,
                              interfaces<no_key_processor>>();
  container.template register_type<
      scope<shared>, storage<typed_key_processor_impl>,
      interfaces<typed_key_processor>, key_type<processor_key>>();

  ASSERT_EQ(container.template resolve<no_key_processor &>().id(), 1);
  ASSERT_EQ(
      container
          .template resolve<typed_key_processor &>(key_type<processor_key>{})
          .id(),
      2);
  auto no_key_processors =
      container.template resolve<std::vector<no_key_processor *>>();
  auto typed_key_processors =
      container.template resolve<std::vector<typed_key_processor *>>(
          key_type<processor_key>{});

  ASSERT_EQ(no_key_processors.size(), 1U);
  ASSERT_EQ(no_key_processors[0]->id(), 1);
  ASSERT_EQ(typed_key_processors.size(), 1U);
  ASSERT_EQ(typed_key_processors[0]->id(), 2);
}

template <typename Backend>
void expect_base_many_backend_enumerates_implicit_static_keys() {
  struct no_key_processor {
    virtual ~no_key_processor() {}
    virtual int id() const { return 0; }
  };
  struct first_no_key_processor : no_key_processor {
    int id() const override { return 1; }
  };
  struct second_no_key_processor : no_key_processor {
    int id() const override { return 2; }
  };
  struct processor_key {};
  struct typed_key_processor {
    virtual ~typed_key_processor() {}
    virtual int id() const { return 0; }
  };
  struct first_typed_key_processor : typed_key_processor {
    int id() const override { return 3; }
  };
  struct second_typed_key_processor : typed_key_processor {
    int id() const override { return 4; }
  };

  container<base_many_test_traits<Backend>> container;
  container
      .template register_type<scope<shared>, storage<first_no_key_processor>,
                              interfaces<no_key_processor>>();
  container
      .template register_type<scope<shared>, storage<second_no_key_processor>,
                              interfaces<no_key_processor>>();
  container.template register_type<
      scope<shared>, storage<first_typed_key_processor>,
      interfaces<typed_key_processor>, key_type<processor_key>>();
  container.template register_type<
      scope<shared>, storage<second_typed_key_processor>,
      interfaces<typed_key_processor>, key_type<processor_key>>();

  auto no_key_processors =
      container.template resolve<std::vector<no_key_processor *>>();
  auto typed_key_processors =
      container.template resolve<std::vector<typed_key_processor *>>(
          key_type<processor_key>{});

  ASSERT_EQ(no_key_processors.size(), 2U);
  ASSERT_EQ(sorted_processor_ids(no_key_processors), (std::vector<int>{1, 2}));
  ASSERT_EQ(typed_key_processors.size(), 2U);
  ASSERT_EQ(sorted_processor_ids(typed_key_processors),
            (std::vector<int>{3, 4}));
  EXPECT_THROW((container.template resolve<no_key_processor &>()),
               type_ambiguous_exception);
  EXPECT_THROW((container.template resolve<typed_key_processor &>(
                   key_type<processor_key>{})),
               type_ambiguous_exception);
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
      dependency<fixed_injection_processor &, key_type<std::size_t, Id>>
          selected)
      : processor(selected) {}

  fixed_injection_processor &processor;
};

enum class fixed_injection_processor_id { first, second };

template <fixed_injection_processor_id Id> struct fixed_enum_consumer {
  explicit fixed_enum_consumer(
      dependency<fixed_injection_processor &,
                 key_type<fixed_injection_processor_id, Id>>
          selected)
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
      dingo::dependency<string_processor &,
                        dingo::key_string<std::string, "json">>
          selected)
      : processor(selected) {}

  string_processor &processor;
};
#endif

} // namespace dingo
