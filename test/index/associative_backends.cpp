//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#include <dingo/container.h>
#include <dingo/storage/shared.h>

#include <gtest/gtest.h>

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
  template <typename Key, typename Rows, typename Allocator> class storage {
    using value_type = std::pair<const Key, Rows>;
    using storage_allocator = typename std::conditional_t<
        is_static_allocator_v<Allocator>, std::allocator<value_type>,
        typename std::allocator_traits<Allocator>::template rebind_alloc<
            value_type>>;
    using storage_type = std::map<Key, Rows, std::less<Key>, storage_allocator>;

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
    iterator begin() { return values_.begin(); }
    const_iterator begin() const { return values_.begin(); }
    iterator end() { return values_.end(); }
    const_iterator end() const { return values_.end(); }

    template <typename... Args>
    auto try_emplace(const Key &key, Args &&...args) {
      ++custom_lookup_backend_insertions;
      return values_.try_emplace(key, std::forward<Args>(args)...);
    }

    iterator erase(iterator it) { return values_.erase(it); }

  private:
    storage_type values_;
  };
};

struct operator_lookup_backend {
  template <typename Key, typename Rows, typename Allocator> class storage {
    using value_type = std::pair<const Key, Rows>;
    using storage_allocator = typename std::conditional_t<
        is_static_allocator_v<Allocator>, std::allocator<value_type>,
        typename std::allocator_traits<Allocator>::template rebind_alloc<
            value_type>>;
    using storage_type = std::map<Key, Rows, std::less<Key>, storage_allocator>;

  public:
    using iterator = typename storage_type::iterator;
    using const_iterator = typename storage_type::const_iterator;

    explicit storage(Allocator &allocator)
        : values_(std::less<Key>{},
                  detail::make_lookup_storage_allocator<storage_allocator>(
                      allocator)) {}

    iterator find(const Key &key) { return values_.find(key); }
    const_iterator find(const Key &key) const { return values_.find(key); }
    iterator begin() { return values_.begin(); }
    const_iterator begin() const { return values_.begin(); }
    iterator end() { return values_.end(); }
    const_iterator end() const { return values_.end(); }

    template <typename... Args>
    auto try_emplace(const Key &key, Args &&...args) {
      return values_.try_emplace(key, std::forward<Args>(args)...);
    }

    Rows &operator[](const Key &key) {
      ++operator_lookup_backend_insertions;
      return values_[key];
    }

    iterator erase(iterator it) { return values_.erase(it); }

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
    using query_definition_type =
        queries<associative<std::size_t, processor, one, unordered>>;
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

TEST(associative_backend_test, unordered_many_preserves_key_order) {
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
    using query_definition_type =
        queries<associative<std::size_t, processor, many, unordered>>;
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
  ASSERT_EQ(values[0]->id(), 1);
  ASSERT_EQ(values[1]->id(), 2);
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
    using query_definition_type =
        queries<associative<std::size_t, processor, one, array<4>>>;
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
    using query_definition_type =
        queries<associative<std::size_t, processor, many, array<4>>>;
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
  ASSERT_EQ(values[0]->id(), 1);
  ASSERT_EQ(values[1]->id(), 2);
  EXPECT_THROW((container.template resolve<processor &>(std::size_t(2))),
               type_ambiguous_exception);
  EXPECT_THROW(
      (container.template register_indexed_type<
          scope<shared>, storage<first_processor>, interfaces<processor>>(
          std::size_t(2))),
      type_index_already_registered_exception);

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
    using query_definition_type =
        queries<associative<int, processor, one, custom_lookup_backend>>;
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
    using query_definition_type =
        queries<associative<int, processor, one, operator_lookup_backend>>;
  };

  operator_lookup_backend_insertions = 0;

  container<traits> container;
  container.template register_indexed_type<
      scope<shared>, storage<processor_impl>, interfaces<processor>>(7);

  ASSERT_EQ(operator_lookup_backend_insertions, 0);
  ASSERT_EQ(container.template resolve<processor &>(7).id(), 1);
}

} // namespace dingo
