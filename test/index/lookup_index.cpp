//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#include <dingo/container.h>
#include <dingo/runtime/lookup_index.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <memory>
#include <type_traits>
#include <vector>

namespace dingo {

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

} // namespace dingo
