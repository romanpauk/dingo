//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#include <dingo/core/binding_resolution.h>
#include <dingo/core/exceptions.h>
#include <dingo/resolution/resolution_operation.h>
#include <dingo/storage/shared.h>

#include <gtest/gtest.h>

#include <optional>
#include <variant>
#include <vector>

namespace resolution_operation_test {
struct conversion_source {};
struct conversion_result {};

struct conversion_target {
  explicit conversion_target(conversion_result) : value(53) {}

  int value;
};
} // namespace resolution_operation_test

namespace dingo {
template <>
struct type_conversion_traits<resolution_operation_test::conversion_target,
                              resolution_operation_test::conversion_source> {
  template <typename> using required_access = borrow;

  static resolution_operation_test::conversion_result
  convert(const resolution_operation_test::conversion_source &) {
    return {};
  }
};

namespace {
struct alternative_a {
  explicit alternative_a(int init) : value(init) {}
  int value;
};

struct alternative_b {
  explicit alternative_b(float init) : value(init) {}
  float value;
};

struct unrelated_alternative {
  int value;
};

struct immovable {
  immovable() = default;
  immovable(const immovable &) = delete;
  immovable(immovable &&) = delete;
};

using alternative_type = std::variant<alternative_a, alternative_b>;

struct alternative_factory {
  alternative_type &value;

  template <typename Context> auto &resolve(Context &) { return value; }
};

struct alternative_pointer_factory {
  alternative_type *value;

  template <typename Context> auto *resolve(Context &) { return value; }
};

struct alternative_storage {
  using type = alternative_type;
  using resolved_type = alternative_type &;
};

struct resolution_context {};

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
     alternative_projectors_reject_unrelated_targets_with_runtime_exception) {
  alternative_type value(std::in_place_type<alternative_a>, 7);
  alternative_factory lvalue_factory{value};
  alternative_pointer_factory pointer_factory{&value};
  resolution_context context;

  using reference_operation =
      detail::type_resolution<unrelated_alternative &, alternative_type &,
                              detail::type_conversion_path_t<
                                  unrelated_alternative &, alternative_type &>>;
  ASSERT_THROW((reference_operation::apply<alternative_storage>(
                   lvalue_factory, context, detail::make_lvalue_source(value),
                   describe_type<unrelated_alternative &>(),
                   describe_type<alternative_type>())),
               type_not_convertible_exception);

  using pointer_operation =
      detail::type_resolution<unrelated_alternative *, alternative_type *,
                              detail::type_conversion_path_t<
                                  unrelated_alternative *, alternative_type *>>;
  ASSERT_THROW(
      (pointer_operation::apply<alternative_storage>(
          pointer_factory, context, detail::make_pointer_source(&value),
          describe_type<unrelated_alternative *>(),
          describe_type<alternative_type *>())),
      type_not_convertible_exception);
}

TEST(resolution_operation_test,
     resolved_immovable_value_is_rejected_without_copy_instantiation) {
  immovable value;
  resolved_address result{&value, resolved_address::access_kind::borrow};

  EXPECT_THROW((void)detail::convert_resolved_binding<immovable>(result),
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
     nested_conversions_publish_retained_cache_types) {
  using retained = detail::retained_type_conversion<
      std::shared_ptr<int> &, int &,
      detail::traits_type_conversion<std::shared_ptr<int>, int &>>;
  using borrowed =
      detail::borrowed_type_conversion<std::shared_ptr<int> &, int &, retained>;
  using alternative = detail::alternative_type_conversion<
      std::shared_ptr<int> &, alternative_type &,
      type_list<
          detail::alternative_type_conversion_branch<alternative_a, borrowed>>>;
  using wrapped =
      detail::wrapper_type_conversion<std::optional<std::shared_ptr<int>>,
                                      int &, retained, false>;
  using selected = detail::target_alternative_type_conversion<
      std::variant<std::shared_ptr<int>, int>, int &, std::shared_ptr<int>,
      retained>;
  using construction = detail::converted_construction<selected>;

  static_assert(
      std::is_same_v<typename detail::conversion_cache_types<borrowed>::type,
                     type_list<std::shared_ptr<int>>>);
  static_assert(
      std::is_same_v<typename detail::conversion_cache_types<alternative>::type,
                     type_list<std::shared_ptr<int>>>);
  static_assert(
      std::is_same_v<typename detail::conversion_cache_types<wrapped>::type,
                     type_list<std::shared_ptr<int>>>);
  static_assert(
      std::is_same_v<typename detail::conversion_cache_types<selected>::type,
                     type_list<std::shared_ptr<int>>>);
  static_assert(std::is_same_v<
                typename detail::conversion_cache_types<construction>::type,
                type_list<std::shared_ptr<int>>>);
}

TEST(resolution_operation_test,
     traits_conversion_explicitly_constructs_the_target_from_its_result) {
  using source = resolution_operation_test::conversion_source;
  using target = resolution_operation_test::conversion_target;
  using conversion = detail::type_conversion_path_t<target, const source &>;
  static_assert(conversion::available);

  source value;
  resolution_context resolver;
  resolution_context context;
  auto converted = detail::type_conversion<conversion>::apply(
      resolver, context, value, describe_type<target>(),
      describe_type<source>());

  EXPECT_EQ(converted.value, 53);
}

TEST(resolution_operation_test,
     shared_fixed_array_storage_reset_destroys_elements_in_reverse_order) {
  std::vector<int> order;
  using array_type = destructor_order_value[3];
  using storage_type =
      detail::storage<shared, array_type, array_type,
                      destructor_order_array_factory,
                      detail::conversions<shared, array_type, array_type>>;
  resolution_context context;
  resolution_context container;

  storage_type storage(destructor_order_array_factory{&order});
  storage.resolve(persistent_scope, context, container);
  storage.reset();

  ASSERT_EQ(order, (std::vector<int>{2, 1, 0}));
}
} // namespace dingo
