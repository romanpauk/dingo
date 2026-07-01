//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#include <dingo/container.h>
#include <dingo/storage/shared.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <map>
#include <memory>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

namespace dingo {

inline int custom_lookup_backend_constructions = 0;
inline int custom_lookup_backend_insertions = 0;
inline int operator_lookup_backend_insertions = 0;

struct custom_lookup_backend {
  template <typename Key, typename Mapped, typename Cardinality,
            typename Allocator>
  class storage {
    using value_type = std::pair<const Key, Mapped>;
    using storage_allocator =
        typename std::allocator_traits<Allocator>::template rebind_alloc<
            value_type>;
    using storage_type = std::conditional_t<
        std::is_same_v<Cardinality, one>,
        std::map<Key, Mapped, std::less<Key>, storage_allocator>,
        std::multimap<Key, Mapped, std::less<Key>, storage_allocator>>;

  public:
    using iterator = typename storage_type::iterator;
    using const_iterator = typename storage_type::const_iterator;

    explicit storage(Allocator &allocator)
        : values_(std::less<Key>{},
                  detail::make_lookup_storage_allocator<storage_allocator>(
                      allocator)) {
      ++custom_lookup_backend_constructions;
    }

    iterator find(const Key &key) { return values_.find(key); }
    const_iterator find(const Key &key) const { return values_.find(key); }
    iterator end() { return values_.end(); }
    const_iterator end() const { return values_.end(); }

    std::pair<iterator, iterator> equal_range(const Key &key) {
      return values_.equal_range(key);
    }

    std::pair<const_iterator, const_iterator>
    equal_range(const Key &key) const {
      return values_.equal_range(key);
    }

    template <typename Value>
    std::pair<iterator, bool> try_emplace(const Key &key, Value &&value) {
      ++custom_lookup_backend_insertions;
      return values_.try_emplace(key, std::forward<Value>(value));
    }

    template <typename Value> iterator emplace(const Key &key, Value &&value) {
      ++custom_lookup_backend_insertions;
      return values_.emplace(key, std::forward<Value>(value));
    }

    void erase(iterator handle) { values_.erase(handle); }

  private:
    storage_type values_;
  };
};

struct operator_lookup_backend {
  template <typename Key, typename Mapped, typename Cardinality,
            typename Allocator>
  class storage {
    using value_type = std::pair<const Key, Mapped>;
    using storage_allocator =
        typename std::allocator_traits<Allocator>::template rebind_alloc<
            value_type>;
    using storage_type = std::conditional_t<
        std::is_same_v<Cardinality, one>,
        std::map<Key, Mapped, std::less<Key>, storage_allocator>,
        std::multimap<Key, Mapped, std::less<Key>, storage_allocator>>;

  public:
    using iterator = typename storage_type::iterator;
    using const_iterator = typename storage_type::const_iterator;

    explicit storage(Allocator &allocator)
        : values_(std::less<Key>{},
                  detail::make_lookup_storage_allocator<storage_allocator>(
                      allocator)) {}

    iterator find(const Key &key) { return values_.find(key); }
    const_iterator find(const Key &key) const { return values_.find(key); }
    iterator end() { return values_.end(); }
    const_iterator end() const { return values_.end(); }

    std::pair<iterator, iterator> equal_range(const Key &key) {
      return values_.equal_range(key);
    }

    std::pair<const_iterator, const_iterator>
    equal_range(const Key &key) const {
      return values_.equal_range(key);
    }

    template <typename Value>
    std::pair<iterator, bool> try_emplace(const Key &key, Value &&value) {
      return values_.try_emplace(key, std::forward<Value>(value));
    }

    template <typename Value> iterator emplace(const Key &key, Value &&value) {
      return values_.emplace(key, std::forward<Value>(value));
    }

    void erase(iterator handle) { values_.erase(handle); }

    Mapped &operator[](const Key &key) {
      ++operator_lookup_backend_insertions;
      return values_[key];
    }

  private:
    storage_type values_;
  };
};

TEST(associative_backend_test, unordered_one_uses_runtime_key_lookup) {
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
        lookups<associative<std::size_t, processor, one, unordered>>;
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
}

TEST(associative_backend_test, unordered_many_resolves_duplicate_key_entries) {
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
        lookups<associative<std::size_t, processor, many, unordered>>;
  };

  container<traits> container;
  container.template register_indexed_type<
      scope<shared>, storage<first_processor>, interfaces<processor>>(
      std::size_t(7));
  container.template register_indexed_type<
      scope<shared>, storage<second_processor>, interfaces<processor>>(
      std::size_t(7));

  auto values =
      container.template resolve<std::vector<processor *>>(std::size_t(7));

  ASSERT_EQ(values.size(), 2);
  std::vector<int> ids;
  for (auto *value : values) {
    ids.push_back(value->id());
  }
  std::sort(ids.begin(), ids.end());
  ASSERT_EQ(ids, (std::vector<int>{1, 2}));
  EXPECT_THROW((container.template resolve<processor &>(std::size_t(7))),
               type_ambiguous_exception);
}

TEST(associative_backend_test, array_one_uses_dense_key_lookup) {
  struct processor {
    virtual ~processor() = default;
    virtual int id() const = 0;
  };
  struct processor_impl : processor {
    int id() const override { return 1; }
  };
  struct out_of_range_processor : processor {
    int id() const override { return 2; }
  };

  struct traits : dynamic_container_traits {
    using lookup_definition_type =
        lookups<associative<std::size_t, processor, one, array<4>>>;
  };

  container<traits> container;
  container.template register_indexed_type<
      scope<shared>, storage<processor_impl>, interfaces<processor>>(
      std::size_t(3));

  ASSERT_EQ(container.template resolve<processor &>(std::size_t(3)).id(), 1);
  EXPECT_THROW((container.template register_indexed_type<
                   scope<shared>, storage<out_of_range_processor>,
                   interfaces<processor>>(std::size_t(4))),
               std::out_of_range);
  ASSERT_EQ(container.template resolve<processor &>(std::size_t(3)).id(), 1);
  EXPECT_THROW((container.template resolve<processor &>(std::size_t(4))),
               type_not_found_exception);
}

TEST(associative_backend_test, array_many_uses_dense_key_rows) {
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
  struct out_of_range_processor : processor {
    int id() const override { return 3; }
  };

  struct traits : dynamic_container_traits {
    using lookup_definition_type =
        lookups<associative<std::size_t, processor, many, array<4>>>;
  };

  container<traits> container;
  container.template register_indexed_type<
      scope<shared>, storage<first_processor>, interfaces<processor>>(
      std::size_t(2));
  container.template register_indexed_type<
      scope<shared>, storage<second_processor>, interfaces<processor>>(
      std::size_t(2));

  auto values =
      container.template resolve<std::vector<processor *>>(std::size_t(2));

  ASSERT_EQ(values.size(), 2);
  std::vector<int> ids;
  for (auto *value : values) {
    ids.push_back(value->id());
  }
  std::sort(ids.begin(), ids.end());
  ASSERT_EQ(ids, (std::vector<int>{1, 2}));
  EXPECT_THROW((container.template resolve<processor &>(std::size_t(2))),
               type_ambiguous_exception);
  container.template register_indexed_type<
      scope<shared>, storage<first_processor>, interfaces<processor>>(
      std::size_t(2));
  values = container.template resolve<std::vector<processor *>>(std::size_t(2));
  ASSERT_EQ(values.size(), 3);
  ids.clear();
  for (auto *value : values) {
    ids.push_back(value->id());
  }
  std::sort(ids.begin(), ids.end());
  ASSERT_EQ(ids, (std::vector<int>{1, 1, 2}));

  container.template register_indexed_type<
      scope<shared>, storage<first_processor>, interfaces<processor>>(
      std::size_t(3));
  ASSERT_EQ(container.template resolve<processor &>(std::size_t(3)).id(), 1);
  EXPECT_THROW((container.template register_indexed_type<
                   scope<shared>, storage<out_of_range_processor>,
                   interfaces<processor>>(std::size_t(4))),
               std::out_of_range);
}

TEST(associative_backend_test, custom_backend_uses_stl_like_try_emplace) {
  struct processor {
    virtual ~processor() = default;
    virtual int id() const = 0;
  };
  struct processor_impl : processor {
    int id() const override { return 1; }
  };

  struct traits : dynamic_container_traits {
    using lookup_definition_type =
        lookups<associative<int, processor, one, custom_lookup_backend>>;
  };

  custom_lookup_backend_constructions = 0;
  custom_lookup_backend_insertions = 0;

  container<traits> container;
  container.template register_indexed_type<
      scope<shared>, storage<processor_impl>, interfaces<processor>>(7);

  ASSERT_GT(custom_lookup_backend_constructions, 0);
  ASSERT_GT(custom_lookup_backend_insertions, 0);
  ASSERT_EQ(container.template resolve<processor &>(7).id(), 1);
}

TEST(associative_backend_test, custom_backend_storage_uses_arbitrary_mapped) {
  struct mapped_value {
    int value;
  };

  custom_lookup_backend_constructions = 0;
  custom_lookup_backend_insertions = 0;

  std::allocator<char> allocator;
  custom_lookup_backend::storage<int, mapped_value, one, std::allocator<char>>
      one_storage(allocator);

  auto [first_it, first_inserted] =
      one_storage.try_emplace(7, mapped_value{11});
  ASSERT_TRUE(first_inserted);
  ASSERT_EQ(first_it->second.value, 11);
  auto [second_it, second_inserted] =
      one_storage.try_emplace(7, mapped_value{22});
  ASSERT_FALSE(second_inserted);
  ASSERT_EQ(second_it->second.value, 11);

  custom_lookup_backend::storage<int, mapped_value, many, std::allocator<char>>
      many_storage(allocator);
  many_storage.emplace(7, mapped_value{11});
  many_storage.emplace(7, mapped_value{22});

  auto [first, last] = many_storage.equal_range(7);
  std::vector<int> values;
  for (auto it = first; it != last; ++it) {
    values.push_back(it->second.value);
  }
  std::sort(values.begin(), values.end());
  ASSERT_EQ(values, (std::vector<int>{11, 22}));
  ASSERT_GT(custom_lookup_backend_constructions, 0);
  ASSERT_GT(custom_lookup_backend_insertions, 0);
}

TEST(associative_backend_test,
     custom_backend_prefers_try_emplace_over_operator_subscript) {
  struct processor {
    virtual ~processor() = default;
    virtual int id() const = 0;
  };
  struct processor_impl : processor {
    int id() const override { return 1; }
  };

  struct traits : dynamic_container_traits {
    using lookup_definition_type =
        lookups<associative<int, processor, one, operator_lookup_backend>>;
  };

  operator_lookup_backend_insertions = 0;

  container<traits> container;
  container.template register_indexed_type<
      scope<shared>, storage<processor_impl>, interfaces<processor>>(7);

  ASSERT_EQ(operator_lookup_backend_insertions, 0);
  ASSERT_EQ(container.template resolve<processor &>(7).id(), 1);
}

} // namespace dingo
