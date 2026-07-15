//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include "matrix/policies/policy_core.h"

#include <cstddef>
#include <memory>
#include <variant>

namespace dingo::matrix::resolution {

template <typename Type> struct raw_array_ptr {
  template <typename Container> static void check(Container &container) {
    auto *values = container.template resolve<Type *>();
    ASSERT_TRUE(is_constructed_value(values[0]));
    ASSERT_TRUE(is_constructed_value(values[1]));
  }
};

template <typename Type, std::size_t Size> struct raw_array_ref {
  template <typename Container> static void check(Container &container) {
    auto &values = container.template resolve<Type(&)[Size]>();
    ASSERT_TRUE(is_constructed_value(values[0]));
    ASSERT_TRUE(is_constructed_value(values[Size - 1]));
  }
};

template <typename Type, std::size_t Size> struct raw_array_ptr_to_array {
  template <typename Container> static void check(Container &container) {
    auto values = container.template resolve<Type(*)[Size]>();
    ASSERT_TRUE(is_constructed_value((*values)[0]));
    ASSERT_TRUE(is_constructed_value((*values)[Size - 1]));
  }
};

template <typename Type, std::size_t Rows, std::size_t Columns>
struct raw_nd_array_ref {
  template <typename Container> static void check(Container &container) {
    auto &values = container.template resolve<Type(&)[Rows][Columns]>();
    ASSERT_TRUE(is_constructed_value(values[0][0]));
    ASSERT_TRUE(is_constructed_value(values[Rows - 1][Columns - 1]));
  }
};

template <typename Type> struct unique_array {
  template <typename Container> static void check(Container &container) {
    auto values = container.template resolve<std::unique_ptr<Type[]> &&>();
    ASSERT_TRUE(is_constructed_value(values[0]));
    ASSERT_TRUE(is_constructed_value(values[1]));
  }
};

template <typename Type> struct shared_array {
  template <typename Container> static void check(Container &container) {
    auto values = container.template resolve<std::shared_ptr<Type[]>>();
    ASSERT_TRUE(is_constructed_value(values[0]));
    ASSERT_TRUE(is_constructed_value(values[1]));
  }
};

template <typename Handle, typename Unique, typename Type>
struct shared_unique_ref {
  template <typename Container> static void check(Container &container) {
    auto &instance = container.template resolve<Handle &>();
    ASSERT_TRUE(instance);
    ASSERT_TRUE(*instance);
    ASSERT_TRUE(is_constructed_value(**instance));
    auto handle = container.template resolve<Handle>();
    ASSERT_TRUE(handle);
    ASSERT_TRUE(*handle);
    ASSERT_TRUE(is_constructed_value(**handle));
    auto &unique_handle = container.template resolve<Unique &>();
    ASSERT_TRUE(unique_handle);
    ASSERT_TRUE(is_constructed_value(*unique_handle));
    auto &leaf = container.template resolve<Type &>();
    ASSERT_TRUE(is_constructed_value(leaf));
  }
};

template <typename Handle, typename UniqueArray, typename Type>
struct shared_unique_array_ref {
  template <typename Container> static void check(Container &container) {
    auto &instance = container.template resolve<Handle &>();
    ASSERT_TRUE(instance);
    ASSERT_TRUE(*instance);
    ASSERT_TRUE(is_constructed_value((*instance)[0]));
    ASSERT_TRUE(is_constructed_value((*instance)[1]));
    auto handle = container.template resolve<Handle>();
    ASSERT_TRUE(handle);
    ASSERT_TRUE(*handle);
    ASSERT_TRUE(is_constructed_value((*handle)[0]));
    auto &unique_array = container.template resolve<UniqueArray &>();
    ASSERT_TRUE(unique_array);
    ASSERT_TRUE(is_constructed_value(unique_array[0]));
    auto *first = container.template resolve<Type *>();
    ASSERT_TRUE(is_constructed_value(*first));
  }
};

template <typename Handle> struct nested_unique_value {
  template <typename Container> static void check(Container &container) {
    auto instance = container.template resolve<Handle &&>();
    ASSERT_TRUE(instance);
    ASSERT_TRUE(*instance);
    ASSERT_TRUE(is_constructed_value(**instance));
  }
};

template <typename Variant, typename Alternative> struct variant_value {
  template <typename Container> static void check(Container &container) {
    auto instance = container.template resolve<Variant>();
    ASSERT_TRUE(std::holds_alternative<Alternative>(instance));
    ASSERT_EQ(std::get<Alternative>(instance).value, 3);
  }
};

template <typename Variant, typename Alternative> struct variant_unique {
  template <typename Container> static void check(Container &container) {
    auto instance = container.template resolve<Variant>();
    ASSERT_TRUE(std::holds_alternative<Alternative>(instance));
    auto &value = std::get<Alternative>(instance);
    ASSERT_TRUE(value);
    ASSERT_TRUE(is_constructed_value(*value));
  }
};

template <typename Variant, typename Alternative> struct variant_shared_ref {
  template <typename Container> static void check(Container &container) {
    auto &instance = container.template resolve<Variant &>();
    ASSERT_TRUE(std::holds_alternative<Alternative>(instance));
    auto value = std::get<Alternative>(instance);
    ASSERT_TRUE(value);
    ASSERT_TRUE(is_constructed_value(*value));
    auto alternative = container.template resolve<Alternative>();
    ASSERT_TRUE(alternative);
    ASSERT_TRUE(is_constructed_value(*alternative));
  }
};

template <typename Variant, typename Handle, typename Unique, typename Type>
struct variant_shared_unique_ref {
  template <typename Container> static void check(Container &container) {
    auto &instance = container.template resolve<Variant &>();
    ASSERT_TRUE(std::holds_alternative<Handle>(instance));
    auto handle = std::get<Handle>(instance);
    ASSERT_TRUE(handle);
    ASSERT_TRUE(*handle);
    ASSERT_TRUE(is_constructed_value(**handle));
    auto resolved_handle = container.template resolve<Handle>();
    ASSERT_TRUE(resolved_handle);
    ASSERT_TRUE(*resolved_handle);
    ASSERT_TRUE(is_constructed_value(**resolved_handle));
    auto &unique_handle = container.template resolve<Unique &>();
    ASSERT_TRUE(unique_handle);
    ASSERT_TRUE(is_constructed_value(*unique_handle));
    auto &leaf = container.template resolve<Type &>();
    ASSERT_TRUE(is_constructed_value(leaf));
  }
};

template <typename Handle, typename Alternative> struct unique_variant {
  template <typename Container> static void check(Container &container) {
    auto instance = container.template resolve<Handle &&>();
    ASSERT_TRUE(instance);
    ASSERT_TRUE(std::holds_alternative<Alternative>(*instance));
    ASSERT_EQ(std::get<Alternative>(*instance).value, 3);
  }
};

template <typename Handle, typename Alternative> struct shared_variant_ref {
  template <typename Container> static void check(Container &container) {
    auto &instance = container.template resolve<Handle &>();
    ASSERT_TRUE(instance);
    ASSERT_TRUE(std::holds_alternative<Alternative>(*instance));
    auto &alternative = container.template resolve<Alternative &>();
    ASSERT_EQ(alternative.value, 3);
  }
};

template <typename Variant, typename Alternative> struct array_variant_ref {
  template <typename Container> static void check(Container &container) {
    auto &instance = container.template resolve<Variant &>();
    ASSERT_TRUE(std::holds_alternative<Alternative>(instance));
    auto &values = std::get<Alternative>(instance);
    ASSERT_TRUE(is_constructed_value(values[0]));
    ASSERT_TRUE(is_constructed_value(values[1]));
  }
};

template <typename Request> struct annotated_value {
  template <typename Container> static void check(Container &container) {
    decltype(auto) instance = container.template resolve<Request>();
    ASSERT_TRUE(is_constructed_value(instance));
  }
};

template <typename Request> struct annotated_pointee {
  template <typename Container> static void check(Container &container) {
    decltype(auto) instance = container.template resolve<Request>();
    ASSERT_TRUE(is_constructed_value(*instance));
  }
};

} // namespace dingo::matrix::resolution
