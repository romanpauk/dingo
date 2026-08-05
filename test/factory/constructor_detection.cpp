//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#include <dingo/container.h>
#include <dingo/factory/constructor_detection.h>
#include <dingo/storage/external.h>

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <memory>
#include <optional>
#include <type_traits>
#include <utility>
#include <variant>

#include "matrix/fixtures/constructor_detection.h"

namespace dingo {

static_assert(constructor<int>::arity == 0);
static_assert(constructor<const int>::arity ==
              constructor_detection<const int>::arity);
static_assert(constructor<volatile int>::arity ==
              constructor_detection<volatile int>::arity);

namespace {

struct empty_aggregate {};
struct empty_aggregate_base {};
struct empty_derived_aggregate : empty_aggregate_base {};
struct annotated_empty_aggregate {
  using dingo_constructor_type = constructor<annotated_empty_aggregate(int)>;
};
struct incomplete_aggregate;

static_assert(detail::is_zero_argument_aggregate_v<empty_aggregate>);
static_assert(!detail::is_zero_argument_aggregate_v<empty_derived_aggregate>);
static_assert(!detail::is_zero_argument_aggregate_v<annotated_empty_aggregate>);
static_assert(!detail::is_zero_argument_aggregate_v<incomplete_aggregate>);
static_assert(constructor<empty_aggregate>::arity == 0);
static_assert(constructor<empty_derived_aggregate>::arity == 1);
static_assert(constructor<annotated_empty_aggregate>::arity == 1);

} // namespace

#if !defined(_MSC_VER)
struct unresolved_argument_category {};
#endif

struct selected_alternative {
  explicit selected_alternative(int init) : value(init) {}

  int value;
};

struct unselected_alternative {};

struct immovable {
  explicit immovable(int init) : value(init) {}
  immovable(const immovable &) = delete;
  immovable(immovable &&) = delete;

  int value;
};

struct array_dependency {
  explicit array_dependency(int init) : value(init) {}
  array_dependency(const array_dependency &) = delete;
  array_dependency(array_dependency &&) = default;

  int value;
};

using array_dependency_pair = std::array<array_dependency, 2>;

struct array_construction_context {
  template <typename T>
  T resolve(construction_scope scope, array_construction_context &container) {
    if constexpr (std::is_same_v<T, array_dependency>) {
      ++resolutions;
      return array_dependency{7};
    } else {
      static_assert(std::is_same_v<T, array_dependency_pair>);
      return constructor_detection<T>::template construct<T>(scope, *this,
                                                             container);
    }
  }

  int resolutions = 0;
};

TEST(constructor_detection_test, public_constructor_argument_metadata) {
  struct explicit_constructor {
    explicit_constructor(int, float) {}
  };

  static_assert(
      std::is_same_v<
          typename constructor<explicit_constructor(int, float)>::arguments,
          type_list<int, float>>);
  static_assert(std::is_same_v<typename constructor_detection<
                                   matrix::constructor_two_values,
                                   detail::constructor_signature>::arguments,
                               type_list<int, float>>);
  static_assert(
      std::is_same_v<typename constructor_detection<
                         matrix::constructor_typedef_selected>::arguments,
                     type_list<double, const char *>>);
  static_assert(std::is_same_v<typename constructor_detection<
                                   matrix::constructor_defaulted>::arguments,
                               type_list<>>);
}

#if !defined(_MSC_VER)
TEST(constructor_detection_test,
     unresolved_signature_arguments_fall_back_to_void) {
  using arguments = typename detail::constructor_signature_arguments<
      matrix::constructor_two_values, unresolved_argument_category,
      std::make_index_sequence<2>>::type;
  static_assert(std::is_void_v<arguments>);
}
#endif

TEST(constructor_detection_test, traits_limit_only_the_public_search) {
  using target = matrix::constructor_trait_limited;
  static_assert(constructor_detection<target>::arity == 2);
  static_assert(constructor_detection<target>::kind ==
                detail::constructor_kind::concrete);

#if defined(_MSC_VER)
  static_assert(
      detail::constructor_detection_msvc<target, detail::constructor_shape,
                                         detail::list_initialization,
                                         3>::arity == 3);
#else
  static_assert(
      detail::constructor_detection<target, detail::constructor_shape,
                                    detail::list_initialization, 3>::arity ==
      3);
#endif
}

TEST(constructor_detection_test, public_detection_defaults_to_shape) {
  static_assert(
      std::is_same_v<constructor_detection<matrix::constructor_two_values>,
                     constructor_detection<matrix::constructor_two_values,
                                           detail::constructor_shape>>);
}

TEST(constructor_detection_test, auto_construction_uses_public_selection) {
  container<> container;
  EXPECT_EQ(
      container.construct<matrix::constructor_public_selected_consumer>().value,
      1);
}

TEST(constructor_detection_test,
     array_traits_construct_move_only_elements_in_place) {
  using array_type = std::array<std::unique_ptr<int>, 2>;
  alignas(array_type) std::byte storage[sizeof(array_type)];

  constructor_traits<array_type>::construct(static_cast<void *>(storage),
                                            std::make_unique<int>(7),
                                            std::make_unique<int>(11));
  auto *values = reinterpret_cast<array_type *>(storage);

  ASSERT_NE((*values)[0], nullptr);
  EXPECT_EQ(*(*values)[0], 7);
  ASSERT_NE((*values)[1], nullptr);
  EXPECT_EQ(*(*values)[1], 11);
  std::destroy_at(values);
}

TEST(constructor_detection_test,
     array_traits_construct_nested_move_only_elements_in_place) {
  using inner_array_type = std::array<std::unique_ptr<int>, 2>;
  using array_type = std::array<inner_array_type, 2>;
  alignas(array_type) std::byte storage[sizeof(array_type)];

  constructor_traits<array_type>::construct(
      static_cast<void *>(storage),
      inner_array_type{std::make_unique<int>(1), std::make_unique<int>(2)},
      inner_array_type{std::make_unique<int>(3), std::make_unique<int>(4)});
  auto *values = reinterpret_cast<array_type *>(storage);

  EXPECT_EQ(*(*values)[0][0], 1);
  EXPECT_EQ(*(*values)[0][1], 2);
  EXPECT_EQ(*(*values)[1][0], 3);
  EXPECT_EQ(*(*values)[1][1], 4);
  std::destroy_at(values);
}

TEST(constructor_detection_test,
     array_traits_construct_nested_move_only_values_in_place) {
  using inner_array_type = std::array<matrix::dependency_move_only, 2>;
  using array_type = std::array<inner_array_type, 2>;
  alignas(array_type) std::byte storage[sizeof(array_type)];

  constructor_traits<array_type>::construct(
      static_cast<void *>(storage), inner_array_type{}, inner_array_type{});
  auto *values = reinterpret_cast<array_type *>(storage);

  EXPECT_EQ((*values)[0][0].marker(), 3);
  EXPECT_EQ((*values)[0][1].marker(), 3);
  EXPECT_EQ((*values)[1][0].marker(), 3);
  EXPECT_EQ((*values)[1][1].marker(), 3);
  std::destroy_at(values);
}

TEST(constructor_detection_test,
     array_shape_constructs_typed_move_only_elements_in_place) {
  using array_type = std::array<array_dependency, 2>;
  alignas(array_type) std::byte storage[sizeof(array_type)];
  array_construction_context context;

  constructor_detection<array_type>::construct<array_type>(
      storage, ephemeral_scope, context, context);
  auto *values = reinterpret_cast<array_type *>(storage);

  EXPECT_EQ((*values)[0].value, 7);
  EXPECT_EQ((*values)[1].value, 7);
  EXPECT_EQ(context.resolutions, 2);
  std::destroy_at(values);
}

TEST(constructor_detection_test,
     nested_array_shape_constructs_typed_move_only_elements_in_place) {
  using array_type = std::array<array_dependency_pair, 2>;
  alignas(array_type) std::byte storage[sizeof(array_type)];
  array_construction_context context;

  constructor_detection<array_type>::construct<array_type>(
      storage, ephemeral_scope, context, context);
  auto *values = reinterpret_cast<array_type *>(storage);

  EXPECT_EQ((*values)[0][0].value, 7);
  EXPECT_EQ((*values)[0][1].value, 7);
  EXPECT_EQ((*values)[1][0].value, 7);
  EXPECT_EQ((*values)[1][1].value, 7);
  EXPECT_EQ(context.resolutions, 4);
  std::destroy_at(values);
}

TEST(constructor_detection_test,
     selected_alternative_uses_the_conversion_operation) {
  using alternative =
      std::variant<selected_alternative, unselected_alternative>;
  using nested = std::optional<alternative>;
  using deeply_nested = std::optional<nested>;

  container<> container;
  container.register_type<scope<external>, storage<int>>(7);

  auto value = container.construct<nested, constructor<selected_alternative>>();
  ASSERT_TRUE(value.has_value());
  ASSERT_TRUE(std::holds_alternative<selected_alternative>(*value));
  EXPECT_EQ(std::get<selected_alternative>(*value).value, 7);

  auto deeply_nested_value =
      container.construct<deeply_nested, constructor<selected_alternative>>();
  ASSERT_TRUE(deeply_nested_value.has_value());
  ASSERT_TRUE(deeply_nested_value->has_value());
  ASSERT_TRUE(
      std::holds_alternative<selected_alternative>(**deeply_nested_value));
  EXPECT_EQ(std::get<selected_alternative>(**deeply_nested_value).value, 7);

  auto *pointer =
      container.construct<alternative *, constructor<selected_alternative>>();
  ASSERT_NE(pointer, nullptr);
  EXPECT_EQ(std::get<selected_alternative>(*pointer).value, 7);
  delete pointer;
}

TEST(constructor_detection_test,
     ordinary_wrapper_construction_remains_in_place) {
  auto value = detail::construction_dispatch<std::optional<immovable>,
                                             immovable>::construct(11);
  ASSERT_TRUE(value.has_value());
  EXPECT_EQ(value->value, 11);
}

TEST(constructor_detection_test, selected_conversion_route_is_executed) {
  using target = std::variant<int, long>;
  using conversion = detail::target_alternative_type_conversion<
      target, int &&, long,
      detail::type_conversion_path_t<long, int &&, consume>>;
  using dispatch =
      detail::construction_dispatch<target, int,
                                    detail::converted_construction<conversion>>;

  auto value = dispatch::construct(7);

  ASSERT_TRUE(std::holds_alternative<long>(value));
  EXPECT_EQ(std::get<long>(value), 7);
}

} // namespace dingo
