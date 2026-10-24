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

#include <dingo/primitives/constructor.h>
#include <dingo/primitives/constructor_detection.h>
#include <dingo/primitives/type_name.h>

#include <cstddef>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

namespace dingo::matrix {

using constructor_unique_argument = std::unique_ptr<int>;
using constructor_shared_argument = std::shared_ptr<int>;

// A conversion category of a constructor parameter: the parameter type, and the
// request by which the injected argument resolves it, a reference or a value.
struct constructor_argument_value {
  template <typename T> using parameter = T;
  template <typename T> using request = T;
  static constexpr bool by_reference = false;
};

struct constructor_argument_lvalue_reference {
  template <typename T> using parameter = T &;
  template <typename T> using request = T &;
  static constexpr bool by_reference = true;
};

struct constructor_argument_const_lvalue_reference {
  template <typename T> using parameter = const T &;
  template <typename T> using request = const T &;
  static constexpr bool by_reference = true;
};

struct constructor_argument_rvalue_reference {
  template <typename T> using parameter = T &&;
  template <typename T> using request = T;
  static constexpr bool by_reference = false;
};

struct constructor_argument_pointer {
  template <typename T> using parameter = T *;
  template <typename T> using request = T *;
  static constexpr bool by_reference = false;
};

struct constructor_argument_const_pointer {
  template <typename T> using parameter = const T *;
  template <typename T> using request = const T *;
  static constexpr bool by_reference = false;
};

template <bool ByReference, typename Request>
std::string constructor_argument_request() {
  return (ByReference ? "resolve " : "argument ") +
         core::type_name_of<Request>();
}

// Reports the request of the injected argument by throwing it.
struct constructor_argument_resolver {
  template <typename Request> Request resolve() {
    throw std::runtime_error(constructor_argument_request<true, Request>());
  }

  template <typename Type> Type argument() {
    throw std::runtime_error(constructor_argument_request<false, Type>());
  }
};

template <typename Dependency, typename Category>
struct constructor_argument_consumer {
  explicit constructor_argument_consumer(
      typename Category::template parameter<Dependency>) {}
};

// The detected constructor of a consumer receives the injected argument of the
// core implementation, which requests what the category of its parameter asks.
template <typename Dependency, typename Category>
void assert_constructor_argument_conversion() {
  using target = constructor_argument_consumer<Dependency, Category>;
  constructor_argument_resolver resolver;
  try {
    (void)core::invoke_constructor<target>(resolver, [](auto &&...args) {
      return target{std::forward<decltype(args)>(args)...};
    });
    FAIL() << "constructor argument conversion did not run";
  } catch (const std::runtime_error &error) {
    ASSERT_EQ(std::string(error.what()),
              (constructor_argument_request<
                  Category::by_reference,
                  typename Category::template request<Dependency>>()));
  }
}

template <typename Request, typename Resolver>
Request constructor_detection_argument(Resolver &resolver) {
  if constexpr (std::is_reference_v<Request>) {
    return resolver.template resolve<Request>();
  } else {
    return resolver.template argument<Request>();
  }
}

template <typename T, typename Resolver, typename... Args>
T construct_from_signature(Resolver &resolver,
                           core::type_list<Args...> /*arguments*/) {
  return T{constructor_detection_argument<Args>(resolver)...};
}

template <typename T, typename Resolver, std::size_t... Is>
T construct_from_shape(Resolver &resolver, std::index_sequence<Is...>) {
  return T{((void)Is, core::constructor_argument<T, Resolver>(resolver))...};
}

// Constructs `T` by its detection: by the recovered argument types in the
// signature mode, by injected arguments of the detected arity otherwise.
template <typename Detection, typename DetectionMode, typename T,
          typename Resolver>
T construct_detected(Resolver &resolver) {
  if constexpr (std::is_same_v<DetectionMode, core::constructor_signature>) {
    return construct_from_signature<T>(resolver,
                                       typename Detection::arguments{});
  } else {
    return construct_from_shape<T>(
        resolver, std::make_index_sequence<Detection::arity>{});
  }
}

struct constructor_detection_resolver {
  template <typename T> T argument() {
    static_assert(std::is_same_v<T, int> || std::is_same_v<T, float>);
    if constexpr (std::is_same_v<T, int>) {
      ++integer_resolutions;
      return 42;
    } else {
      ++float_resolutions;
      return 2.5f;
    }
  }

  template <typename Request> Request resolve() {
    static_assert(!std::is_same_v<Request, Request>,
                  "the arguments of constructor_two_values are values");
  }

  int integer_resolutions = 0;
  int float_resolutions = 0;
};

struct constructor_copy_only_resolver {
  template <typename T> T argument() {
    static_assert(std::is_same_v<T, constructor_copy_only_config>);
    return {};
  }

  template <typename Request> Request resolve() {
    static_assert(!std::is_same_v<Request, Request>,
                  "the argument of constructor_copy_only_value is a value");
  }
};

template <typename T> struct constructor_detection_construction_check {
  template <typename Detection, typename DetectionMode> static void run() {}
};

using constructor_annotated_pointer =
    annotated<constructor_config *, constructor_selector>;
using constructor_annotated_shared_pointer =
    annotated<std::shared_ptr<constructor_config>, constructor_selector>;

// The value category of the msvc signature recovery keeps an annotated
// argument as the annotated type.
template <typename Dependency>
using constructor_annotation_argument_t =
    typename core::detail::category_result<constructor_dependency<Dependency>,
                                           1, 0,
                                           core::detail::value_probe>::type;

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
    constructor_detection_resolver resolver;

    auto instance =
        construct_detected<Detection, DetectionMode, constructor_two_values>(
            resolver);
    ASSERT_EQ(instance.integer, 42);
    ASSERT_EQ(instance.real, 2.5f);

    ASSERT_EQ(resolver.integer_resolutions, 1);
    ASSERT_EQ(resolver.float_resolutions, 1);
  }
};

template <>
struct constructor_detection_construction_check<constructor_copy_only_value> {
  template <typename Detection, typename DetectionMode> static void run() {
    if constexpr (std::is_same_v<DetectionMode, core::constructor_signature>) {
      constructor_copy_only_resolver resolver;
      (void)construct_detected<Detection, DetectionMode,
                               constructor_copy_only_value>(resolver);
    }
  }
};

template <typename Backend, typename DetectionMode, typename T,
          core::constructor_kind ExpectedKind, std::size_t ExpectedArity,
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
        !core::detail::msvc_probe<T, core::detail::opaque_argument,
                                  core::detail::invalid_arity>::value);
  }

  constructor_detection_construction_check<T>::template run<detection,
                                                            DetectionMode>();
}

} // namespace dingo::matrix
