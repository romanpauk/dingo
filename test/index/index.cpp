//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#include <dingo/container.h>
#include <dingo/storage/shared.h>
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

struct fixed_injection_processor {
  virtual ~fixed_injection_processor() = default;
  virtual int id() const = 0;
};

template <int Id>
struct fixed_injection_processor_impl : fixed_injection_processor {
  int id() const override { return Id; }
};

template <std::size_t Id> struct fixed_injection_consumer {
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

TEST(index_test,
     runtime_keyed_collection_requires_associative_many_projection) {
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

TEST(index_test, typed_key_one_selector_reuses_projection_entry_cache) {
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

TEST(index_test, selector_projection_storage_uses_rebound_allocator) {
  test_allocator_state state;
  {
    test_allocator<char> allocator(&state);
    detail::unique_selector_projection<int, int, test_allocator<char>>
        unique_projection(allocator);
    detail::multi_selector_projection<int, int, test_allocator<char>>
        multi_projection(allocator);

    unique_projection.emplace(1, 10);
    multi_projection.emplace(2, 20);

    ASSERT_GE(state.allocations, 2);
    ASSERT_LT(state.deallocations, state.allocations);
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
