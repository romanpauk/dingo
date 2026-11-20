//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#include <dingo/base/exceptions.h>
#include <dingo/binding/resolution.h>
#include <dingo/storage/shared.h>

#include <gtest/gtest.h>

#include <optional>
#include <vector>

namespace dingo {
namespace {
struct immovable {
  immovable() = default;
  immovable(const immovable &) = delete;
  immovable(immovable &&) = delete;
};

struct fake_resolution_context {};

struct destructor_order_value {
  destructor_order_value(int init, std::vector<int> &target_order)
      : value(init), order(&target_order) {}
  ~destructor_order_value() { order->push_back(value); }

  int value;
  std::vector<int> *order;
};

struct destructor_order_array_factory {
  std::vector<int> *order;

  template <typename Type, typename Context, typename Container>
  void construct(void *ptr, construction_scope, Context &, Container &) {
    auto *values = reinterpret_cast<Type *>(ptr);
    for (int i = 0; i != 3; ++i) {
      new (&(*values)[i]) destructor_order_value(i, *order);
    }
  }
};
} // namespace

TEST(resolution_operation_test,
     resolved_immovable_value_is_rejected_without_copy_instantiation) {
  immovable value;
  resolved_address result{&value, resolved_address::access_kind::borrow};

  EXPECT_THROW((void)detail::resolved_value<immovable>(result),
               type_not_convertible_exception);
}

TEST(resolution_operation_test, rvalue_source_destroys_materialized_value) {
  std::vector<int> order;

  {
    auto source = detail::make_rvalue_source<destructor_order_value>(
        std::in_place,
        [&](void *ptr) { new (ptr) destructor_order_value(0, order); });
    (void)source;
  }

  ASSERT_EQ(order, (std::vector<int>{0}));
}

TEST(resolution_operation_test,
     shared_fixed_array_storage_reset_destroys_elements_in_reverse_order) {
  std::vector<int> order;
  using array_type = destructor_order_value[3];
  using storage_type = detail::storage<shared, array_type, array_type,
                                       destructor_order_array_factory>;
  fake_resolution_context context;
  fake_resolution_context container;

  storage_type storage(destructor_order_array_factory{&order});
  storage.resolve(persistent_scope, context, container);
  storage.reset();

  ASSERT_EQ(order, (std::vector<int>{2, 1, 0}));
}
} // namespace dingo
