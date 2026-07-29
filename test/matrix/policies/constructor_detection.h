//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include "matrix/common/assertions.h"
#include "matrix/fixtures/constructor_detection.h"
#include "matrix/fixtures/dependency_shapes.h"

#include <cstddef>
#include <memory>
#include <optional>
#include <stdexcept>
#include <type_traits>

namespace dingo::matrix {

using constructor_unique_argument = std::unique_ptr<int>;
using constructor_shared_argument = std::shared_ptr<int>;

struct constructor_argument_value {
  template <typename T> using parameter = T;
  static constexpr const char *expected = "operator T()";
};

struct constructor_argument_lvalue_reference {
  template <typename T> using parameter = T &;
  static constexpr const char *expected = "operator T&()";
};

struct constructor_argument_const_lvalue_reference {
  template <typename T> using parameter = const T &;
  static constexpr const char *expected = "operator const T&()";
};

struct constructor_argument_rvalue_reference {
  template <typename T> using parameter = T &&;
  static constexpr const char *expected = "operator T()";
};

struct constructor_argument_pointer {
  template <typename T> using parameter = T *;
  static constexpr const char *expected = "operator T*()";
};

struct constructor_argument_const_pointer {
  template <typename T> using parameter = const T *;
  static constexpr const char *expected = "operator T*()";
};

template <typename U> struct constructor_argument_rvalue_conversion {
  template <typename T,
            typename = std::enable_if_t<!std::is_same_v<std::decay_t<T>, U>>>
  operator T &&() const {
    throw std::runtime_error("operator T&&()");
  }
};

template <typename U> struct constructor_argument_const_lvalue_conversion {
  template <typename T,
            typename = std::enable_if_t<!std::is_same_v<std::decay_t<T>, U>>>
  operator const T &() const {
    throw std::runtime_error("operator const T&()");
  }
};

template <typename U> struct constructor_argument_lvalue_conversion {
  template <typename T,
            typename = std::enable_if_t<!std::is_same_v<std::decay_t<T>, U>>>
  operator T &() const {
    throw std::runtime_error("operator T&()");
  }
};

template <typename U> struct constructor_argument_value_conversion {
  template <typename T,
            typename = std::enable_if_t<!std::is_same_v<std::decay_t<T>, U>>>
  operator T() {
    throw std::runtime_error("operator T()");
  }
};

template <typename U> struct constructor_argument_pointer_conversion {
  template <typename T,
            typename = std::enable_if_t<!std::is_same_v<std::decay_t<T>, U>>>
  operator T *() {
    throw std::runtime_error("operator T*()");
  }
};

template <typename U>
struct constructor_argument_probe
    : constructor_argument_value_conversion<U>,
      constructor_argument_const_lvalue_conversion<U>,
      constructor_argument_lvalue_conversion<U>,
      constructor_argument_pointer_conversion<U>
#if (defined(_MSC_VER) && DINGO_CXX_STANDARD == 17) ||                         \
    (defined(__GNUC__) && (__GNUC__ == 12 || __GNUC__ == 13))
    ,
      constructor_argument_rvalue_conversion<U>
#endif
{
};

template <typename Dependency, typename Category>
struct constructor_argument_consumer {
  explicit constructor_argument_consumer(
      typename Category::template parameter<Dependency>) {}
};

template <typename Dependency, typename Category>
void assert_constructor_argument_conversion() {
  using target = constructor_argument_consumer<Dependency, Category>;
  try {
    target instance((constructor_argument_probe<target>()));
    (void)instance;
    FAIL() << "constructor argument conversion did not run";
  } catch (const std::runtime_error &error) {
    if constexpr (std::is_same_v<Category,
                                 constructor_argument_rvalue_reference>) {
#if defined(__GNUC__) && (__GNUC__ == 12 || __GNUC__ == 13)
      ASSERT_STREQ(error.what(), "operator T&&()");
#else
      ASSERT_STREQ(error.what(), Category::expected);
#endif
    } else {
      ASSERT_STREQ(error.what(), Category::expected);
    }
  }
}

template <typename T> struct constructor_detection_construction_check {
  template <typename Detection, typename DetectionMode> static void run() {}
};

struct constructor_annotation_probe {};

template <typename Dependency>
using constructor_annotation_argument_t = typename detail::category_result<
    constructor_dependency<Dependency>, constructor_annotation_probe,
    detail::list_initialization, 1, 0, detail::annotated_probe>::type;

using constructor_annotated_pointer =
    annotated<constructor_config *, constructor_selector>;
using constructor_annotated_shared_pointer =
    annotated<std::shared_ptr<constructor_config>, constructor_selector>;

static_assert(std::is_same_v<
              constructor_annotation_argument_t<constructor_annotated_pointer>,
              constructor_annotated_pointer>);
static_assert(
    std::is_same_v<
        constructor_annotation_argument_t<constructor_annotated_shared_pointer>,
        constructor_annotated_shared_pointer>);

template <>
struct constructor_detection_construction_check<constructor_two_values> {
  template <typename Detection, typename DetectionMode> static void run() {
    constructor_detection_context context;

    auto instance = Detection::template construct<constructor_two_values>(
        ephemeral_scope, context, context);
    ASSERT_EQ(instance.integer, 42);
    ASSERT_EQ(instance.real, 2.5f);

    alignas(constructor_two_values) unsigned char
        storage[sizeof(constructor_two_values)];
    Detection::template construct<constructor_two_values>(
        storage, ephemeral_scope, context, context);
    auto *placed = reinterpret_cast<constructor_two_values *>(storage);
    ASSERT_EQ(placed->integer, 42);
    ASSERT_EQ(placed->real, 2.5f);
    placed->~constructor_two_values();

    ASSERT_EQ(context.integer_resolutions, 2);
    ASSERT_EQ(context.float_resolutions, 2);
  }
};

template <>
struct constructor_detection_construction_check<constructor_copy_only_value> {
  template <typename Detection, typename DetectionMode> static void run() {
    if constexpr (std::is_same_v<DetectionMode,
                                 detail::constructor_signature>) {
      constructor_copy_only_context context;
      (void)Detection::template construct<constructor_copy_only_value>(
          ephemeral_scope, context, context);
    }
  }
};

template <typename Dependency>
struct constructor_dependency_construction_check {
  template <typename Detection, typename DetectionMode> static void run() {
    if constexpr (std::is_same_v<DetectionMode, detail::constructor_shape>) {
      using target = constructor_dependency<Dependency>;
      constructor_dependency_context<Dependency> context;

      // Cover both construction dispatches. The context additionally checks
      // that each dispatch resolves the exact outer dependency.
      (void)Detection::template construct<target>(ephemeral_scope, context,
                                                  context);

      alignas(target) unsigned char storage[sizeof(target)];
      Detection::template construct<target>(storage, ephemeral_scope, context,
                                            context);
      reinterpret_cast<target *>(storage)->~target();

      ASSERT_EQ(context.resolutions, 2);
    }
  }
};

template <>
struct constructor_detection_construction_check<
    constructor_dependency<std::optional<dependency_regular *>>>
    : constructor_dependency_construction_check<
          std::optional<dependency_regular *>> {};

template <>
struct constructor_detection_construction_check<constructor_dependency<
    std::optional<dependency_const_pointer<dependency_regular>>>>
    : constructor_dependency_construction_check<
          std::optional<dependency_const_pointer<dependency_regular>>> {};

template <>
struct constructor_detection_construction_check<
    constructor_dependency<std::optional<std::array<dependency_copy_only, 2>>>>
    : constructor_dependency_construction_check<
          std::optional<std::array<dependency_copy_only, 2>>> {};

template <>
struct constructor_detection_construction_check<constructor_dependency<
    std::optional<std::variant<dependency_regular, dependency_copy_only>>>>
    : constructor_dependency_construction_check<std::optional<
          std::variant<dependency_regular, dependency_copy_only>>> {};

template <>
struct constructor_detection_construction_check<constructor_dependency<
    std::variant<std::optional<dependency_regular>, dependency_move_only>>>
    : constructor_dependency_construction_check<std::variant<
          std::optional<dependency_regular>, dependency_move_only>> {};

template <>
struct constructor_detection_construction_check<constructor_dependency<
    std::variant<dependency_const_pointer<dependency_regular>,
                 std::optional<dependency_copy_only>>>>
    : constructor_dependency_construction_check<
          std::variant<dependency_const_pointer<dependency_regular>,
                       std::optional<dependency_copy_only>>> {};

template <typename Backend, typename DetectionMode, typename T,
          detail::constructor_kind ExpectedKind, std::size_t ExpectedArity,
          typename ExpectedArguments>
void assert_constructor_detection() {
  using detection = typename Backend::template detection<T, DetectionMode>;
  static_assert(detection::kind == ExpectedKind);
  static_assert(detection::arity == ExpectedArity);
  static_assert(
      std::is_same_v<typename detection::arguments, ExpectedArguments>);

  if constexpr (std::is_same_v<Backend, constructor_detection_msvc_backend> &&
                std::is_same_v<T, constructor_same_arity_overload>) {
    static_assert(
        !detail::constructor_probe_msvc<
            T, detail::constructor_shape, detail::opaque_constructor_argument,
            detail::list_initialization, detail::invalid_arity>::value);
  }

  constructor_detection_construction_check<T>::template run<detection,
                                                            DetectionMode>();
}

} // namespace dingo::matrix
