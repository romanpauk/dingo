//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#include <dingo/core/dependency.h>
#include <dingo/registration/collection_traits.h>
#include <dingo/storage/interface_storage_traits.h>
#include <dingo/type/dependency_traits.h>
#include <dingo/type/type_traits.h>

#include <gtest/gtest.h>

#include <map>
#include <memory>
#include <vector>

#include "support/class.h"
#include "support/custom_wrappers.h"

namespace dingo {
namespace {
struct request_type_tag;
struct request_type_service;

struct non_default_copy_only {
  explicit non_default_copy_only(int init_value) : value(init_value) {}
  non_default_copy_only(const non_default_copy_only &) = default;
  non_default_copy_only(non_default_copy_only &&) = delete;

  int value;
};
} // namespace

static_assert(
    !std::is_copy_constructible_v<detail::recursion_guard<int, false>>);
static_assert(
    !std::is_move_constructible_v<detail::recursion_guard<int, false>>);

static_assert(is_interface_storage_rebindable_v<test_shared<Class>, IClass>);
static_assert(is_interface_storage_rebindable_v<test_unique<Class>, IClass>);
static_assert(is_copy_constructible_v<test_shared<Class>>);
static_assert(!is_copy_constructible_v<test_unique<Class>>);
static_assert(is_copy_constructible_v<test_optional<Class>>);
static_assert(is_copy_constructible_v<std::vector<test_shared<Class>>>);
static_assert(!is_copy_constructible_v<std::vector<test_unique<Class>>>);
static_assert(!is_copy_constructible_v<std::map<int, std::unique_ptr<Class>>>);

static_assert(std::is_empty_v<request_type<int>>);
static_assert(std::is_same_v<request_type<int>::user_type, int>);
static_assert(std::is_same_v<request_type<int>::lookup_type, int>);
static_assert(std::is_same_v<request_type<int>::interface_type, int>);
static_assert(std::is_same_v<request_type<int>::result_type, int>);
static_assert(std::is_same_v<request_type<int>::value_type, int>);
static_assert(std::is_same_v<request_type<int>::exact_type, int>);
static_assert(!request_type<int>::removes_rvalue_references);
static_assert(!request_type<int>::is_rvalue_request);

static_assert(std::is_same_v<request_type<int &>::lookup_type, int &>);
static_assert(std::is_same_v<request_type<int &>::interface_type, int &>);
static_assert(std::is_same_v<request_type<int &>::value_type, int>);
static_assert(std::is_same_v<request_type<int &>::exact_type, int>);

static_assert(std::is_same_v<request_type<int &&>::lookup_type, int &&>);
static_assert(std::is_same_v<request_type<int &&>::interface_type, int &&>);
static_assert(std::is_same_v<request_type<int &&>::result_type, int &&>);
static_assert(std::is_same_v<request_type<int &&>::value_type, int>);
static_assert(std::is_same_v<request_type<int &&>::exact_type, int>);
static_assert(request_type<int &&>::is_rvalue_request);
static_assert(std::is_same_v<request_type<int &&, true>::lookup_type, int>);
static_assert(std::is_same_v<request_type<int &&, true>::interface_type, int>);
static_assert(std::is_same_v<request_type<int &&, true>::result_type, int>);
static_assert(request_type<int &&, true>::removes_rvalue_references);
static_assert(request_type<int &&, true>::is_rvalue_request);

static_assert(std::is_same_v<request_type<int *>::lookup_type, int *>);
static_assert(std::is_same_v<request_type<int *>::interface_type, int *>);
static_assert(std::is_same_v<request_type<int *>::value_type, int>);
static_assert(std::is_same_v<request_type<int *>::exact_type, int *>);

static_assert(std::is_same_v<request_type<int[3]>::value_type, int>);
static_assert(std::is_same_v<request_type<int (&)[3]>::value_type, int[3]>);

using annotated_request = annotated<request_type_service &, request_type_tag>;
static_assert(std::is_same_v<request_type<annotated_request>::lookup_type,
                             annotated_request>);
static_assert(std::is_same_v<request_type<annotated_request>::interface_type,
                             request_type_service &>);
static_assert(
    std::is_same_v<request_type<annotated_request>::value_type,
                   annotated<request_type_service, request_type_tag>>);
static_assert(std::is_same_v<request_type<annotated_request>::exact_type,
                             request_type_service>);

using copyable_annotated_request =
    annotated<std::shared_ptr<request_type_service>, request_type_tag>;
using move_only_annotated_request =
    annotated<std::unique_ptr<request_type_service>, request_type_tag>;
using non_default_annotated_request =
    annotated<non_default_copy_only, request_type_tag>;
static_assert(std::is_copy_constructible_v<non_default_copy_only>);
static_assert(!std::is_move_constructible_v<non_default_copy_only>);
static_assert(
    std::is_constructible_v<copyable_annotated_request,
                            const std::shared_ptr<request_type_service> &>);
static_assert(
    !std::is_constructible_v<move_only_annotated_request,
                             const std::unique_ptr<request_type_service> &>);
static_assert(std::is_constructible_v<non_default_annotated_request,
                                      const non_default_copy_only &>);

TEST(type_traits_test,
     annotated_copies_non_default_constructible_copy_only_values) {
  const non_default_copy_only value(17);

  non_default_annotated_request request(value);

  EXPECT_EQ(static_cast<non_default_copy_only>(request).value, 17);
}

using keyed_request =
    dependency<request_type_service &, key_type<request_type_tag>>;
static_assert(
    std::is_same_v<request_type<keyed_request>::lookup_type, keyed_request>);
static_assert(std::is_same_v<request_type<keyed_request>::interface_type,
                             request_type_service &>);
static_assert(std::is_same_v<
              request_type<keyed_request>::value_type,
              dependency<request_type_service, key_type<request_type_tag>>>);
static_assert(std::is_same_v<request_type<keyed_request>::exact_type,
                             request_type_service>);

static_assert(std::is_same_v<
              request_type<std::shared_ptr<request_type_service>>::value_type,
              request_type_service>);
static_assert(std::is_same_v<
              request_type<std::unique_ptr<request_type_service>>::value_type,
              request_type_service>);

} // namespace dingo
