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
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
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

template <typename Container>
using lookup_identity_probe =
    detail::runtime_lookup_identity_probe<typename Container::registry_type>;

struct lookup_table_test_entry;
using lookup_table_test_rtti = rtti<typeid_provider>;
using lookup_table_test_allocator = test_allocator<char>;
using lookup_table_test_table =
    detail::runtime_lookup_table<lookup_table_test_rtti,
                                 lookup_table_test_allocator,
                                 lookup_table_test_entry>;

struct lookup_table_test_entry {
  explicit lookup_table_test_entry(
      lookup_table_test_table::lookup_identity &&resolved_identity,
      typename lookup_table_test_rtti::type_index resolved_storage_type =
          lookup_table_test_rtti::template get_type_index<
              lookup_table_test_entry>())
      : storage_type(std::move(resolved_storage_type)),
        identity(std::move(resolved_identity)) {}

  typename lookup_table_test_rtti::type_index storage_type;
  lookup_table_test_table::lookup_identity identity;
};

class prototype_lookup_entry_owner {
  using entry_allocator =
      typename std::allocator_traits<lookup_table_test_allocator>::
          template rebind_alloc<lookup_table_test_entry>;

public:
  explicit prototype_lookup_entry_owner(lookup_table_test_allocator &allocator)
      : spill_entries_(entry_allocator(allocator)) {}

  lookup_table_test_entry &emplace_uncommitted(
      lookup_table_test_table::lookup_identity &&identity,
      typename lookup_table_test_rtti::type_index storage_type) {
    if (!inline_entry_) {
      inline_entry_.emplace(std::move(identity), std::move(storage_type));
      uncommitted_entry_ = std::addressof(*inline_entry_);
      uncommitted_is_inline_ = true;
      return *uncommitted_entry_;
    }

    spill_entries_.emplace_back(std::move(identity), std::move(storage_type));
    uncommitted_entry_ = std::addressof(spill_entries_.back());
    uncommitted_is_inline_ = false;
    return *uncommitted_entry_;
  }

  void commit_uncommitted() {
    uncommitted_entry_ = nullptr;
    uncommitted_is_inline_ = false;
  }

  void rollback_uncommitted() {
    if (!uncommitted_entry_) {
      return;
    }

    if (uncommitted_is_inline_) {
      inline_entry_.reset();
    } else {
      spill_entries_.pop_back();
    }

    uncommitted_entry_ = nullptr;
    uncommitted_is_inline_ = false;
  }

  std::size_t count() const {
    return (inline_entry_ ? std::size_t{1} : std::size_t{0}) +
           spill_entries_.size();
  }

private:
  std::optional<lookup_table_test_entry> inline_entry_;
  std::vector<lookup_table_test_entry, entry_allocator> spill_entries_;
  lookup_table_test_entry *uncommitted_entry_ = nullptr;
  bool uncommitted_is_inline_ = false;
};

template <typename... LookupEntries, typename Group>
lookup_table_test_entry *prototype_register_owned_entry(
    prototype_lookup_entry_owner &owner, Group &group,
    lookup_table_test_table::lookup_identity &&identity,
    typename lookup_table_test_rtti::type_index storage_type) {
  auto &entry =
      owner.emplace_uncommitted(std::move(identity), std::move(storage_type));

  try {
    if (!group.template insert<LookupEntries...>(entry)) {
      owner.rollback_uncommitted();
      return nullptr;
    }
  } catch (...) {
    owner.rollback_uncommitted();
    throw;
  }

  owner.commit_uncommitted();
  return std::addressof(entry);
}

template <typename Container>
struct registered_storage_candidate_access : Container::registry_type {
  using registry_type = typename Container::registry_type;
  using registered_binding_entry =
      typename registry_type::registered_binding_entry;
  using lookup_table_type = typename registry_type::lookup_table_type;
};

template <typename Access, typename Container, typename Storage,
          typename LookupIdentity>
typename Access::registered_binding_entry
make_registered_binding_entry(LookupIdentity &&identity) {
  using runtime_container_type = typename Container::container_type;
  using rtti_type = typename runtime_container_type::rtti_type;
  return typename Access::registered_binding_entry(
      runtime_binding_ptr<runtime_binding_interface<runtime_container_type>>{},
      rtti_type::template get_type_index<Storage>(), std::nullopt,
      std::forward<LookupIdentity>(identity));
}

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
// std::string key and select the associative<std::string, Interface> view. It
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
    using view_definition_type = views<associative<std::size_t, processor>>;
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
    using view_definition_type = views<associative<std::size_t, processor>>;
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
    using view_definition_type = views<associative<std::size_t, processor>>;
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
    using view_definition_type =
        views<associative<std::size_t, typed_key_cached_processor>>;
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
    using view_definition_type = views<associative<std::size_t, processor>>;
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
     normal_unkeyed_singular_registration_uses_inline_entry_and_binding) {
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

    ASSERT_EQ(state.allocations, 1);
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

    ASSERT_EQ(large_state.allocations, 2);
    ASSERT_GE(large_state.largest_allocation_bytes,
              sizeof(large_processor_impl));
  }

  ASSERT_EQ(large_state.deallocations, large_state.allocations);
}

TEST(index_test, first_typed_key_singular_registration_uses_inline_entry) {
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

    ASSERT_EQ(state.allocations, 1);
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

    ASSERT_EQ(large_state.allocations, 2);
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

  container<> container;
  container.template register_type<scope<shared>,
                                   storage<typed_key_cached_processor_impl>,
                                   interfaces<typed_key_cached_processor>>();

  auto &before = container.template resolve<typed_key_cached_processor &>();

  EXPECT_THROW((container.template register_type<
                   scope<shared>, storage<typed_key_cached_processor_impl>,
                   interfaces<typed_key_cached_processor>>()),
               type_already_registered_exception);

  auto &after = container.template resolve<typed_key_cached_processor &>();
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

  EXPECT_THROW((container.template register_type<
                   scope<shared>, storage<typed_key_cached_processor_impl>,
                   interfaces<typed_key_cached_processor>>()),
               type_already_registered_exception);

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
    using view_definition_type = views<associative<std::size_t, processor>>;
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

  EXPECT_THROW(
      (container.template register_type<
          scope<shared>, storage<typed_key_cached_processor_impl>,
          interfaces<typed_key_cached_processor>, key<processor_key>>()),
      type_index_already_registered_exception);

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

TEST(index_test, implicit_unkeyed_registration_records_no_key_one) {
  runtime_container<> container;
  container.template register_type<scope<shared>,
                                   storage<typed_key_cached_processor_impl>,
                                   interfaces<typed_key_cached_processor>>();

  using probe = lookup_identity_probe<decltype(container)>;
  ASSERT_TRUE((probe::template selected_no_key_identity_matches<
               typed_key_cached_processor, typed_key_cached_processor, one>(
      container.registry())));
}

TEST(index_test, explicit_collection_registration_records_no_key_many) {
  struct traits : dynamic_container_traits {
    using view_definition_type = views<collection<typed_key_cached_processor>>;
  };

  runtime_container<traits> container;
  container.template register_type<scope<shared>,
                                   storage<typed_key_cached_processor_impl>,
                                   interfaces<typed_key_cached_processor>>();

  using probe = lookup_identity_probe<decltype(container)>;
  ASSERT_TRUE((probe::template selected_no_key_identity_matches<
               typed_key_cached_processor, typed_key_cached_processor, many>(
      container.registry())));
}

TEST(index_test, implicit_typed_key_registration_records_typed_key_one) {
  struct processor_key {};

  runtime_container<> container;
  container.template register_type<
      scope<shared>, storage<typed_key_cached_processor_impl>,
      interfaces<typed_key_cached_processor>, key<processor_key>>();

  using probe = lookup_identity_probe<decltype(container)>;
  ASSERT_TRUE(
      (probe::template selected_typed_key_identity_matches<
          typed_key_cached_processor, typed_key_cached_processor, processor_key,
          one>(container.registry(), key<processor_key>{})));
}

TEST(index_test, explicit_typed_key_many_registration_records_typed_key_many) {
  struct processor_key {};
  struct traits : dynamic_container_traits {
    using view_definition_type =
        views<typed<processor_key, typed_key_cached_processor, many>>;
  };

  runtime_container<traits> container;
  container.template register_type<
      scope<shared>, storage<typed_key_cached_processor_impl>,
      interfaces<typed_key_cached_processor>, key<processor_key>>();

  using probe = lookup_identity_probe<decltype(container)>;
  ASSERT_TRUE(
      (probe::template selected_typed_key_identity_matches<
          typed_key_cached_processor, typed_key_cached_processor, processor_key,
          many>(container.registry(), key<processor_key>{})));
}

TEST(index_test, associative_one_registration_records_runtime_key_one) {
  struct traits : dynamic_container_traits {
    using view_definition_type =
        views<associative<std::size_t, typed_key_cached_processor>>;
  };

  runtime_container<traits> container;
  container.template register_indexed_type<
      scope<shared>, storage<typed_key_cached_processor_impl>,
      interfaces<typed_key_cached_processor>>(std::size_t(7));

  using probe = lookup_identity_probe<decltype(container)>;
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
    bool operator<(const comparable_key &other) const {
      return value < other.value;
    }
  };
  struct traits : dynamic_container_traits {
    using view_definition_type =
        views<associative<comparable_key, typed_key_cached_processor>>;
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
    using view_definition_type =
        views<associative<std::size_t, typed_key_cached_processor, many>>;
  };

  runtime_container<traits> container;
  container.template register_indexed_type<
      scope<shared>, storage<typed_key_cached_processor_impl>,
      interfaces<typed_key_cached_processor>>(std::size_t(7));

  using probe = lookup_identity_probe<decltype(container)>;
  ASSERT_TRUE((probe::template selected_runtime_key_identity_matches<
               typed_key_cached_processor, typed_key_cached_processor,
               std::size_t, many>(container.registry(), std::size_t(7))));
}

TEST(index_test, lookup_table_indexes_entries_by_identity_bucket) {
  struct processor {};
  struct processor_key {};

  lookup_table_test_allocator allocator;
  lookup_table_test_table table(allocator);
  lookup_table_test_entry no_key_entry(
      lookup_table_test_table::template make_no_key_identity<processor,
                                                             many>());
  lookup_table_test_entry typed_key_entry(
      lookup_table_test_table::template make_typed_key_identity<
          processor, processor_key, many>());

  ASSERT_TRUE(table.insert(no_key_entry));
  ASSERT_TRUE(table.insert(typed_key_entry));

  bool ambiguous = false;
  auto *selected =
      table.template find_singular_no_key<processor, many>(ambiguous);
  ASSERT_FALSE(ambiguous);
  ASSERT_EQ(selected, &no_key_entry);
  ASSERT_EQ((table.template count_no_key<processor, many>()), 1U);

  std::size_t visits = 0;
  ASSERT_EQ((table.template for_each_typed_key<processor, processor_key, many>(
                [&](auto &selected_entry) {
                  ++visits;
                  ASSERT_EQ(&selected_entry, &typed_key_entry);
                })),
            1U);
  ASSERT_EQ(visits, 1U);

  table.erase(no_key_entry);
  table.erase(typed_key_entry);
  ASSERT_EQ((table.template count_no_key<processor, many>()), 0U);
  ASSERT_EQ((table.template count_typed_key<processor, processor_key, many>()),
            0U);
}

TEST(index_test,
     lookup_table_reuses_many_bucket_and_grows_rows_after_inline_capacity) {
  struct processor {};
  struct first_storage {};
  struct second_storage {};

  test_allocator_state state;
  {
    lookup_table_test_allocator allocator(&state);
    lookup_table_test_table table(allocator);
    lookup_table_test_entry first_entry(
        lookup_table_test_table::template make_no_key_identity<processor,
                                                               many>(),
        lookup_table_test_rtti::template get_type_index<first_storage>());
    lookup_table_test_entry second_entry(
        lookup_table_test_table::template make_no_key_identity<processor,
                                                               many>(),
        lookup_table_test_rtti::template get_type_index<second_storage>());

    ASSERT_TRUE(table.insert(first_entry));
    ASSERT_EQ(state.allocations, 0);

    ASSERT_TRUE(table.insert(second_entry));
    ASSERT_EQ(state.allocations, 1);
    ASSERT_EQ((table.template count_no_key<processor, many>()), 2U);
  }

  ASSERT_EQ(state.deallocations, state.allocations);
}

TEST(index_test, lookup_table_one_bucket_rejects_duplicate_without_row_growth) {
  struct processor {};
  struct first_storage {};
  struct second_storage {};

  test_allocator_state state;
  {
    lookup_table_test_allocator allocator(&state);
    lookup_table_test_table table(allocator);
    lookup_table_test_entry first_entry(
        lookup_table_test_table::template make_no_key_identity<processor,
                                                               one>(),
        lookup_table_test_rtti::template get_type_index<first_storage>());
    lookup_table_test_entry second_entry(
        lookup_table_test_table::template make_no_key_identity<processor,
                                                               one>(),
        lookup_table_test_rtti::template get_type_index<second_storage>());

    ASSERT_TRUE(table.insert(first_entry));
    ASSERT_EQ(state.allocations, 0);

    ASSERT_FALSE(table.insert(second_entry));
    ASSERT_EQ(state.allocations, 0);
    ASSERT_EQ((table.template count_no_key<processor, one>()), 1U);

    bool ambiguous = false;
    auto *selected =
        table.template find_singular_no_key<processor, one>(ambiguous);
    ASSERT_FALSE(ambiguous);
    ASSERT_EQ(selected, &first_entry);
  }

  ASSERT_EQ(state.deallocations, state.allocations);
}

TEST(index_test, selector_storage_no_key_one_rejects_duplicate_inline) {
  struct processor {};
  struct first_storage {};
  struct second_storage {};
  using lookup_entry = detail::lookup_entry<processor, no_key, one, ordered>;
  using storage_type =
      detail::selector_storage<lookup_entry, lookup_table_test_entry,
                               lookup_table_test_allocator>;

  test_allocator_state state;
  lookup_table_test_allocator allocator(&state);
  storage_type storage(allocator);
  lookup_table_test_entry first_entry(
      lookup_table_test_table::template make_no_key_identity<processor, one>(),
      lookup_table_test_rtti::template get_type_index<first_storage>());
  lookup_table_test_entry second_entry(
      lookup_table_test_table::template make_no_key_identity<processor, one>(),
      lookup_table_test_rtti::template get_type_index<second_storage>());

  ASSERT_FALSE(storage.conflicts(first_entry));
  storage.insert(first_entry);
  ASSERT_EQ(state.allocations, 0);
  ASSERT_TRUE(storage.conflicts(second_entry));
  ASSERT_EQ(storage.count(), 1U);

  bool ambiguous = false;
  auto *selected = storage.find_singular(ambiguous);
  ASSERT_FALSE(ambiguous);
  ASSERT_EQ(selected, &first_entry);
}

TEST(index_test, selector_storage_typed_key_many_tracks_ambiguity_and_storage) {
  struct processor {};
  struct processor_key {};
  struct first_storage {};
  struct second_storage {};
  using lookup_entry =
      detail::lookup_entry<processor, typed_key<processor_key>, many, ordered>;
  using storage_type =
      detail::selector_storage<lookup_entry, lookup_table_test_entry,
                               lookup_table_test_allocator>;

  test_allocator_state state;
  lookup_table_test_allocator allocator(&state);
  storage_type storage(allocator);
  lookup_table_test_entry first_entry(
      lookup_table_test_table::template make_typed_key_identity<
          processor, processor_key, many>(),
      lookup_table_test_rtti::template get_type_index<first_storage>());
  lookup_table_test_entry duplicate_storage_entry(
      lookup_table_test_table::template make_typed_key_identity<
          processor, processor_key, many>(),
      lookup_table_test_rtti::template get_type_index<first_storage>());
  lookup_table_test_entry second_entry(
      lookup_table_test_table::template make_typed_key_identity<
          processor, processor_key, many>(),
      lookup_table_test_rtti::template get_type_index<second_storage>());

  storage.insert(first_entry);
  ASSERT_EQ(state.allocations, 0);
  ASSERT_TRUE(storage.conflicts(duplicate_storage_entry));
  ASSERT_FALSE(storage.conflicts(second_entry));

  storage.insert(second_entry);
  ASSERT_EQ(state.allocations, 1);
  ASSERT_EQ(storage.count(), 2U);

  bool ambiguous = false;
  ASSERT_EQ(storage.find_singular(ambiguous), nullptr);
  ASSERT_TRUE(ambiguous);

  std::size_t visits = 0;
  ASSERT_EQ(storage.for_each([&](auto &entry) {
    ++visits;
    ASSERT_TRUE(&entry == &first_entry || &entry == &second_entry);
  }),
            2U);
  ASSERT_EQ(visits, 2U);

  storage.erase(first_entry);
  ASSERT_EQ(storage.count(), 1U);
  ambiguous = true;
  ASSERT_EQ(storage.find_singular(ambiguous), &second_entry);
  ASSERT_FALSE(ambiguous);
}

TEST(index_test, selector_table_no_key_one_selects_single_entry) {
  struct processor {};
  struct first_storage {};
  struct second_storage {};

  using no_key_one_entry =
      detail::lookup_entry<processor, no_key, one, ordered>;
  using table_type =
      detail::selector_table<lookup_table_test_entry,
                             lookup_table_test_allocator, no_key_one_entry>;

  lookup_table_test_allocator allocator;
  table_type table(allocator);
  lookup_table_test_entry first_entry(
      lookup_table_test_table::template make_no_key_identity<processor, one>(),
      lookup_table_test_rtti::template get_type_index<first_storage>());
  lookup_table_test_entry second_entry(
      lookup_table_test_table::template make_no_key_identity<processor, one>(),
      lookup_table_test_rtti::template get_type_index<second_storage>());

  ASSERT_FALSE(table.template conflicts<no_key_one_entry>(first_entry));
  table.template insert<no_key_one_entry>(first_entry);
  ASSERT_TRUE(table.template conflicts<no_key_one_entry>(second_entry));
  ASSERT_EQ(table.template count<no_key_one_entry>(), 1U);

  bool ambiguous = true;
  ASSERT_EQ(table.template find_singular<no_key_one_entry>(ambiguous),
            &first_entry);
  ASSERT_FALSE(ambiguous);

  table.template erase<no_key_one_entry>(first_entry);
  ASSERT_EQ(table.template count<no_key_one_entry>(), 0U);
}

TEST(index_test, selector_table_no_key_many_reports_ambiguity) {
  struct processor {};
  struct first_storage {};
  struct second_storage {};

  using no_key_many_entry =
      detail::lookup_entry<processor, no_key, many, ordered>;
  using table_type =
      detail::selector_table<lookup_table_test_entry,
                             lookup_table_test_allocator, no_key_many_entry>;

  lookup_table_test_allocator allocator;
  table_type table(allocator);
  lookup_table_test_entry first_entry(
      lookup_table_test_table::template make_no_key_identity<processor, many>(),
      lookup_table_test_rtti::template get_type_index<first_storage>());
  lookup_table_test_entry second_entry(
      lookup_table_test_table::template make_no_key_identity<processor, many>(),
      lookup_table_test_rtti::template get_type_index<second_storage>());

  table.template insert<no_key_many_entry>(first_entry);
  ASSERT_FALSE(table.template conflicts<no_key_many_entry>(second_entry));
  table.template insert<no_key_many_entry>(second_entry);
  ASSERT_EQ(table.template count<no_key_many_entry>(), 2U);

  bool ambiguous = false;
  ASSERT_EQ(table.template find_singular<no_key_many_entry>(ambiguous),
            nullptr);
  ASSERT_TRUE(ambiguous);
}

TEST(index_test, selector_table_typed_key_one_and_many_are_distinct_rows) {
  struct processor {};
  struct processor_key {};
  struct first_storage {};
  struct second_storage {};
  struct third_storage {};

  using typed_key_one_entry =
      detail::lookup_entry<processor, typed_key<processor_key>, one, ordered>;
  using typed_key_many_entry =
      detail::lookup_entry<processor, typed_key<processor_key>, many, ordered>;
  using table_type =
      detail::selector_table<lookup_table_test_entry,
                             lookup_table_test_allocator, typed_key_one_entry,
                             typed_key_many_entry>;

  lookup_table_test_allocator allocator;
  table_type table(allocator);
  lookup_table_test_entry one_entry(
      lookup_table_test_table::template make_typed_key_identity<
          processor, processor_key, one>(),
      lookup_table_test_rtti::template get_type_index<first_storage>());
  lookup_table_test_entry first_many_entry(
      lookup_table_test_table::template make_typed_key_identity<
          processor, processor_key, many>(),
      lookup_table_test_rtti::template get_type_index<second_storage>());
  lookup_table_test_entry second_many_entry(
      lookup_table_test_table::template make_typed_key_identity<
          processor, processor_key, many>(),
      lookup_table_test_rtti::template get_type_index<third_storage>());

  table.template insert<typed_key_one_entry>(one_entry);
  table.template insert<typed_key_many_entry>(first_many_entry);
  table.template insert<typed_key_many_entry>(second_many_entry);

  ASSERT_EQ(table.template count<typed_key_one_entry>(), 1U);
  ASSERT_EQ(table.template count<typed_key_many_entry>(), 2U);

  bool ambiguous = true;
  ASSERT_EQ(table.template find_singular<typed_key_one_entry>(ambiguous),
            &one_entry);
  ASSERT_FALSE(ambiguous);

  ambiguous = false;
  ASSERT_EQ(table.template find_singular<typed_key_many_entry>(ambiguous),
            nullptr);
  ASSERT_TRUE(ambiguous);
}

TEST(index_test, selector_table_keeps_different_typed_keys_separate) {
  struct processor {};
  struct first_key {};
  struct second_key {};
  struct first_storage {};
  struct second_storage {};

  using first_lookup_entry =
      detail::lookup_entry<processor, typed_key<first_key>, many, ordered>;
  using second_lookup_entry =
      detail::lookup_entry<processor, typed_key<second_key>, many, ordered>;
  using table_type =
      detail::selector_table<lookup_table_test_entry,
                             lookup_table_test_allocator, first_lookup_entry,
                             second_lookup_entry>;

  lookup_table_test_allocator allocator;
  table_type table(allocator);
  lookup_table_test_entry first_entry(
      lookup_table_test_table::template make_typed_key_identity<
          processor, first_key, many>(),
      lookup_table_test_rtti::template get_type_index<first_storage>());
  lookup_table_test_entry second_entry(
      lookup_table_test_table::template make_typed_key_identity<
          processor, second_key, many>(),
      lookup_table_test_rtti::template get_type_index<second_storage>());

  table.template insert<first_lookup_entry>(first_entry);
  ASSERT_EQ(table.template count<first_lookup_entry>(), 1U);
  ASSERT_EQ(table.template count<second_lookup_entry>(), 0U);

  table.template insert<second_lookup_entry>(second_entry);
  ASSERT_EQ(table.template count<first_lookup_entry>(), 1U);
  ASSERT_EQ(table.template count<second_lookup_entry>(), 1U);

  std::size_t visits = 0;
  ASSERT_EQ(
      (table.template for_each<first_lookup_entry>([&](auto &selected_entry) {
        ++visits;
        ASSERT_EQ(&selected_entry, &first_entry);
      })),
      1U);
  ASSERT_EQ(visits, 1U);
}

TEST(index_test, selector_table_for_each_and_count_cover_many_rows) {
  struct processor {};
  struct first_storage {};
  struct second_storage {};
  struct third_storage {};

  using lookup_entry = detail::lookup_entry<processor, no_key, many, ordered>;
  using table_type =
      detail::selector_table<lookup_table_test_entry,
                             lookup_table_test_allocator, lookup_entry>;

  lookup_table_test_allocator allocator;
  table_type table(allocator);
  lookup_table_test_entry first_entry(
      lookup_table_test_table::template make_no_key_identity<processor, many>(),
      lookup_table_test_rtti::template get_type_index<first_storage>());
  lookup_table_test_entry second_entry(
      lookup_table_test_table::template make_no_key_identity<processor, many>(),
      lookup_table_test_rtti::template get_type_index<second_storage>());
  lookup_table_test_entry third_entry(
      lookup_table_test_table::template make_no_key_identity<processor, many>(),
      lookup_table_test_rtti::template get_type_index<third_storage>());

  table.template insert<lookup_entry>(first_entry);
  table.template insert<lookup_entry>(second_entry);
  table.template insert<lookup_entry>(third_entry);

  std::size_t visits = 0;
  ASSERT_EQ(table.template for_each<lookup_entry>([&](auto &entry) {
    ++visits;
    ASSERT_TRUE(&entry == &first_entry || &entry == &second_entry ||
                &entry == &third_entry);
  }),
            3U);
  ASSERT_EQ(visits, 3U);
  ASSERT_EQ(table.template count<lookup_entry>(), 3U);
}

TEST(index_test, selector_table_accepts_type_list_lookup_entries) {
  struct processor {};
  struct processor_key {};
  struct no_key_storage {};
  struct typed_key_storage {};

  using no_key_lookup_entry =
      detail::lookup_entry<processor, no_key, one, ordered>;
  using typed_key_lookup_entry =
      detail::lookup_entry<processor, typed_key<processor_key>, many, ordered>;
  using lookup_entries = type_list<no_key_lookup_entry, typed_key_lookup_entry>;
  using table_type =
      detail::selector_table<lookup_table_test_entry,
                             lookup_table_test_allocator, lookup_entries>;

  static_assert(
      std::is_constructible_v<table_type, lookup_table_test_allocator &>);

  lookup_table_test_allocator allocator;
  table_type table(allocator);
  lookup_table_test_entry no_key_entry(
      lookup_table_test_table::template make_no_key_identity<processor, one>(),
      lookup_table_test_rtti::template get_type_index<no_key_storage>());
  lookup_table_test_entry typed_key_entry(
      lookup_table_test_table::template make_typed_key_identity<
          processor, processor_key, many>(),
      lookup_table_test_rtti::template get_type_index<typed_key_storage>());

  table.template insert<no_key_lookup_entry>(no_key_entry);
  table.template insert<typed_key_lookup_entry>(typed_key_entry);

  ASSERT_EQ(table.template count<no_key_lookup_entry>(), 1U);
  ASSERT_EQ(table.template count<typed_key_lookup_entry>(), 1U);

  bool ambiguous = true;
  ASSERT_EQ(table.template find_singular<no_key_lookup_entry>(ambiguous),
            &no_key_entry);
  ASSERT_FALSE(ambiguous);
}

TEST(index_test, ordinary_selector_materialization_builds_lookup_entries) {
  struct processor {};
  struct other_processor {};
  struct processor_key {};
  struct other_key {};

  using implicit_no_key_one =
      detail::lookup_entry<processor, no_key, one, detail::no_lookup_backend>;
  using no_key_many = detail::lookup_entry<processor, no_key, many, ordered>;
  using typed_key_one =
      detail::lookup_entry<processor, typed_key<processor_key>, one, ordered>;
  using typed_key_many =
      detail::lookup_entry<processor, typed_key<processor_key>, many, ordered>;
  using implicit_typed_key_one =
      detail::lookup_entry<processor, typed_key<processor_key>, one,
                           detail::no_lookup_backend>;
  using other_interface_no_key_one =
      detail::lookup_entry<other_processor, no_key, one,
                           detail::no_lookup_backend>;
  using other_key_typed_key_one =
      detail::lookup_entry<processor, typed_key<other_key>, one,
                           detail::no_lookup_backend>;

  using empty_entries = type_list<>;

  using implicit_no_key_entries =
      detail::materialized_ordinary_selector_entries_t<
          empty_entries,
          type_list<detail::ordinary_selector_request<processor, no_key>>>;
  static_assert(
      std::is_same_v<implicit_no_key_entries, type_list<implicit_no_key_one>>);

  using explicit_no_key_many_entries =
      detail::materialized_ordinary_selector_entries_t<
          type_list<no_key_many>,
          type_list<detail::ordinary_selector_request<processor, no_key>>>;
  static_assert(
      std::is_same_v<explicit_no_key_many_entries, type_list<no_key_many>>);

  using explicit_typed_key_entries =
      detail::materialized_ordinary_selector_entries_t<
          type_list<typed_key_many, typed_key_one>,
          type_list<detail::ordinary_selector_request<
              processor, typed_key<processor_key>>>>;
  static_assert(
      std::is_same_v<explicit_typed_key_entries, type_list<typed_key_one>>);

  using implicit_typed_key_entries =
      detail::materialized_ordinary_selector_entries_t<
          empty_entries, type_list<detail::ordinary_selector_request<
                             processor, typed_key<processor_key>>>>;
  static_assert(std::is_same_v<implicit_typed_key_entries,
                               type_list<implicit_typed_key_one>>);

  using duplicate_request_entries =
      detail::materialized_ordinary_selector_entries_t<
          empty_entries,
          type_list<detail::ordinary_selector_request<processor, no_key>,
                    detail::ordinary_selector_request<processor, no_key>>>;
  static_assert(type_list_size_v<duplicate_request_entries> == 1U);
  static_assert(
      type_list_contains_v<implicit_no_key_one, duplicate_request_entries>);

  using separate_request_entries =
      detail::materialized_ordinary_selector_entries_t<
          empty_entries,
          type_list<detail::ordinary_selector_request<processor, no_key>,
                    detail::ordinary_selector_request<other_processor, no_key>,
                    detail::ordinary_selector_request<processor,
                                                      typed_key<processor_key>>,
                    detail::ordinary_selector_request<processor,
                                                      typed_key<other_key>>>>;
  static_assert(type_list_size_v<separate_request_entries> == 4U);
  static_assert(
      type_list_contains_v<implicit_no_key_one, separate_request_entries>);
  static_assert(type_list_contains_v<other_interface_no_key_one,
                                     separate_request_entries>);
  static_assert(
      type_list_contains_v<implicit_typed_key_one, separate_request_entries>);
  static_assert(
      type_list_contains_v<other_key_typed_key_one, separate_request_entries>);
}

TEST(index_test, materialized_ordinary_selector_table_uses_selected_entries) {
  struct processor {};
  struct processor_key {};
  struct other_key {};
  struct first_storage {};
  struct second_storage {};
  struct third_storage {};

  using empty_entries = type_list<>;
  using implicit_no_key_one = detail::materialized_ordinary_selector_entry_t<
      empty_entries, detail::ordinary_selector_request<processor, no_key>>;
  using implicit_no_key_table = detail::materialized_ordinary_selector_table_t<
      lookup_table_test_entry, lookup_table_test_allocator, empty_entries,
      type_list<detail::ordinary_selector_request<processor, no_key>>>;

  lookup_table_test_allocator allocator;
  implicit_no_key_table implicit_table(allocator);
  lookup_table_test_entry no_key_entry(
      lookup_table_test_table::template make_no_key_identity<processor, one>(),
      lookup_table_test_rtti::template get_type_index<first_storage>());

  implicit_table.template insert<implicit_no_key_one>(no_key_entry);

  bool ambiguous = true;
  ASSERT_EQ(
      implicit_table.template find_singular<implicit_no_key_one>(ambiguous),
      &no_key_entry);
  ASSERT_FALSE(ambiguous);

  using explicit_no_key_many =
      detail::lookup_entry<processor, no_key, many, ordered>;
  using explicit_no_key_many_table =
      detail::materialized_ordinary_selector_table_t<
          lookup_table_test_entry, lookup_table_test_allocator,
          type_list<explicit_no_key_many>,
          type_list<detail::ordinary_selector_request<processor, no_key>>>;

  explicit_no_key_many_table many_table(allocator);
  lookup_table_test_entry first_many_entry(
      lookup_table_test_table::template make_no_key_identity<processor, many>(),
      lookup_table_test_rtti::template get_type_index<first_storage>());
  lookup_table_test_entry second_many_entry(
      lookup_table_test_table::template make_no_key_identity<processor, many>(),
      lookup_table_test_rtti::template get_type_index<second_storage>());

  many_table.template insert<explicit_no_key_many>(first_many_entry);
  many_table.template insert<explicit_no_key_many>(second_many_entry);
  ASSERT_EQ(many_table.template count<explicit_no_key_many>(), 2U);
  ambiguous = false;
  ASSERT_EQ(many_table.template find_singular<explicit_no_key_many>(ambiguous),
            nullptr);
  ASSERT_TRUE(ambiguous);

  using processor_key_entry = detail::materialized_ordinary_selector_entry_t<
      empty_entries,
      detail::ordinary_selector_request<processor, typed_key<processor_key>>>;
  using other_key_entry = detail::materialized_ordinary_selector_entry_t<
      empty_entries,
      detail::ordinary_selector_request<processor, typed_key<other_key>>>;
  using typed_key_table = detail::materialized_ordinary_selector_table_t<
      lookup_table_test_entry, lookup_table_test_allocator, empty_entries,
      type_list<
          detail::ordinary_selector_request<processor,
                                            typed_key<processor_key>>,
          detail::ordinary_selector_request<processor, typed_key<other_key>>>>;

  typed_key_table typed_table(allocator);
  lookup_table_test_entry processor_key_lookup_entry(
      lookup_table_test_table::template make_typed_key_identity<
          processor, processor_key, one>(),
      lookup_table_test_rtti::template get_type_index<first_storage>());
  lookup_table_test_entry other_key_lookup_entry(
      lookup_table_test_table::template make_typed_key_identity<
          processor, other_key, one>(),
      lookup_table_test_rtti::template get_type_index<second_storage>());

  typed_table.template insert<processor_key_entry>(processor_key_lookup_entry);
  typed_table.template insert<other_key_entry>(other_key_lookup_entry);
  ASSERT_EQ(typed_table.template count<processor_key_entry>(), 1U);
  ASSERT_EQ(typed_table.template count<other_key_entry>(), 1U);

  using duplicate_request_table =
      detail::materialized_ordinary_selector_table_t<
          lookup_table_test_entry, lookup_table_test_allocator, empty_entries,
          type_list<detail::ordinary_selector_request<processor, no_key>,
                    detail::ordinary_selector_request<processor, no_key>>>;

  static_assert(std::is_constructible_v<duplicate_request_table,
                                        lookup_table_test_allocator &>);

  duplicate_request_table duplicate_table(allocator);
  lookup_table_test_entry duplicate_entry(
      lookup_table_test_table::template make_no_key_identity<processor, one>(),
      lookup_table_test_rtti::template get_type_index<third_storage>());

  duplicate_table.template insert<implicit_no_key_one>(duplicate_entry);
  ASSERT_EQ(duplicate_table.template count<implicit_no_key_one>(), 1U);
}

TEST(index_test, ordinary_registration_selectors_collect_requests) {
  struct processor {};
  struct other_processor {};
  struct processor_key {};
  struct other_key {};

  using no_key_request = detail::ordinary_selector_request<processor, no_key>;
  using typed_key_request =
      detail::ordinary_selector_request<processor, typed_key<processor_key>>;
  using other_interface_request =
      detail::ordinary_selector_request<other_processor, no_key>;
  using other_key_request =
      detail::ordinary_selector_request<processor, typed_key<other_key>>;

  using no_key_requests = detail::ordinary_selector_requests_t<
      type_list<detail::ordinary_registration_selector<processor, no_key>>>;
  static_assert(std::is_same_v<no_key_requests, type_list<no_key_request>>);

  using typed_key_requests = detail::ordinary_selector_requests_t<
      type_list<detail::ordinary_registration_selector<
          processor, typed_key<processor_key>>>>;
  static_assert(
      std::is_same_v<typed_key_requests, type_list<typed_key_request>>);

  using duplicate_requests = detail::ordinary_selector_requests_t<
      type_list<detail::ordinary_registration_selector<processor, no_key>,
                detail::ordinary_registration_selector<processor, no_key>>>;
  static_assert(type_list_size_v<duplicate_requests> == 1U);
  static_assert(type_list_contains_v<no_key_request, duplicate_requests>);

  using separate_interface_requests =
      detail::ordinary_selector_requests_t<type_list<
          detail::ordinary_registration_selector<processor, no_key>,
          detail::ordinary_registration_selector<other_processor, no_key>>>;
  static_assert(type_list_size_v<separate_interface_requests> == 2U);
  static_assert(
      type_list_contains_v<no_key_request, separate_interface_requests>);
  static_assert(type_list_contains_v<other_interface_request,
                                     separate_interface_requests>);

  using separate_key_requests = detail::ordinary_selector_requests_t<type_list<
      detail::ordinary_registration_selector<processor,
                                             typed_key<processor_key>>,
      detail::ordinary_registration_selector<processor, typed_key<other_key>>>>;
  static_assert(type_list_size_v<separate_key_requests> == 2U);
  static_assert(type_list_contains_v<typed_key_request, separate_key_requests>);
  static_assert(type_list_contains_v<other_key_request, separate_key_requests>);

  using collected_requests = detail::ordinary_selector_requests_t<
      type_list<detail::ordinary_registration_selector<processor, no_key>,
                detail::ordinary_registration_selector<
                    processor, typed_key<processor_key>>>>;
  using materialized_entries =
      detail::materialized_ordinary_selector_entries_t<type_list<>,
                                                       collected_requests>;
  using implicit_no_key_one =
      detail::lookup_entry<processor, no_key, one, detail::no_lookup_backend>;
  using implicit_typed_key_one =
      detail::lookup_entry<processor, typed_key<processor_key>, one,
                           detail::no_lookup_backend>;

  static_assert(type_list_size_v<materialized_entries> == 2U);
  static_assert(
      type_list_contains_v<implicit_no_key_one, materialized_entries>);
  static_assert(
      type_list_contains_v<implicit_typed_key_one, materialized_entries>);

  using table_type = detail::materialized_ordinary_selector_table_t<
      lookup_table_test_entry, lookup_table_test_allocator, type_list<>,
      collected_requests>;
  static_assert(
      std::is_constructible_v<table_type, lookup_table_test_allocator &>);
}

TEST(index_test, ordinary_selector_prototype_pipeline_indexes_rows) {
  struct collection_processor {};
  struct single_processor {};
  struct typed_processor {};
  struct first_key {};
  struct second_key {};
  struct first_storage {};
  struct second_storage {};
  struct third_storage {};
  struct fourth_storage {};

  using explicit_collection_entry =
      detail::lookup_entry<collection_processor, no_key, many, ordered>;
  using explicit_entries = type_list<explicit_collection_entry>;
  using registration_selectors = type_list<
      detail::ordinary_registration_selector<collection_processor, no_key>,
      detail::ordinary_registration_selector<collection_processor, no_key>,
      detail::ordinary_registration_selector<single_processor, no_key>,
      detail::ordinary_registration_selector<typed_processor,
                                             typed_key<first_key>>,
      detail::ordinary_registration_selector<typed_processor,
                                             typed_key<second_key>>>;
  using requests = detail::ordinary_selector_requests_t<registration_selectors>;
  using materialized_entries =
      detail::materialized_ordinary_selector_entries_t<explicit_entries,
                                                       requests>;
  using implicit_single_entry = detail::materialized_ordinary_selector_entry_t<
      explicit_entries,
      detail::ordinary_selector_request<single_processor, no_key>>;
  using first_key_entry = detail::materialized_ordinary_selector_entry_t<
      explicit_entries,
      detail::ordinary_selector_request<typed_processor, typed_key<first_key>>>;
  using second_key_entry = detail::materialized_ordinary_selector_entry_t<
      explicit_entries, detail::ordinary_selector_request<
                            typed_processor, typed_key<second_key>>>;
  using table_type = detail::materialized_ordinary_selector_table_t<
      lookup_table_test_entry, lookup_table_test_allocator, explicit_entries,
      requests>;

  static_assert(type_list_size_v<requests> == 4U);
  static_assert(type_list_size_v<materialized_entries> == 4U);
  static_assert(
      type_list_contains_v<explicit_collection_entry, materialized_entries>);
  static_assert(
      type_list_contains_v<implicit_single_entry, materialized_entries>);
  static_assert(type_list_contains_v<first_key_entry, materialized_entries>);
  static_assert(type_list_contains_v<second_key_entry, materialized_entries>);

  lookup_table_test_allocator allocator;
  table_type table(allocator);
  lookup_table_test_entry first_collection_entry(
      lookup_table_test_table::template make_no_key_identity<
          collection_processor, many>(),
      lookup_table_test_rtti::template get_type_index<first_storage>());
  lookup_table_test_entry second_collection_entry(
      lookup_table_test_table::template make_no_key_identity<
          collection_processor, many>(),
      lookup_table_test_rtti::template get_type_index<second_storage>());
  lookup_table_test_entry first_single_entry(
      lookup_table_test_table::template make_no_key_identity<single_processor,
                                                             one>(),
      lookup_table_test_rtti::template get_type_index<third_storage>());
  lookup_table_test_entry second_single_entry(
      lookup_table_test_table::template make_no_key_identity<single_processor,
                                                             one>(),
      lookup_table_test_rtti::template get_type_index<fourth_storage>());
  lookup_table_test_entry first_key_lookup_entry(
      lookup_table_test_table::template make_typed_key_identity<
          typed_processor, first_key, one>(),
      lookup_table_test_rtti::template get_type_index<first_storage>());
  lookup_table_test_entry second_key_lookup_entry(
      lookup_table_test_table::template make_typed_key_identity<
          typed_processor, second_key, one>(),
      lookup_table_test_rtti::template get_type_index<second_storage>());

  table.template insert<explicit_collection_entry>(first_collection_entry);
  ASSERT_FALSE(table.template conflicts<explicit_collection_entry>(
      second_collection_entry));
  table.template insert<explicit_collection_entry>(second_collection_entry);

  ASSERT_EQ(table.template count<explicit_collection_entry>(), 2U);
  bool ambiguous = false;
  ASSERT_EQ(table.template find_singular<explicit_collection_entry>(ambiguous),
            nullptr);
  ASSERT_TRUE(ambiguous);

  std::size_t visits = 0;
  ASSERT_EQ(
      table.template for_each<explicit_collection_entry>([&](auto &entry) {
        ++visits;
        ASSERT_TRUE(&entry == &first_collection_entry ||
                    &entry == &second_collection_entry);
      }),
      2U);
  ASSERT_EQ(visits, 2U);

  ASSERT_EQ(table.template count<implicit_single_entry>(), 0U);
  table.template insert<implicit_single_entry>(first_single_entry);
  ASSERT_TRUE(
      table.template conflicts<implicit_single_entry>(second_single_entry));
  ASSERT_EQ(table.template count<implicit_single_entry>(), 1U);

  ambiguous = true;
  ASSERT_EQ(table.template find_singular<implicit_single_entry>(ambiguous),
            &first_single_entry);
  ASSERT_FALSE(ambiguous);

  table.template insert<first_key_entry>(first_key_lookup_entry);
  ASSERT_EQ(table.template count<first_key_entry>(), 1U);
  ASSERT_EQ(table.template count<second_key_entry>(), 0U);

  table.template insert<second_key_entry>(second_key_lookup_entry);
  ASSERT_EQ(table.template count<first_key_entry>(), 1U);
  ASSERT_EQ(table.template count<second_key_entry>(), 1U);

  ambiguous = true;
  ASSERT_EQ(table.template find_singular<first_key_entry>(ambiguous),
            &first_key_lookup_entry);
  ASSERT_FALSE(ambiguous);
  ambiguous = true;
  ASSERT_EQ(table.template find_singular<second_key_entry>(ambiguous),
            &second_key_lookup_entry);
  ASSERT_FALSE(ambiguous);
}

TEST(index_test,
     materialized_ordinary_selector_group_indexes_one_entry_in_multiple_rows) {
  struct collection_processor {};
  struct typed_processor {};
  struct processor_key {};
  struct first_storage {};

  using no_key_many_entry =
      detail::lookup_entry<collection_processor, no_key, many, ordered>;
  using typed_key_one_entry = detail::materialized_ordinary_selector_entry_t<
      type_list<no_key_many_entry>,
      detail::ordinary_selector_request<typed_processor,
                                        typed_key<processor_key>>>;
  using group_type = detail::materialized_ordinary_selector_group<
      lookup_table_test_entry, lookup_table_test_allocator,
      type_list<no_key_many_entry>,
      type_list<detail::ordinary_selector_request<collection_processor, no_key>,
                detail::ordinary_selector_request<typed_processor,
                                                  typed_key<processor_key>>>>;

  lookup_table_test_allocator allocator;
  group_type group(allocator);
  lookup_table_test_entry entry(
      lookup_table_test_table::template make_no_key_identity<
          collection_processor, many>(),
      lookup_table_test_rtti::template get_type_index<first_storage>());

  ASSERT_TRUE(
      (group.template insert<no_key_many_entry, typed_key_one_entry>(entry)));

  ASSERT_EQ(group.template count<no_key_many_entry>(), 1U);
  ASSERT_EQ(group.template count<typed_key_one_entry>(), 1U);

  bool ambiguous = true;
  ASSERT_EQ(group.template find_singular<typed_key_one_entry>(ambiguous),
            &entry);
  ASSERT_FALSE(ambiguous);

  std::size_t visits = 0;
  ASSERT_EQ(group.template for_each<no_key_many_entry>([&](auto &selected) {
    ++visits;
    ASSERT_EQ(&selected, &entry);
  }),
            1U);
  ASSERT_EQ(visits, 1U);
}

TEST(index_test,
     materialized_ordinary_selector_group_rolls_back_on_later_conflict) {
  struct collection_processor {};
  struct typed_processor {};
  struct processor_key {};
  struct first_storage {};
  struct second_storage {};

  using no_key_many_entry =
      detail::lookup_entry<collection_processor, no_key, many, ordered>;
  using typed_key_one_entry = detail::materialized_ordinary_selector_entry_t<
      type_list<no_key_many_entry>,
      detail::ordinary_selector_request<typed_processor,
                                        typed_key<processor_key>>>;
  using group_type = detail::materialized_ordinary_selector_group<
      lookup_table_test_entry, lookup_table_test_allocator,
      type_list<no_key_many_entry>,
      type_list<detail::ordinary_selector_request<collection_processor, no_key>,
                detail::ordinary_selector_request<typed_processor,
                                                  typed_key<processor_key>>>>;

  lookup_table_test_allocator allocator;
  group_type group(allocator);
  lookup_table_test_entry existing_typed_entry(
      lookup_table_test_table::template make_typed_key_identity<
          typed_processor, processor_key, one>(),
      lookup_table_test_rtti::template get_type_index<first_storage>());
  lookup_table_test_entry candidate_entry(
      lookup_table_test_table::template make_no_key_identity<
          collection_processor, many>(),
      lookup_table_test_rtti::template get_type_index<second_storage>());

  ASSERT_TRUE(group.template insert<typed_key_one_entry>(existing_typed_entry));
  ASSERT_FALSE((group.template insert<no_key_many_entry, typed_key_one_entry>(
      candidate_entry)));

  ASSERT_EQ(group.template count<no_key_many_entry>(), 0U);
  ASSERT_EQ(group.template count<typed_key_one_entry>(), 1U);

  bool ambiguous = true;
  ASSERT_EQ(group.template find_singular<typed_key_one_entry>(ambiguous),
            &existing_typed_entry);
  ASSERT_FALSE(ambiguous);
}

TEST(index_test, materialized_ordinary_selector_group_erases_inserted_rows) {
  struct collection_processor {};
  struct typed_processor {};
  struct processor_key {};
  struct first_storage {};

  using no_key_many_entry =
      detail::lookup_entry<collection_processor, no_key, many, ordered>;
  using typed_key_one_entry = detail::materialized_ordinary_selector_entry_t<
      type_list<no_key_many_entry>,
      detail::ordinary_selector_request<typed_processor,
                                        typed_key<processor_key>>>;
  using group_type = detail::materialized_ordinary_selector_group<
      lookup_table_test_entry, lookup_table_test_allocator,
      type_list<no_key_many_entry>,
      type_list<detail::ordinary_selector_request<collection_processor, no_key>,
                detail::ordinary_selector_request<typed_processor,
                                                  typed_key<processor_key>>>>;

  lookup_table_test_allocator allocator;
  group_type group(allocator);
  lookup_table_test_entry entry(
      lookup_table_test_table::template make_no_key_identity<
          collection_processor, many>(),
      lookup_table_test_rtti::template get_type_index<first_storage>());

  ASSERT_TRUE(
      (group.template insert<no_key_many_entry, typed_key_one_entry>(entry)));
  group.template erase<no_key_many_entry, typed_key_one_entry>(entry);

  ASSERT_EQ(group.template count<no_key_many_entry>(), 0U);
  ASSERT_EQ(group.template count<typed_key_one_entry>(), 0U);

  bool ambiguous = true;
  ASSERT_EQ(group.template find_singular<typed_key_one_entry>(ambiguous),
            nullptr);
  ASSERT_FALSE(ambiguous);
}

TEST(index_test,
     materialized_ordinary_selector_group_keeps_interfaces_and_keys_isolated) {
  struct first_interface {};
  struct second_interface {};
  struct keyed_interface {};
  struct first_key {};
  struct second_key {};
  struct first_storage {};
  struct second_storage {};
  struct third_storage {};

  using first_interface_entry = detail::materialized_ordinary_selector_entry_t<
      type_list<>, detail::ordinary_selector_request<first_interface, no_key>>;
  using second_interface_entry = detail::materialized_ordinary_selector_entry_t<
      type_list<>, detail::ordinary_selector_request<second_interface, no_key>>;
  using first_key_entry = detail::materialized_ordinary_selector_entry_t<
      type_list<>,
      detail::ordinary_selector_request<keyed_interface, typed_key<first_key>>>;
  using second_key_entry = detail::materialized_ordinary_selector_entry_t<
      type_list<>, detail::ordinary_selector_request<keyed_interface,
                                                     typed_key<second_key>>>;
  using group_type = detail::materialized_ordinary_selector_group<
      lookup_table_test_entry, lookup_table_test_allocator, type_list<>,
      type_list<detail::ordinary_selector_request<first_interface, no_key>,
                detail::ordinary_selector_request<second_interface, no_key>,
                detail::ordinary_selector_request<keyed_interface,
                                                  typed_key<first_key>>,
                detail::ordinary_selector_request<keyed_interface,
                                                  typed_key<second_key>>>>;

  lookup_table_test_allocator allocator;
  group_type group(allocator);
  lookup_table_test_entry first_interface_entry_value(
      lookup_table_test_table::template make_no_key_identity<first_interface,
                                                             one>(),
      lookup_table_test_rtti::template get_type_index<first_storage>());
  lookup_table_test_entry first_key_entry_value(
      lookup_table_test_table::template make_typed_key_identity<
          keyed_interface, first_key, one>(),
      lookup_table_test_rtti::template get_type_index<second_storage>());
  lookup_table_test_entry second_key_entry_value(
      lookup_table_test_table::template make_typed_key_identity<
          keyed_interface, second_key, one>(),
      lookup_table_test_rtti::template get_type_index<third_storage>());

  ASSERT_TRUE(group.template insert<first_interface_entry>(
      first_interface_entry_value));
  ASSERT_TRUE(group.template insert<first_key_entry>(first_key_entry_value));
  ASSERT_TRUE(group.template insert<second_key_entry>(second_key_entry_value));

  ASSERT_EQ(group.template count<first_interface_entry>(), 1U);
  ASSERT_EQ(group.template count<second_interface_entry>(), 0U);
  ASSERT_EQ(group.template count<first_key_entry>(), 1U);
  ASSERT_EQ(group.template count<second_key_entry>(), 1U);

  bool ambiguous = true;
  ASSERT_EQ(group.template find_singular<first_key_entry>(ambiguous),
            &first_key_entry_value);
  ASSERT_FALSE(ambiguous);
  ambiguous = true;
  ASSERT_EQ(group.template find_singular<second_key_entry>(ambiguous),
            &second_key_entry_value);
  ASSERT_FALSE(ambiguous);
}

TEST(index_test,
     materialized_ordinary_selector_group_deduplicates_requested_rows) {
  struct collection_processor {};
  struct first_storage {};

  using no_key_many_entry =
      detail::lookup_entry<collection_processor, no_key, many, ordered>;
  using group_type = detail::materialized_ordinary_selector_group<
      lookup_table_test_entry, lookup_table_test_allocator,
      type_list<no_key_many_entry>,
      type_list<
          detail::ordinary_selector_request<collection_processor, no_key>,
          detail::ordinary_selector_request<collection_processor, no_key>>>;

  lookup_table_test_allocator allocator;
  group_type group(allocator);
  lookup_table_test_entry entry(
      lookup_table_test_table::template make_no_key_identity<
          collection_processor, many>(),
      lookup_table_test_rtti::template get_type_index<first_storage>());

  ASSERT_TRUE(
      (group.template insert<no_key_many_entry, no_key_many_entry>(entry)));
  ASSERT_EQ(group.template count<no_key_many_entry>(), 1U);

  group.template erase<no_key_many_entry, no_key_many_entry>(entry);
  ASSERT_EQ(group.template count<no_key_many_entry>(), 0U);
}

TEST(index_test,
     ordinary_selector_group_indexes_inline_no_key_one_without_allocation) {
  struct processor {};
  struct first_storage {};

  using no_key_one_entry = detail::materialized_ordinary_selector_entry_t<
      type_list<>, detail::ordinary_selector_request<processor, no_key>>;
  using group_type = detail::materialized_ordinary_selector_group<
      lookup_table_test_entry, lookup_table_test_allocator, type_list<>,
      type_list<detail::ordinary_selector_request<processor, no_key>>>;

  test_allocator_state state;
  {
    lookup_table_test_allocator allocator(&state);
    group_type group(allocator);
    std::optional<lookup_table_test_entry> inline_entry;
    inline_entry.emplace(
        lookup_table_test_table::template make_no_key_identity<processor,
                                                               one>(),
        lookup_table_test_rtti::template get_type_index<first_storage>());

    ASSERT_EQ(state.allocations, 0);
    ASSERT_TRUE(group.template insert<no_key_one_entry>(*inline_entry));
    ASSERT_EQ(state.allocations, 0);
    ASSERT_EQ(group.template count<no_key_one_entry>(), 1U);

    bool ambiguous = true;
    ASSERT_EQ(group.template find_singular<no_key_one_entry>(ambiguous),
              std::addressof(*inline_entry));
    ASSERT_FALSE(ambiguous);
  }

  ASSERT_EQ(state.deallocations, state.allocations);
}

TEST(index_test,
     ordinary_selector_group_no_key_one_does_not_drive_many_row_allocation) {
  struct processor {};
  struct processor_key {};
  struct first_storage {};
  struct second_storage {};

  using no_key_one_entry = detail::materialized_ordinary_selector_entry_t<
      type_list<>, detail::ordinary_selector_request<processor, no_key>>;
  using typed_key_many_entry =
      detail::lookup_entry<processor, typed_key<processor_key>, many, ordered>;
  using group_type = detail::materialized_ordinary_selector_group<
      lookup_table_test_entry, lookup_table_test_allocator,
      type_list<typed_key_many_entry>,
      type_list<detail::ordinary_selector_request<processor, no_key>,
                detail::ordinary_selector_request<processor,
                                                  typed_key<processor_key>>>>;

  test_allocator_state state;
  {
    lookup_table_test_allocator allocator(&state);
    group_type group(allocator);
    std::optional<lookup_table_test_entry> first_inline_entry;
    std::optional<lookup_table_test_entry> second_inline_entry;
    first_inline_entry.emplace(
        lookup_table_test_table::template make_no_key_identity<processor,
                                                               one>(),
        lookup_table_test_rtti::template get_type_index<first_storage>());
    second_inline_entry.emplace(
        lookup_table_test_table::template make_typed_key_identity<
            processor, processor_key, many>(),
        lookup_table_test_rtti::template get_type_index<second_storage>());

    ASSERT_TRUE((group.template insert<no_key_one_entry, typed_key_many_entry>(
        *first_inline_entry)));
    ASSERT_EQ(state.allocations, 0);

    ASSERT_TRUE(
        group.template insert<typed_key_many_entry>(*second_inline_entry));
    ASSERT_EQ(state.allocations, 1);
    ASSERT_EQ(group.template count<no_key_one_entry>(), 1U);
    ASSERT_EQ(group.template count<typed_key_many_entry>(), 2U);
  }

  ASSERT_EQ(state.deallocations, state.allocations);
}

TEST(
    index_test,
    ordinary_selector_group_rollback_from_singular_conflict_does_not_allocate) {
  struct no_key_processor {};
  struct typed_processor {};
  struct processor_key {};
  struct first_storage {};
  struct second_storage {};

  using no_key_one_entry = detail::materialized_ordinary_selector_entry_t<
      type_list<>, detail::ordinary_selector_request<no_key_processor, no_key>>;
  using typed_key_one_entry = detail::materialized_ordinary_selector_entry_t<
      type_list<>, detail::ordinary_selector_request<typed_processor,
                                                     typed_key<processor_key>>>;
  using group_type = detail::materialized_ordinary_selector_group<
      lookup_table_test_entry, lookup_table_test_allocator, type_list<>,
      type_list<detail::ordinary_selector_request<no_key_processor, no_key>,
                detail::ordinary_selector_request<typed_processor,
                                                  typed_key<processor_key>>>>;

  test_allocator_state state;
  {
    lookup_table_test_allocator allocator(&state);
    group_type group(allocator);
    std::optional<lookup_table_test_entry> existing_entry;
    std::optional<lookup_table_test_entry> candidate_entry;
    existing_entry.emplace(
        lookup_table_test_table::template make_typed_key_identity<
            typed_processor, processor_key, one>(),
        lookup_table_test_rtti::template get_type_index<first_storage>());
    candidate_entry.emplace(
        lookup_table_test_table::template make_no_key_identity<no_key_processor,
                                                               one>(),
        lookup_table_test_rtti::template get_type_index<second_storage>());

    ASSERT_TRUE(group.template insert<typed_key_one_entry>(*existing_entry));
    ASSERT_EQ(state.allocations, 0);

    ASSERT_FALSE((group.template insert<no_key_one_entry, typed_key_one_entry>(
        *candidate_entry)));
    ASSERT_EQ(state.allocations, 0);
    ASSERT_EQ(group.template count<no_key_one_entry>(), 0U);
    ASSERT_EQ(group.template count<typed_key_one_entry>(), 1U);

    bool ambiguous = true;
    ASSERT_EQ(group.template find_singular<typed_key_one_entry>(ambiguous),
              std::addressof(*existing_entry));
    ASSERT_FALSE(ambiguous);
  }

  ASSERT_EQ(state.deallocations, state.allocations);
}

TEST(index_test,
     ordinary_selector_group_dedupes_no_key_one_without_duplicate_row_work) {
  struct processor {};
  struct first_storage {};

  using no_key_one_entry = detail::materialized_ordinary_selector_entry_t<
      type_list<>, detail::ordinary_selector_request<processor, no_key>>;
  using group_type = detail::materialized_ordinary_selector_group<
      lookup_table_test_entry, lookup_table_test_allocator, type_list<>,
      type_list<detail::ordinary_selector_request<processor, no_key>,
                detail::ordinary_selector_request<processor, no_key>>>;

  test_allocator_state state;
  {
    lookup_table_test_allocator allocator(&state);
    group_type group(allocator);
    std::optional<lookup_table_test_entry> inline_entry;
    inline_entry.emplace(
        lookup_table_test_table::template make_no_key_identity<processor,
                                                               one>(),
        lookup_table_test_rtti::template get_type_index<first_storage>());

    ASSERT_TRUE((group.template insert<no_key_one_entry, no_key_one_entry>(
        *inline_entry)));
    ASSERT_EQ(state.allocations, 0);
    ASSERT_EQ(group.template count<no_key_one_entry>(), 1U);

    group.template erase<no_key_one_entry, no_key_one_entry>(*inline_entry);
    ASSERT_EQ(state.allocations, 0);
    ASSERT_EQ(group.template count<no_key_one_entry>(), 0U);
  }

  ASSERT_EQ(state.deallocations, state.allocations);
}

TEST(
    index_test,
    prototype_owned_registration_no_key_one_commits_inline_without_allocation) {
  struct processor {};
  struct first_storage {};

  using no_key_one_entry = detail::materialized_ordinary_selector_entry_t<
      type_list<>, detail::ordinary_selector_request<processor, no_key>>;
  using group_type = detail::materialized_ordinary_selector_group<
      lookup_table_test_entry, lookup_table_test_allocator, type_list<>,
      type_list<detail::ordinary_selector_request<processor, no_key>>>;

  test_allocator_state state;
  {
    lookup_table_test_allocator allocator(&state);
    prototype_lookup_entry_owner owner(allocator);
    group_type group(allocator);

    auto *entry = prototype_register_owned_entry<no_key_one_entry>(
        owner, group,
        lookup_table_test_table::template make_no_key_identity<processor,
                                                               one>(),
        lookup_table_test_rtti::template get_type_index<first_storage>());

    ASSERT_NE(entry, nullptr);
    ASSERT_EQ(state.allocations, 0);
    ASSERT_EQ(owner.count(), 1U);
    ASSERT_EQ(group.template count<no_key_one_entry>(), 1U);

    bool ambiguous = true;
    ASSERT_EQ(group.template find_singular<no_key_one_entry>(ambiguous), entry);
    ASSERT_FALSE(ambiguous);
  }

  ASSERT_EQ(state.deallocations, state.allocations);
}

TEST(index_test,
     prototype_owned_registration_rolls_back_failed_multi_row_insert) {
  struct collection_processor {};
  struct typed_processor {};
  struct processor_key {};
  struct first_storage {};
  struct second_storage {};

  using no_key_many_entry =
      detail::lookup_entry<collection_processor, no_key, many, ordered>;
  using typed_key_one_entry = detail::materialized_ordinary_selector_entry_t<
      type_list<no_key_many_entry>,
      detail::ordinary_selector_request<typed_processor,
                                        typed_key<processor_key>>>;
  using group_type = detail::materialized_ordinary_selector_group<
      lookup_table_test_entry, lookup_table_test_allocator,
      type_list<no_key_many_entry>,
      type_list<detail::ordinary_selector_request<collection_processor, no_key>,
                detail::ordinary_selector_request<typed_processor,
                                                  typed_key<processor_key>>>>;

  test_allocator_state state;
  {
    lookup_table_test_allocator allocator(&state);
    prototype_lookup_entry_owner owner(allocator);
    group_type group(allocator);

    auto *existing_entry = prototype_register_owned_entry<typed_key_one_entry>(
        owner, group,
        lookup_table_test_table::template make_typed_key_identity<
            typed_processor, processor_key, one>(),
        lookup_table_test_rtti::template get_type_index<first_storage>());

    ASSERT_NE(existing_entry, nullptr);
    ASSERT_EQ(state.allocations, 0);

    auto *failed_entry =
        (prototype_register_owned_entry<no_key_many_entry, typed_key_one_entry>(
            owner, group,
            lookup_table_test_table::template make_no_key_identity<
                collection_processor, many>(),
            lookup_table_test_rtti::template get_type_index<second_storage>()));

    ASSERT_EQ(failed_entry, nullptr);
    ASSERT_EQ(state.allocations, 1);
    ASSERT_EQ(owner.count(), 1U);
    ASSERT_EQ(group.template count<no_key_many_entry>(), 0U);
    ASSERT_EQ(group.template count<typed_key_one_entry>(), 1U);

    bool ambiguous = true;
    ASSERT_EQ(group.template find_singular<typed_key_one_entry>(ambiguous),
              existing_entry);
    ASSERT_FALSE(ambiguous);

    std::size_t visits = 0;
    ASSERT_EQ(
        group.template for_each<no_key_many_entry>([&](auto &) { ++visits; }),
        0U);
    ASSERT_EQ(visits, 0U);
  }

  ASSERT_EQ(state.deallocations, state.allocations);
}

TEST(index_test,
     prototype_owned_registration_commits_one_entry_to_multiple_rows) {
  struct collection_processor {};
  struct typed_processor {};
  struct processor_key {};
  struct first_storage {};

  using no_key_many_entry =
      detail::lookup_entry<collection_processor, no_key, many, ordered>;
  using typed_key_one_entry = detail::materialized_ordinary_selector_entry_t<
      type_list<no_key_many_entry>,
      detail::ordinary_selector_request<typed_processor,
                                        typed_key<processor_key>>>;
  using group_type = detail::materialized_ordinary_selector_group<
      lookup_table_test_entry, lookup_table_test_allocator,
      type_list<no_key_many_entry>,
      type_list<detail::ordinary_selector_request<collection_processor, no_key>,
                detail::ordinary_selector_request<typed_processor,
                                                  typed_key<processor_key>>>>;

  test_allocator_state state;
  {
    lookup_table_test_allocator allocator(&state);
    prototype_lookup_entry_owner owner(allocator);
    group_type group(allocator);

    auto *entry =
        (prototype_register_owned_entry<no_key_many_entry, typed_key_one_entry>(
            owner, group,
            lookup_table_test_table::template make_no_key_identity<
                collection_processor, many>(),
            lookup_table_test_rtti::template get_type_index<first_storage>()));

    ASSERT_NE(entry, nullptr);
    ASSERT_EQ(state.allocations, 0);
    ASSERT_EQ(owner.count(), 1U);
    ASSERT_EQ(group.template count<no_key_many_entry>(), 1U);
    ASSERT_EQ(group.template count<typed_key_one_entry>(), 1U);

    bool ambiguous = true;
    ASSERT_EQ(group.template find_singular<typed_key_one_entry>(ambiguous),
              entry);
    ASSERT_FALSE(ambiguous);

    std::size_t visits = 0;
    ASSERT_EQ(group.template for_each<no_key_many_entry>([&](auto &selected) {
      ++visits;
      ASSERT_EQ(std::addressof(selected), entry);
    }),
              1U);
    ASSERT_EQ(visits, 1U);
  }

  ASSERT_EQ(state.deallocations, state.allocations);
}

TEST(index_test, prototype_owned_registration_dedupes_lookup_entries) {
  struct processor {};
  struct first_storage {};

  using no_key_one_entry = detail::materialized_ordinary_selector_entry_t<
      type_list<>, detail::ordinary_selector_request<processor, no_key>>;
  using group_type = detail::materialized_ordinary_selector_group<
      lookup_table_test_entry, lookup_table_test_allocator, type_list<>,
      type_list<detail::ordinary_selector_request<processor, no_key>,
                detail::ordinary_selector_request<processor, no_key>>>;

  test_allocator_state state;
  {
    lookup_table_test_allocator allocator(&state);
    prototype_lookup_entry_owner owner(allocator);
    group_type group(allocator);

    auto *entry =
        (prototype_register_owned_entry<no_key_one_entry, no_key_one_entry>(
            owner, group,
            lookup_table_test_table::template make_no_key_identity<processor,
                                                                   one>(),
            lookup_table_test_rtti::template get_type_index<first_storage>()));

    ASSERT_NE(entry, nullptr);
    ASSERT_EQ(state.allocations, 0);
    ASSERT_EQ(owner.count(), 1U);
    ASSERT_EQ(group.template count<no_key_one_entry>(), 1U);
  }

  ASSERT_EQ(state.deallocations, state.allocations);
}

TEST(index_test,
     prototype_owned_registration_second_entry_spills_only_in_owner) {
  struct first_processor {};
  struct second_processor {};
  struct first_storage {};
  struct second_storage {};

  using first_no_key_one_entry = detail::materialized_ordinary_selector_entry_t<
      type_list<>, detail::ordinary_selector_request<first_processor, no_key>>;
  using second_no_key_one_entry =
      detail::materialized_ordinary_selector_entry_t<
          type_list<>,
          detail::ordinary_selector_request<second_processor, no_key>>;
  using group_type = detail::materialized_ordinary_selector_group<
      lookup_table_test_entry, lookup_table_test_allocator, type_list<>,
      type_list<detail::ordinary_selector_request<first_processor, no_key>,
                detail::ordinary_selector_request<second_processor, no_key>>>;

  test_allocator_state state;
  {
    lookup_table_test_allocator allocator(&state);
    prototype_lookup_entry_owner owner(allocator);
    group_type group(allocator);

    auto *first_entry = prototype_register_owned_entry<first_no_key_one_entry>(
        owner, group,
        lookup_table_test_table::template make_no_key_identity<first_processor,
                                                               one>(),
        lookup_table_test_rtti::template get_type_index<first_storage>());

    ASSERT_NE(first_entry, nullptr);
    ASSERT_EQ(state.allocations, 0);

    auto *second_entry =
        prototype_register_owned_entry<second_no_key_one_entry>(
            owner, group,
            lookup_table_test_table::template make_no_key_identity<
                second_processor, one>(),
            lookup_table_test_rtti::template get_type_index<second_storage>());

    ASSERT_NE(second_entry, nullptr);
    ASSERT_EQ(state.allocations, 1);
    ASSERT_EQ(owner.count(), 2U);
    ASSERT_EQ(group.template count<first_no_key_one_entry>(), 1U);
    ASSERT_EQ(group.template count<second_no_key_one_entry>(), 1U);

    bool ambiguous = true;
    ASSERT_EQ(group.template find_singular<first_no_key_one_entry>(ambiguous),
              first_entry);
    ASSERT_FALSE(ambiguous);
    ambiguous = true;
    ASSERT_EQ(group.template find_singular<second_no_key_one_entry>(ambiguous),
              second_entry);
    ASSERT_FALSE(ambiguous);
  }

  ASSERT_EQ(state.deallocations, state.allocations);
}

TEST(index_test,
     registered_ordinary_selector_group_materializes_production_entries) {
  struct collection_processor {};
  struct typed_processor {};
  struct processor_key {};
  struct first_storage {};

  struct traits : dynamic_container_traits {
    using view_definition_type =
        views<collection<collection_processor>,
              typed<processor_key, typed_processor, one>>;
  };
  using container_type = container<traits, lookup_table_test_allocator>;
  using access_type = registered_storage_candidate_access<container_type>;
  using table_type = typename access_type::lookup_table_type;
  using registered_entry = typename access_type::registered_binding_entry;

  using no_key_many_entry =
      detail::lookup_entry<collection_processor, no_key, many, ordered>;
  using typed_key_one_entry = detail::materialized_ordinary_selector_entry_t<
      type_list<no_key_many_entry>,
      detail::ordinary_selector_request<typed_processor,
                                        typed_key<processor_key>>>;
  using group_type = detail::materialized_ordinary_selector_group<
      registered_entry, lookup_table_test_allocator,
      type_list<no_key_many_entry>,
      type_list<detail::ordinary_selector_request<collection_processor, no_key>,
                detail::ordinary_selector_request<typed_processor,
                                                  typed_key<processor_key>>>>;

  lookup_table_test_allocator allocator;
  group_type group(allocator);
  auto entry = make_registered_binding_entry<access_type, container_type,
                                             first_storage>(
      table_type::template make_no_key_identity<collection_processor, many>());

  ASSERT_TRUE(
      (group.template insert<no_key_many_entry, typed_key_one_entry>(entry)));
  ASSERT_EQ(group.template count<no_key_many_entry>(), 1U);
  ASSERT_EQ(group.template count<typed_key_one_entry>(), 1U);

  bool ambiguous = true;
  ASSERT_EQ(group.template find_singular<typed_key_one_entry>(ambiguous),
            std::addressof(entry));
  ASSERT_FALSE(ambiguous);

  std::size_t visits = 0;
  ASSERT_EQ(group.template for_each<no_key_many_entry>([&](auto &selected) {
    ++visits;
    ASSERT_EQ(std::addressof(selected), std::addressof(entry));
  }),
            1U);
  ASSERT_EQ(visits, 1U);
}

TEST(index_test,
     registered_ordinary_selector_group_no_key_one_is_pointer_only) {
  struct processor {};
  struct first_storage {};

  struct traits : dynamic_container_traits {
    using view_definition_type = views<single<processor>>;
  };
  using container_type = container<traits, lookup_table_test_allocator>;
  using access_type = registered_storage_candidate_access<container_type>;
  using table_type = typename access_type::lookup_table_type;
  using registered_entry = typename access_type::registered_binding_entry;

  using no_key_one_entry = detail::materialized_ordinary_selector_entry_t<
      type_list<>, detail::ordinary_selector_request<processor, no_key>>;
  using group_type = detail::materialized_ordinary_selector_group<
      registered_entry, lookup_table_test_allocator, type_list<>,
      type_list<detail::ordinary_selector_request<processor, no_key>>>;

  test_allocator_state state;
  {
    lookup_table_test_allocator allocator(&state);
    group_type group(allocator);
    auto entry = make_registered_binding_entry<access_type, container_type,
                                               first_storage>(
        table_type::template make_no_key_identity<processor, one>());

    ASSERT_EQ(state.allocations, 0);
    ASSERT_TRUE(group.template insert<no_key_one_entry>(entry));
    ASSERT_EQ(state.allocations, 0);
    ASSERT_EQ(group.template count<no_key_one_entry>(), 1U);

    bool ambiguous = true;
    ASSERT_EQ(group.template find_singular<no_key_one_entry>(ambiguous),
              std::addressof(entry));
    ASSERT_FALSE(ambiguous);
  }

  ASSERT_EQ(state.deallocations, state.allocations);
}

TEST(index_test, registered_ordinary_selector_group_rolls_back_partial_insert) {
  struct collection_processor {};
  struct typed_processor {};
  struct processor_key {};
  struct first_storage {};
  struct second_storage {};

  struct traits : dynamic_container_traits {
    using view_definition_type =
        views<collection<collection_processor>,
              typed<processor_key, typed_processor, one>>;
  };
  using container_type = container<traits, lookup_table_test_allocator>;
  using access_type = registered_storage_candidate_access<container_type>;
  using table_type = typename access_type::lookup_table_type;
  using registered_entry = typename access_type::registered_binding_entry;

  using no_key_many_entry =
      detail::lookup_entry<collection_processor, no_key, many, ordered>;
  using typed_key_one_entry = detail::materialized_ordinary_selector_entry_t<
      type_list<no_key_many_entry>,
      detail::ordinary_selector_request<typed_processor,
                                        typed_key<processor_key>>>;
  using group_type = detail::materialized_ordinary_selector_group<
      registered_entry, lookup_table_test_allocator,
      type_list<no_key_many_entry>,
      type_list<detail::ordinary_selector_request<collection_processor, no_key>,
                detail::ordinary_selector_request<typed_processor,
                                                  typed_key<processor_key>>>>;

  test_allocator_state state;
  {
    lookup_table_test_allocator allocator(&state);
    group_type group(allocator);
    auto existing_entry =
        make_registered_binding_entry<access_type, container_type,
                                      first_storage>(
            table_type::template make_typed_key_identity<typed_processor,
                                                         processor_key, one>());
    auto candidate_entry =
        make_registered_binding_entry<access_type, container_type,
                                      second_storage>(
            table_type::template make_no_key_identity<collection_processor,
                                                      many>());

    ASSERT_TRUE(group.template insert<typed_key_one_entry>(existing_entry));
    ASSERT_EQ(state.allocations, 0);

    ASSERT_FALSE((group.template insert<no_key_many_entry, typed_key_one_entry>(
        candidate_entry)));
    ASSERT_EQ(state.allocations, 0);
    ASSERT_EQ(group.template count<no_key_many_entry>(), 0U);
    ASSERT_EQ(group.template count<typed_key_one_entry>(), 1U);

    bool ambiguous = true;
    ASSERT_EQ(group.template find_singular<typed_key_one_entry>(ambiguous),
              std::addressof(existing_entry));
    ASSERT_FALSE(ambiguous);
  }

  ASSERT_EQ(state.deallocations, state.allocations);
}

TEST(index_test,
     registered_ordinary_selector_group_erases_and_dedupes_row_arguments) {
  struct processor {};
  struct first_storage {};

  struct traits : dynamic_container_traits {
    using view_definition_type = views<single<processor>>;
  };
  using container_type = container<traits, lookup_table_test_allocator>;
  using access_type = registered_storage_candidate_access<container_type>;
  using table_type = typename access_type::lookup_table_type;
  using registered_entry = typename access_type::registered_binding_entry;

  using no_key_one_entry = detail::materialized_ordinary_selector_entry_t<
      type_list<>, detail::ordinary_selector_request<processor, no_key>>;
  using group_type = detail::materialized_ordinary_selector_group<
      registered_entry, lookup_table_test_allocator, type_list<>,
      type_list<detail::ordinary_selector_request<processor, no_key>,
                detail::ordinary_selector_request<processor, no_key>>>;

  test_allocator_state state;
  {
    lookup_table_test_allocator allocator(&state);
    group_type group(allocator);
    auto entry = make_registered_binding_entry<access_type, container_type,
                                               first_storage>(
        table_type::template make_no_key_identity<processor, one>());

    ASSERT_TRUE(
        (group.template insert<no_key_one_entry, no_key_one_entry>(entry)));
    ASSERT_EQ(state.allocations, 0);
    ASSERT_EQ(group.template count<no_key_one_entry>(), 1U);

    group.template erase<no_key_one_entry, no_key_one_entry>(entry);
    ASSERT_EQ(state.allocations, 0);
    ASSERT_EQ(group.template count<no_key_one_entry>(), 0U);

    bool ambiguous = true;
    ASSERT_EQ(group.template find_singular<no_key_one_entry>(ambiguous),
              nullptr);
    ASSERT_FALSE(ambiguous);
  }

  ASSERT_EQ(state.deallocations, state.allocations);
}

TEST(index_test,
     lookup_table_runtime_key_one_rejects_duplicate_without_row_growth) {
  struct processor {};
  struct first_storage {};
  struct second_storage {};
  using runtime_lookup_entry =
      detail::lookup_entry<processor, runtime_key<std::size_t>, one, ordered>;
  struct entry;
  using table_type =
      detail::runtime_lookup_table<lookup_table_test_rtti,
                                   lookup_table_test_allocator, entry,
                                   type_list<runtime_lookup_entry>>;
  struct entry {
    entry(table_type::lookup_identity &&resolved_identity,
          typename lookup_table_test_rtti::type_index resolved_storage_type)
        : storage_type(std::move(resolved_storage_type)),
          identity(std::move(resolved_identity)) {}

    typename lookup_table_test_rtti::type_index storage_type;
    table_type::lookup_identity identity;
  };

  test_allocator_state state;
  {
    lookup_table_test_allocator allocator(&state);
    table_type table(allocator);
    entry first_entry(
        table_type::template make_runtime_key_identity<processor, std::size_t,
                                                       one>(allocator,
                                                            std::size_t(7)),
        lookup_table_test_rtti::template get_type_index<first_storage>());
    entry second_entry(
        table_type::template make_runtime_key_identity<processor, std::size_t,
                                                       one>(allocator,
                                                            std::size_t(7)),
        lookup_table_test_rtti::template get_type_index<second_storage>());

    ASSERT_TRUE(table.insert(first_entry));
    const auto allocations_after_first_insert = state.allocations;

    ASSERT_FALSE(table.insert(second_entry));
    ASSERT_EQ(state.allocations, allocations_after_first_insert);
    ASSERT_EQ((table.template count_runtime_key<processor, std::size_t, one,
                                                runtime_lookup_entry>(
                  std::size_t(7))),
              1U);

    bool ambiguous = false;
    auto *selected =
        table.template find_singular_runtime_key<processor, std::size_t, one,
                                                 runtime_lookup_entry>(
            std::size_t(7), ambiguous);
    ASSERT_FALSE(ambiguous);
    ASSERT_EQ(selected, &first_entry);
  }

  ASSERT_EQ(state.deallocations, state.allocations);
}

TEST(index_test, lookup_table_spills_second_distinct_bucket_and_reuses_inline) {
  struct processor {};
  struct processor_key {};

  test_allocator_state state;
  {
    lookup_table_test_allocator allocator(&state);
    lookup_table_test_table table(allocator);
    lookup_table_test_entry no_key_entry(
        lookup_table_test_table::template make_no_key_identity<processor,
                                                               many>());
    lookup_table_test_entry typed_key_entry(
        lookup_table_test_table::template make_typed_key_identity<
            processor, processor_key, many>());

    ASSERT_TRUE(table.insert(no_key_entry));
    ASSERT_EQ(state.allocations, 0);

    ASSERT_TRUE(table.insert(typed_key_entry));
    ASSERT_EQ(state.allocations, 1);
    ASSERT_EQ((table.template count_no_key<processor, many>()), 1U);
    ASSERT_EQ(
        (table.template count_typed_key<processor, processor_key, many>()), 1U);

    table.erase(no_key_entry);
    ASSERT_EQ((table.template count_no_key<processor, many>()), 0U);

    ASSERT_TRUE(table.insert(no_key_entry));
    ASSERT_EQ(state.allocations, 1);
    ASSERT_EQ((table.template count_no_key<processor, many>()), 1U);
  }

  ASSERT_EQ(state.deallocations, state.allocations);
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
    using view_definition_type = views<collection<processor>>;
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
    using view_definition_type = views<typed<processor_key, processor, many>>;
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
    using view_definition_type =
        views<associative<comparable_key, processor, many>>;
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
    using view_definition_type =
        views<associative<comparable_key, processor, many>>;
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
                     detail::view_definition<
                         processor, runtime_key<std::size_t>, one, ordered>>);

  struct traits : dynamic_container_traits {
    using view_definition_type = views<associative<std::size_t, processor>>;
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
                     detail::view_definition<
                         processor, runtime_key<std::size_t>, many, ordered>>);

  struct traits : dynamic_container_traits {
    using view_definition_type =
        views<associative<std::size_t, processor, many>>;
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
    using view_definition_type =
        views<associative<std::size_t, typed_key_cached_processor>>;
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
    using view_definition_type = views<associative<std::size_t, processor>>;
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
    using view_definition_type = views<associative<std::size_t, processor>>;
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
    using view_definition_type =
        views<associative<std::size_t, processor, many>>;
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
    using view_definition_type =
        views<associative<std::size_t, processor, many>>;
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
    using view_definition_type =
        views<associative<std::size_t, processor, many>>;
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
    using view_definition_type = views<single<processor>>;
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
    using view_definition_type = views<single<processor>>;
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
    using view_definition_type = views<single<processor>>;
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
    using view_definition_type = views<collection<processor>>;
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
    using view_definition_type = views<collection<processor>>;
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
    using view_definition_type = views<collection<processor>>;
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
    using view_definition_type = views<collection<processor>>;
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
     failed_no_key_many_registration_preserves_existing_lookup_rows) {
  struct processor {
    virtual ~processor() = default;
    virtual int id() const = 0;
  };
  struct processor_impl : processor {
    int id() const override { return 1; }
  };

  struct traits : dynamic_container_traits {
    using view_definition_type = views<collection<processor>>;
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

TEST(index_test, empty_child_no_key_one_lookup_falls_back_to_parent) {
  struct processor {
    virtual ~processor() = default;
    virtual int id() const = 0;
  };
  struct parent_processor : processor {
    int id() const override { return 1; }
  };

  struct traits : dynamic_container_traits {
    using view_definition_type = views<single<processor>>;
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
    using view_definition_type = views<single<processor>>;
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
    using view_definition_type = views<collection<processor>>;
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
    using view_definition_type = views<collection<processor>>;
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
    using view_definition_type = views<collection<processor>>;
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
    using view_definition_type = views<typed<processor_key, processor, one>>;
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
    using view_definition_type = views<typed<processor_key, processor, one>>;
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
    using view_definition_type = views<typed<processor_key, processor, many>>;
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
  ASSERT_EQ(processors[0]->id(), 1);
  ASSERT_EQ(processors[1]->id(), 2);
  ASSERT_EQ(tracked.reserve_count, 2U);
  ASSERT_EQ(tracked.values.size(), 2U);
  ASSERT_EQ(tracked.values[0]->id(), 1);
  ASSERT_EQ(tracked.values[1]->id(), 2);
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
    using view_definition_type = views<typed<processor_key, processor, many>>;
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
    using view_definition_type = views<typed<processor_key, processor, many>>;
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
     failed_typed_key_many_registration_preserves_existing_lookup_rows) {
  struct processor_key {};
  struct processor {
    virtual ~processor() = default;
    virtual int id() const = 0;
  };
  struct processor_impl : processor {
    int id() const override { return 1; }
  };

  struct traits : dynamic_container_traits {
    using view_definition_type = views<typed<processor_key, processor, many>>;
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
    using view_definition_type = views<typed<processor_key, processor, one>>;
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
    using view_definition_type = views<typed<processor_key, processor, one>>;
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
    using view_definition_type = views<typed<processor_key, processor, many>>;
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
    using view_definition_type = views<typed<processor_key, processor, many>>;
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
    using view_definition_type = views<typed<processor_key, processor, many>>;
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
    using view_definition_type =
        views<typed<processor_key, typed_key_cached_processor, one>>;
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
    using view_definition_type = views<associative<std::size_t, processor>,
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
    using view_definition_type = views<associative<std::size_t, source>,
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
    using view_definition_type = views<associative<std::size_t, processor>,
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
    using view_definition_type = views<associative<std::size_t, animal>,
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
    using view_definition_type = views<associative<std::string, animal>>;
  };

  test_allocator_state state;
  test_allocator<char> allocator(&state);
  container<traits, test_allocator<char>> container(allocator);

  container.template register_indexed_type<scope<shared>, storage<dog>,
                                           interfaces<animal>>(
      std::string("dog"));

  ASSERT_EQ(container.template resolve<animal &>(std::string("dog")).id(), 11);
}

TEST(index_test, runtime_key_lookup_payload_uses_rebound_allocator) {
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
    using view_definition_type = views<associative<small_key, animal>>;
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
    using view_definition_type =
        views<associative<allocator_visible_key, animal>>;
  };

  test_allocator_state state;
  {
    test_allocator<char> allocator(&state);
    container<large_key_traits, test_allocator<char>> container(allocator);

    container.template register_indexed_type<scope<shared>, storage<dog>,
                                             interfaces<animal>>(
        allocator_visible_key{7});

    ASSERT_EQ(state.allocations, 5);
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
    using view_definition_type =
        views<associative<std::size_t, processor, many>>;
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
    using view_definition_type =
        views<associative<std::size_t, processor, many>>;
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
    using view_definition_type =
        views<associative<std::size_t, processor, many>>;
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
     failed_runtime_key_many_registration_preserves_existing_lookup_rows) {
  struct processor {
    virtual ~processor() = default;
    virtual int id() const = 0;
  };
  struct processor_impl : processor {
    int id() const override { return 1; }
  };

  struct traits : dynamic_container_traits {
    using view_definition_type =
        views<associative<std::size_t, processor, many>>;
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
    using view_definition_type =
        views<associative<std::size_t, processor, many>>;
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
    using view_definition_type = views<associative<std::string, processor>>;
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
    using view_definition_type =
        views<associative<std::size_t, fixed_injection_processor>>;
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
    using view_definition_type =
        views<associative<std::size_t, fixed_injection_processor>>;
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
    using view_definition_type =
        views<associative<std::size_t, fixed_injection_processor>>;
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
    using view_definition_type =
        views<associative<std::size_t, fixed_injection_processor>>;
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
    using view_definition_type =
        views<associative<std::size_t, fixed_injection_processor, many>>;
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
    using view_definition_type =
        views<associative<std::size_t, fixed_injection_processor, many>>;
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
    using view_definition_type =
        views<associative<std::size_t, fixed_injection_processor, many>>;
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
    using view_definition_type =
        views<associative<std::size_t, fixed_injection_processor, many>>;
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
    using view_definition_type =
        views<associative<std::size_t, fixed_injection_processor, many>>;
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
    using view_definition_type =
        views<associative<std::size_t, fixed_injection_processor>>;
  };

  static_container<source, traits> container;

  auto &processor = container.template resolve<
      request<fixed_injection_processor &, key<std::size_t, 0>>>();

  ASSERT_EQ(processor.id(), 1);
}

TEST(index_test, constructor_injects_fixed_indexed_associative_binding) {
  struct traits : dynamic_container_traits {
    using view_definition_type =
        views<associative<std::size_t, fixed_injection_processor>>;
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
    using view_definition_type = views<
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
    using view_definition_type =
        views<associative<std::size_t, fixed_injection_processor>>;
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
    using view_definition_type =
        views<associative<std::size_t, fixed_injection_processor>>;
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
    using view_definition_type =
        views<associative<std::size_t, fixed_injection_processor>>;
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
    using view_definition_type =
        views<associative<std::size_t, annotated<fixed_injection_processor,
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
    using view_definition_type = dingo::views<
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
