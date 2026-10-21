//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

// The indexes of the key values of the candidates of a request, the memo of a
// request by a key without a value, and the indexing a lookup declaration
// names for its domain. spec: index_structures, lookup_declarations

#include <dingo/core/backend/memo.h>
#include <dingo/lookup/lookup.h>

#include <gtest/gtest.h>

#include <cstddef>
#include <string>
#include <type_traits>
#include <vector>

namespace {

using dingo::core::candidate;
using dingo::core::index_item;
using dingo::core::request_memo;

// A key type that only compares equal and hashes, so no ordered index
// serves it.
struct hashed_only {
  int value;
  bool operator==(const hashed_only &other) const {
    return value == other.value;
  }
};

// A key type that only compares equal and orders, so no hash table serves it.
struct ordered_only {
  int value;
  bool operator==(const ordered_only &other) const {
    return value == other.value;
  }
  bool operator<(const ordered_only &other) const {
    return value < other.value;
  }
};

} // namespace

template <> struct std::hash<hashed_only> {
  std::size_t operator()(const hashed_only &key) const {
    return std::hash<int>()(key.value);
  }
};

namespace {

// The keys of bindings and requests: which of them carry a value the index
// compares, and the key each key_type declares.
static_assert(!dingo::core::key_has_value(dingo::core::no_key::descriptor));
static_assert(
    !dingo::core::key_has_value(dingo::core::tag_key<int>::descriptor));
static_assert(
    dingo::core::key_has_value(dingo::core::fixed_key<int, 1>::descriptor));
static_assert(
    dingo::core::key_has_value(dingo::core::runtime_key<int>::descriptor));
static_assert(
    std::is_same_v<dingo::core::key_of_t<dingo::key_type<dingo::none_t>>,
                   dingo::core::no_key>);
static_assert(std::is_same_v<dingo::core::key_of_t<dingo::key_type<int>>,
                             dingo::core::tag_key<int>>);
static_assert(std::is_same_v<dingo::core::key_of_t<dingo::key_type<int, 1>>,
                             dingo::core::fixed_key<int, 1>>);
static_assert(
    std::is_same_v<dingo::core::request_key_of<dingo::key_value<int>>::type,
                   dingo::core::runtime_key<int>>);
static_assert(std::is_same_v<dingo::core::request_key_of<std::size_t>::type,
                             dingo::core::runtime_key<std::size_t>>);

// The built-in indexes, and the one each falls back to when the key type
// cannot serve it.
static_assert(std::is_same_v<dingo::core::ordered_index_t<int, int>,
                             dingo::core::sorted_index<int, int>>);
static_assert(std::is_same_v<dingo::core::ordered_index_t<hashed_only, int>,
                             dingo::core::hashed_index<hashed_only, int>>);
static_assert(std::is_same_v<dingo::core::hashed_index_t<int, int>,
                             dingo::core::hashed_index<int, int>>);
static_assert(std::is_same_v<dingo::core::hashed_index_t<ordered_only, int>,
                             dingo::core::sorted_index<ordered_only, int>>);
static_assert(std::is_same_v<dingo::core::dense_index_t<4, std::size_t, int>,
                             dingo::core::dense_index<4, std::size_t, int>>);
static_assert(std::is_same_v<dingo::core::dense_index_t<4, std::string, int>,
                             dingo::core::hashed_index<std::string, int>>);
static_assert(
    std::is_same_v<dingo::core::dense_index_t<dingo::core::unbounded, int, int>,
                   dingo::core::hashed_index<int, int>>);

template <typename Index, typename Value>
std::vector<int> visit_all(const Index &index, const Value &key) {
  std::vector<int> found;
  index.visit(key, [&](int candidate) {
    found.push_back(candidate);
    return true;
  });
  return found;
}

template <typename Index> void check_one_candidate_of_a_key() {
  const std::vector<index_item<std::size_t, int>> items = {{7, 1}, {3, 2}};
  Index index;
  index.build(items.data(), items.size());
  EXPECT_EQ(visit_all(index, std::size_t(7)), (std::vector<int>{1}));
  EXPECT_EQ(visit_all(index, std::size_t(3)), (std::vector<int>{2}));
  EXPECT_TRUE(visit_all(index, std::size_t(2)).empty());
}

TEST(next_index, finds_the_candidate_of_a_key_value_and_no_other) {
  check_one_candidate_of_a_key<dingo::core::linear_index<std::size_t, int>>();
  check_one_candidate_of_a_key<dingo::core::sorted_index<std::size_t, int>>();
  check_one_candidate_of_a_key<dingo::core::hashed_index<std::size_t, int>>();
  check_one_candidate_of_a_key<dingo::core::dense_index<8, std::size_t, int>>();
}

template <typename Index> void check_candidates_in_registration_order() {
  const std::vector<index_item<std::size_t, int>> items = {
      {7, 1}, {2, 9}, {7, 2}, {7, 3}};
  Index index;
  index.build(items.data(), items.size());
  EXPECT_EQ(visit_all(index, std::size_t(7)), (std::vector<int>{1, 2, 3}));
  std::vector<int> first;
  index.visit(std::size_t(7), [&](int candidate) {
    first.push_back(candidate);
    return false;
  });
  EXPECT_EQ(first, (std::vector<int>{1}));
}

TEST(next_index, visits_the_candidates_of_a_key_in_order_until_told_to_stop) {
  check_candidates_in_registration_order<
      dingo::core::linear_index<std::size_t, int>>();
  check_candidates_in_registration_order<
      dingo::core::sorted_index<std::size_t, int>>();
  check_candidates_in_registration_order<
      dingo::core::hashed_index<std::size_t, int>>();
  check_candidates_in_registration_order<
      dingo::core::dense_index<8, std::size_t, int>>();
}

template <typename Index> void check_rebuild() {
  std::vector<index_item<std::size_t, int>> items = {{7, 1}, {7, 2}, {8, 3}};
  Index index;
  index.build(items.data(), items.size());
  ASSERT_EQ(visit_all(index, std::size_t(7)), (std::vector<int>{1, 2}));
  items.erase(items.begin());
  items.pop_back();
  index.build(items.data(), items.size());
  EXPECT_EQ(visit_all(index, std::size_t(7)), (std::vector<int>{2}));
  EXPECT_TRUE(visit_all(index, std::size_t(8)).empty());
  index.build(items.data(), 0);
  EXPECT_TRUE(visit_all(index, std::size_t(7)).empty());
}

TEST(next_index, a_rebuilt_index_holds_only_the_candidates_of_its_last_build) {
  check_rebuild<dingo::core::linear_index<std::size_t, int>>();
  check_rebuild<dingo::core::sorted_index<std::size_t, int>>();
  check_rebuild<dingo::core::hashed_index<std::size_t, int>>();
  check_rebuild<dingo::core::dense_index<16, std::size_t, int>>();
}

struct entry {};

template <typename Key> void check_memo_without_a_value() {
  std::vector<entry> entries(3);
  request_memo<entry, Key> memo;
  for (std::size_t index = 0; index < entries.size(); ++index) {
    memo.add(candidate<entry>{&entries[index], index, nullptr});
  }
  memo.index();
  std::vector<std::size_t> found;
  memo.visit(nullptr, [&](const candidate<entry> &each) {
    found.push_back(each.term);
    return true;
  });
  EXPECT_EQ(found, (std::vector<std::size_t>{0, 1, 2}));
  std::vector<std::size_t> first;
  memo.visit(nullptr, [&](const candidate<entry> &each) {
    first.push_back(each.term);
    return false;
  });
  EXPECT_EQ(first, (std::vector<std::size_t>{0}));
  memo.clear();
  memo.index();
  bool visited = false;
  memo.visit(nullptr, [&](const candidate<entry> &) {
    visited = true;
    return true;
  });
  EXPECT_FALSE(visited);
}

TEST(next_index, a_memo_of_a_key_without_a_value_visits_every_candidate) {
  struct tag {};
  check_memo_without_a_value<dingo::core::no_key>();
  check_memo_without_a_value<dingo::core::tag_key<tag>>();
}

struct processor {};
struct animal {};

struct linear {
  template <typename Value, typename Candidate>
  using index = dingo::core::linear_index<Value, Candidate>;
};

using declarations = dingo::core::declaration_list<dingo::core::type_list<
    dingo::associative<std::size_t, processor, dingo::one, dingo::unordered>,
    dingo::associative<std::size_t, animal, dingo::many, linear>,
    dingo::associative<int, processor, dingo::one, dingo::array<4>>>>;

template <typename Interface, typename Key>
using indexing_of = typename declarations::template at<declarations::position(
    dingo::core::type_id_v<Interface>, Key::descriptor)>::indexing;

TEST(next_index, a_domain_is_indexed_by_the_index_of_its_declaration) {
  using size_key = dingo::core::runtime_key<std::size_t>;
  using int_key = dingo::core::runtime_key<int>;
  EXPECT_EQ(declarations::position(dingo::core::type_id_v<processor>,
                                   size_key::descriptor),
            0u);
  EXPECT_EQ(declarations::position(dingo::core::type_id_v<animal>,
                                   size_key::descriptor),
            1u);
  EXPECT_EQ(declarations::position(dingo::core::type_id_v<processor>,
                                   int_key::descriptor),
            2u);
  static_assert(
      std::is_same_v<indexing_of<processor, size_key>, dingo::unordered>);
  static_assert(std::is_same_v<indexing_of<animal, size_key>, linear>);
  static_assert(
      std::is_same_v<indexing_of<processor, int_key>, dingo::array<4>>);
  // A domain no declaration names has no indexing of its own, so the default
  // one indexes it.
  static_assert(std::is_void_v<indexing_of<animal, int_key>>);
}

using with_base = dingo::core::declaration_list<dingo::core::type_list<
    dingo::associative<std::size_t, processor, dingo::one, dingo::unordered>,
    dingo::base<dingo::many, linear>>>;

TEST(next_index, a_default_declaration_indexes_every_other_domain) {
  using int_key = dingo::core::runtime_key<int>;
  EXPECT_EQ(
      with_base::position(dingo::core::type_id_v<animal>, int_key::descriptor),
      1u);
  static_assert(
      std::is_same_v<typename with_base::template at<1>::indexing, linear>);
  static_assert(std::is_same_v<typename with_base::template at<0>::indexing,
                               dingo::unordered>);
}

} // namespace
