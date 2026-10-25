//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#include <dingo/storage/storage_scope_policy.h>
#include <dingo/storage/type_storage_traits.h>

#include <gtest/gtest.h>

#include <memory>
#include <type_traits>

namespace dingo {
namespace {

struct scope_info_service {};

struct shared_plain_storage {
  using tag_type = shared;
  using type = scope_info_service;
  using resolved_type = scope_info_service *;
};

struct external_plain_storage {
  using tag_type = external;
  using type = scope_info_service;
  using resolved_type = scope_info_service &;
};

struct unique_plain_storage {
  using tag_type = unique;
  using type = scope_info_service;
  using resolved_type = scope_info_service;
};

struct unique_pointer_storage {
  using tag_type = unique;
  using type = scope_info_service *;
  using resolved_type = scope_info_service *;
};

struct unique_array_storage {
  using tag_type = unique;
  using type = scope_info_service[2];
  using resolved_type = scope_info_service *;
};

struct shared_handle_storage {
  using tag_type = shared;
  using type = std::shared_ptr<scope_info_service>;
  using resolved_type = std::shared_ptr<scope_info_service> &;
};

static_assert(scope_info_of<shared>.enabled);
static_assert(scope_info_of<shared>.stable);
static_assert(!scope_info_of<shared>.consumes);
static_assert(scope_info_of<shared>.retains_unresolved_source);
static_assert(scope_info_of<shared>.guard ==
              materialization_guard::while_unresolved);

static_assert(scope_info_of<unique>.consumes);
static_assert(!scope_info_of<unique>.stable);
static_assert(scope_info_of<unique>.guard == materialization_guard::always);

static_assert(scope_info_of<external>.guard == materialization_guard::none);
static_assert(!scope_info_of<external>.retains_unresolved_source);

static_assert(scope_info_of<shared_cyclical>.allows_cycles);
static_assert(!scope_info_of<shared_cyclical>.stable);

static_assert(
    std::is_same_v<storage_yield_traits<shared_plain_storage>::yielded_type,
                   scope_info_service *>);
static_assert(storage_yield_traits<shared_plain_storage>::category ==
              source_category::pointer);

static_assert(
    std::is_same_v<storage_yield_traits<external_plain_storage>::yielded_type,
                   scope_info_service &>);
static_assert(storage_yield_traits<external_plain_storage>::category ==
              source_category::lvalue);

static_assert(
    std::is_same_v<storage_yield_traits<unique_plain_storage>::yielded_type,
                   scope_info_service>);
static_assert(storage_yield_traits<unique_plain_storage>::category ==
              source_category::consume_value);

static_assert(
    std::is_same_v<storage_yield_traits<unique_pointer_storage>::yielded_type,
                   scope_info_service *>);
static_assert(storage_yield_traits<unique_pointer_storage>::category ==
              source_category::consume_pointer);

static_assert(
    std::is_same_v<storage_yield_traits<unique_array_storage>::yielded_type,
                   scope_info_service *>);
static_assert(storage_yield_traits<unique_array_storage>::category ==
              source_category::consume_pointer);

static_assert(
    std::is_same_v<storage_yield_traits<shared_handle_storage>::yielded_type,
                   std::shared_ptr<scope_info_service> &>);
static_assert(storage_yield_traits<shared_handle_storage>::category ==
              source_category::lvalue);

TEST(scope_info_test, descriptors_follow_the_generated_scope_policies) {
  EXPECT_TRUE(scope_info_of<shared>.stable);
  EXPECT_FALSE(scope_info_of<shared>.consumes);
  EXPECT_TRUE(scope_info_of<unique>.consumes);
  EXPECT_TRUE(scope_info_of<shared_cyclical>.allows_cycles);
}

} // namespace
} // namespace dingo
