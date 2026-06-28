//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#include <dingo/container.h>
#include <dingo/index/array.h>
#include <dingo/index/map.h>
#include <dingo/index/unordered_map.h>
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

namespace index_type {
struct test_vector {};
struct test_allocator_vector {};
} // namespace index_type

struct test_allocator_state {
  int backend_constructed = 0;
};

template <typename T> class test_backend_allocator {
public:
  using value_type = T;

  explicit test_backend_allocator(test_allocator_state *state = nullptr)
      : state_(state) {}

  template <typename U>
  test_backend_allocator(const test_backend_allocator<U> &other) noexcept
      : state_(other.state()) {}

  value_type *allocate(std::size_t n) {
    return static_cast<value_type *>(::operator new(n * sizeof(value_type)));
  }

  void deallocate(value_type *p, std::size_t) noexcept { ::operator delete(p); }

  test_allocator_state *state() const { return state_; }

  void record_backend_constructed() {
    if (state_) {
      ++state_->backend_constructed;
    }
  }

private:
  test_allocator_state *state_;
};

template <typename T, typename U>
bool operator==(const test_backend_allocator<T> &lhs,
                const test_backend_allocator<U> &rhs) noexcept {
  return lhs.state() == rhs.state();
}

template <typename T, typename U>
bool operator!=(const test_backend_allocator<T> &lhs,
                const test_backend_allocator<U> &rhs) noexcept {
  return !(lhs == rhs);
}

namespace detail {
template <typename Key, typename Value, typename Allocator>
struct index_storage<index_type::test_vector, Key, Value, Allocator> {
  index_storage(Allocator &allocator) : values_(allocator) {}

  bool emplace(Key key, Value value) {
    for (auto &&entry : values_) {
      if (entry.first == key) {
        return false;
      }
    }
    values_.emplace_back(std::move(key), value);
    return true;
  }

  Value *find(const Key &key) {
    for (auto &&entry : values_) {
      if (entry.first == key) {
        return &entry.second;
      }
    }
    return nullptr;
  }

private:
  using allocator_type = typename std::allocator_traits<
      Allocator>::template rebind_alloc<std::pair<Key, Value>>;

  std::vector<std::pair<Key, Value>, allocator_type> values_;
};

template <typename Key, typename Value, typename Allocator>
struct index_storage<index_type::test_allocator_vector, Key, Value, Allocator> {
  index_storage(Allocator &allocator) : values_(allocator) {
    allocator.record_backend_constructed();
  }

  bool emplace(Key key, Value value) {
    for (auto &&entry : values_) {
      if (entry.first == key) {
        return false;
      }
    }
    values_.emplace_back(std::move(key), value);
    return true;
  }

  Value *find(const Key &key) {
    for (auto &&entry : values_) {
      if (entry.first == key) {
        return &entry.second;
      }
    }
    return nullptr;
  }

private:
  using allocator_type = typename std::allocator_traits<
      Allocator>::template rebind_alloc<std::pair<Key, Value>>;

  std::vector<std::pair<Key, Value>, allocator_type> values_;
};
} // namespace detail

template <typename Interface, typename IndexKey, typename IndexType>
struct dynamic_container_with_index : dynamic_container_traits {
  using index_definition_type = indexes<index<Interface, IndexKey, IndexType>>;
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

TEST(index_test, index_tag) {
  using definitions =
      indexes<index<void, short, float>, index<void, int, double>,
              index<void, std::size_t, char>>;
  static_assert(
      std::is_same_v<typename index_tag<int, definitions>::type, double>);
  static_assert(
      std::is_same_v<typename index_tag<short, definitions>::type, float>);
  static_assert(
      std::is_same_v<typename index_tag<std::size_t, definitions>::type, char>);
}

TEST(index_test, index_tag_selects_exact_interface_and_key) {
  struct indexed_interface {};

  using definitions =
      indexes<index<indexed_interface, std::size_t, index_type::array<3>>,
              index<interfaces<indexed_interface>, key<int>, index_type::map>>;

  static_assert(std::is_same_v<
                typename index_tag<type_list<indexed_interface, std::size_t>,
                                   definitions>::type,
                index_type::array<3>>);
  static_assert(
      std::is_same_v<typename index_tag<type_list<indexed_interface, int>,
                                        definitions>::type,
                     index_type::map>);
  static_assert(
      std::is_same_v<typename index_tag<std::size_t, definitions>::type, void>);
}

TEST(index_test, exception_message_index_out_of_range) {
  struct indexed_interface {
    virtual ~indexed_interface() = default;
  };
  struct indexed_class : indexed_interface {};

  using container_type =
      container<dynamic_container_with_index<indexed_interface, std::size_t,
                                             index_type::array<2>>>;

  container_type container;

  try {
    container.template register_indexed_type<
        scope<shared>, storage<std::shared_ptr<indexed_class>>,
        interfaces<indexed_interface>>(std::size_t(3));
    FAIL() << "expected type_index_out_of_range_exception";
  } catch (const type_index_out_of_range_exception &e) {
    std::string expected = "type index out of range: key type ";
    expected += type_name<std::size_t>();
    ASSERT_STREQ(e.what(), expected.c_str());
  }
}

TEST(index_test, exception_message_index_out_of_range_negative) {
  auto e = detail::make_type_index_out_of_range_exception(-1, std::size_t(2));

  std::string expected = "type index out of range: key type ";
  expected += type_name<int>();
  ASSERT_STREQ(e.what(), expected.c_str());
}

TEST(index_test, same_key_type_can_use_different_array_sizes_per_interface) {
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
    using index_definition_type =
        indexes<index<processor, std::size_t, index_type::array<3>>,
                index<animal, std::size_t, index_type::array<16>>>;
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

TEST(index_test, constructor_injects_dependencies_from_two_array_indexes) {
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
        indexes<index<source, std::size_t, index_type::array<2>>,
                index<processor, std::size_t, index_type::array<3>>>;
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
    using index_definition_type =
        indexes<index<processor, std::size_t, index_type::array<3>>,
                index<animal, std::size_t, index_type::array<16>>>;
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
    using index_definition_type =
        indexes<index<animal, std::size_t, index_type::array<3>>,
                index<animal, std::string, index_type::unordered_map>>;
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

TEST(index_test, custom_backend_extension_point_compiles_and_resolves) {
  struct animal {
    virtual ~animal() = default;
    virtual int id() const = 0;
  };
  struct dog : animal {
    int id() const override { return 11; }
  };

  struct traits : dynamic_container_traits {
    using index_definition_type =
        indexes<index<animal, std::string, index_type::test_vector>>;
  };

  container<traits> container;
  container.template register_indexed_type<scope<shared>, storage<dog>,
                                           interfaces<animal>>(
      std::string("dog"));

  ASSERT_EQ(container.template resolve<animal &>(std::string("dog")).id(), 11);
}

TEST(index_test, custom_backend_can_use_container_allocator) {
  struct animal {
    virtual ~animal() = default;
    virtual int id() const = 0;
  };
  struct dog : animal {
    int id() const override { return 11; }
  };

  struct traits : dynamic_container_traits {
    using index_definition_type =
        indexes<index<animal, std::string, index_type::test_allocator_vector>>;
  };

  test_allocator_state state;
  test_backend_allocator<char> allocator(&state);
  container<traits, test_backend_allocator<char>> container(allocator);

  container.template register_indexed_type<scope<shared>, storage<dog>,
                                           interfaces<animal>>(
      std::string("dog"));

  ASSERT_EQ(state.backend_constructed, 1);
  ASSERT_EQ(container.template resolve<animal &>(std::string("dog")).id(), 11);
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
        indexes<index<processor, key<std::string>, index_type::unordered_map>>;
  };

  container<traits> container;
  container.template register_indexed_type<
      scope<shared>, storage<json_processor>, interfaces<processor>>(
      std::string{"json"});

  auto selected = container.template construct<consumer>();

  ASSERT_EQ(selected.selected_processor.id(), 7);
}
#endif

TEST(index_test, constructor_injects_fixed_indexed_array_bindings) {
  struct traits : dynamic_container_traits {
    using index_definition_type = indexes<
        index<fixed_injection_processor, std::size_t, index_type::array<2>>>;
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

TEST(index_test, constructor_injects_fixed_indexed_map_binding) {
  struct traits : dynamic_container_traits {
    using index_definition_type =
        indexes<index<fixed_injection_processor, std::size_t, index_type::map>>;
  };

  container<traits> container;
  container.template register_indexed_type<
      scope<shared>, storage<fixed_injection_processor_impl<1>>,
      interfaces<fixed_injection_processor>>(std::size_t(1));

  auto consumer = container.template construct<fixed_injection_consumer<1>>();

  ASSERT_EQ(consumer.processor.id(), 1);
}

TEST(index_test, constructor_injects_fixed_indexed_unordered_map_binding) {
  struct traits : dynamic_container_traits {
    using index_definition_type =
        indexes<index<fixed_injection_processor, std::size_t,
                      index_type::unordered_map>>;
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
    using index_definition_type =
        indexes<index<fixed_injection_processor, fixed_injection_processor_id,
                      index_type::map>>;
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
    using index_definition_type = indexes<
        index<fixed_injection_processor, std::size_t, index_type::array<2>>>;
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
    using index_definition_type = indexes<
        index<fixed_injection_processor, std::size_t, index_type::array<2>>>;
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
    using index_definition_type = indexes<
        index<fixed_injection_processor, std::size_t, index_type::array<2>>>;
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
    using index_definition_type = indexes<
        index<annotated<fixed_injection_processor, annotated_index_primary_tag>,
              std::size_t, index_type::array<2>>,
        index<annotated<fixed_injection_processor, annotated_index_replica_tag>,
              std::size_t, index_type::array<4>>>;
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
    using index_definition_type =
        dingo::indexes<dingo::index<dingo::interfaces<string_processor>,
                                    dingo::key<std::string>,
                                    dingo::index_type::unordered_map>>;
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
