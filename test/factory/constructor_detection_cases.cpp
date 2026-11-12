//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

// What both detection backends report about a constructor, in both modes, and
// what constructing with the detected constructor requests. The portable
// backend is the one GCC and Clang use; the MSVC backend is instantiated on
// every compiler so that both algorithms are checked where the tests run.

#include <dingo/container.h>
#include <dingo/factory/constructor_detection.h>
#include <dingo/factory/detail/constructor_detection_msvc.hpp>

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <initializer_list>
#include <memory>
#include <new>
#include <optional>
#include <type_traits>
#include <utility>
#include <variant>

#include "factory/constructor_detection_fixture.h"

namespace dingo {
namespace {

using detail::constructor_kind;
using detail::constructor_shape;
using detail::constructor_signature;

constexpr size_t max_arity = DINGO_CONSTRUCTOR_DETECTION_ARGS;

template <typename T, typename Mode>
using msvc_detection =
    detail::constructor_detection_msvc<T, Mode, detail::list_initialization,
                                       max_arity>;
#if !defined(_MSC_VER)
template <typename T, typename Mode>
using portable_detection =
    detail::constructor_detection<T, Mode, detail::list_initialization,
                                  max_arity>;
#endif

// What both modes of a backend report: the kind and arity, and the arguments,
// which shape detection lists only for a concrete constructor without any and
// signature detection recovers for every concrete one. shape_only skips the
// signature.
struct shape_only;

template <template <typename, typename> class Detection, typename T,
          constructor_kind Kind, size_t Arity, typename Arguments>
constexpr bool detects_with() {
  using shape = Detection<T, constructor_shape>;
  static_assert(shape::kind == Kind && shape::arity == Arity);
  static_assert(
      std::is_same_v<
          typename shape::arguments,
          std::conditional_t<Kind == constructor_kind::concrete && Arity == 0,
                             type_list<>, void>>);
  if constexpr (!std::is_same_v<Arguments, shape_only>) {
    using signature = Detection<T, constructor_signature>;
    static_assert(signature::kind == Kind && signature::arity == Arity);
    static_assert(std::is_same_v<typename signature::arguments, Arguments>);
  }
  return true;
}

template <typename T, constructor_kind Kind, size_t Arity, typename Arguments>
constexpr bool detects() {
#if !defined(_MSC_VER)
  static_assert(detects_with<portable_detection, T, Kind, Arity, Arguments>());
#endif
  return detects_with<msvc_detection, T, Kind, Arity, Arguments>();
}

struct leaf {};
struct copy_only {
  copy_only() = default;
  copy_only(const copy_only &) = default;
  copy_only(copy_only &&) = delete;
};
using move_only = fixture::dependency_move_only;
struct immovable {
  immovable() = default;
  immovable(const immovable &) = delete;
  immovable(immovable &&) = delete;
};
struct tag {};
struct incomplete;

template <typename T>
using selected_t = detail::selected<T, detail::type_selector<tag>>;

template <typename P> struct takes {
  explicit takes(P) {}
};

// A concrete constructor of each parameter, detected as that parameter.
template <typename... Ps> constexpr bool detects_parameters(type_list<Ps...>) {
  return (detects<takes<Ps>, constructor_kind::concrete, 1, type_list<Ps>>() &&
          ...);
}

template <typename... Ps>
constexpr bool detects_parameter_shapes(type_list<Ps...>) {
  return (detects<takes<Ps>, constructor_kind::concrete, 1, shape_only>() &&
          ...);
}

TEST(constructor_detection_cases, parameter_categories_are_recovered) {
  static_assert(detects_parameters(
      type_list<int, leaf, leaf &, const leaf &, leaf *, const leaf *,
                std::shared_ptr<leaf>, std::shared_ptr<leaf> &,
                std::unique_ptr<leaf>, std::optional<leaf> &, leaf(&)[2],
                copy_only, move_only>{}));
}

// Known limitation: detection cannot tell an rvalue reference from a value,
// so the dependency requested is the value.
TEST(constructor_detection_cases, rvalue_reference_is_recovered_as_a_value) {
  static_assert(detects<takes<leaf &&>, constructor_kind::concrete, 1,
                        type_list<leaf>>());
}

TEST(constructor_detection_cases, selected_and_annotated_keep_their_wrapper) {
  static_assert(detects_parameters(
      type_list<selected_t<leaf>, selected_t<leaf &>, annotated<leaf &, tag>,
                annotated<leaf *, tag>, annotated<std::shared_ptr<leaf>, tag>,
                annotated<std::shared_ptr<leaf> &, tag>>{}));
}

TEST(constructor_detection_cases, references_to_incomplete_types) {
  static_assert(
      detects_parameters(type_list<incomplete &, selected_t<incomplete &>>{}));
}

struct value_and_reference_aggregate {
  int number;
  leaf &reference;
};

TEST(constructor_detection_cases, several_parameters_are_recovered_in_order) {
  static_assert(
      detects<fixture::constructor_two_values, constructor_kind::concrete, 2,
              type_list<int, float>>());
  static_assert(
      detects<value_and_reference_aggregate, constructor_kind::concrete, 2,
              type_list<int, leaf &>>());
}

TEST(constructor_detection_cases, std_array_is_detected_by_its_elements) {
  using elements = std::array<move_only, 2>;
  static_assert(detects<elements, constructor_kind::concrete, 2,
                        type_list<move_only, move_only>>());
  static_assert(detects<std::array<elements, 2>, constructor_kind::concrete, 2,
                        type_list<elements, elements>>());
}

TEST(constructor_detection_cases, default_construction_has_no_arguments) {
  static_assert(detects<fixture::constructor_defaulted,
                        constructor_kind::concrete, 0, type_list<>>());
}

struct generic_constructor {
  template <typename... Args> generic_constructor(Args &&...) {}
};

TEST(constructor_detection_cases, generic_constructor_is_generic) {
  static_assert(detects<generic_constructor, constructor_kind::generic,
                        max_arity, void>());
}

struct ambiguous_constructor {
  explicit ambiguous_constructor(int) {}
  explicit ambiguous_constructor(float) {}
};

struct same_arity_overload {
  same_arity_overload(int, float) {}
  same_arity_overload(float, int) {}
};

TEST(constructor_detection_cases, unranked_overloads_are_invalid) {
  static_assert(
      detects<ambiguous_constructor, constructor_kind::invalid, 0, void>());
  static_assert(
      detects<same_arity_overload, constructor_kind::invalid, 0, void>());
  // MSVC may instantiate the generic-constructor probe of an invalid result
  // from a discarded branch; the invalid-arity sentinel must not be viable.
  static_assert(
      !detail::constructor_probe_msvc<same_arity_overload, constructor_shape,
                                      detail::opaque_constructor_argument,
                                      detail::list_initialization,
                                      detail::invalid_arity>::value);
}

struct initializer_list_constructor {
  initializer_list_constructor(std::initializer_list<int>) {}
};

template <typename T, typename Sequence> struct repeated;
template <typename T, size_t... Is>
struct repeated<T, std::index_sequence<Is...>> {
  using type = type_list<detail::repeated_type<T, Is>...>;
};

TEST(constructor_detection_cases, initializer_list_takes_the_largest_arity) {
  static_assert(
      detects<
          initializer_list_constructor, constructor_kind::concrete, max_arity,
          typename repeated<int, std::make_index_sequence<max_arity>>::type>());
}

// Value wrappers, nested, over leaves of every copy and move ability.
using wrappers =
    type_list<immovable *, const copy_only *, std::optional<move_only> *,
              const std::variant<leaf, immovable> *, leaf **,
              std::shared_ptr<const immovable>,
              std::unique_ptr<const copy_only>,
              std::shared_ptr<std::optional<immovable>>,
              std::unique_ptr<std::array<move_only, 2>>,
              std::shared_ptr<std::unique_ptr<copy_only>>,
              std::shared_ptr<std::variant<leaf, move_only>>,
              std::array<copy_only, 2>, std::array<std::optional<copy_only>, 2>,
              std::array<std::array<move_only, 2>, 2>,
              std::array<std::variant<leaf, move_only>, 2>>;

// Known limitation: signature detection fails to compile for a variant by
// value and for an array of immovable elements by value.
using unrecoverable_wrappers = type_list<
    std::array<immovable, 2>, std::variant<leaf, move_only>,
    std::variant<copy_only, immovable>, std::variant<leaf *, immovable>,
    std::variant<std::unique_ptr<move_only>, std::array<immovable, 2>>,
    std::variant<std::variant<leaf, move_only>,
                 std::variant<copy_only, immovable>>>;

TEST(constructor_detection_cases, value_wrappers_are_detected) {
  static_assert(detects_parameters(wrappers{}));
  static_assert(detects_parameter_shapes(unrecoverable_wrappers{}));
}

template <typename T> struct unconstrained_forwarding {
  template <typename U> unconstrained_forwarding(U &&) {}
};

template <typename T> struct forwarding {
  template <typename U,
            typename = std::enable_if_t<std::is_constructible_v<T, U &&>>>
  forwarding(U &&) {}
};

// Known limitations: no constructor is detected for a parameter by value that
// converts from anything its element can be made of: std::optional, also as a
// variant alternative, a class with an unconstrained forwarding constructor,
// or one whose forwarding constructor accepts what such a class does.
using converting_from_anything =
    type_list<std::optional<leaf>, std::optional<leaf *>,
              std::optional<std::array<copy_only, 2>>,
              std::variant<std::optional<leaf>, leaf>,
              unconstrained_forwarding<leaf>, forwarding<forwarding<leaf>>>;

template <typename... Ps> constexpr bool rejects(type_list<Ps...>) {
  return (detects<takes<Ps>, constructor_kind::invalid, 0, void>() && ...);
}

TEST(constructor_detection_cases, parameters_converting_from_anything) {
  static_assert(rejects(converting_from_anything{}));
}

template <template <typename, typename> class Detection, typename Mode>
struct detection_of {
  using mode = Mode;
  template <typename T> using type = Detection<T, Mode>;
};

template <typename F> void for_each_detection(F &&f) {
#if !defined(_MSC_VER)
  f(detection_of<portable_detection, constructor_shape>{});
  f(detection_of<portable_detection, constructor_signature>{});
#endif
  f(detection_of<msvc_detection, constructor_shape>{});
  f(detection_of<msvc_detection, constructor_signature>{});
}

struct counting_context {
  template <typename T, typename Scope> T resolve(Scope, counting_context &) {
    static_assert(std::is_same_v<T, int> || std::is_same_v<T, float>);
    if constexpr (std::is_same_v<T, int>) {
      ++integers;
      return 42;
    } else {
      ++reals;
      return 2.5f;
    }
  }

  int integers = 0;
  int reals = 0;
};

TEST(constructor_detection_cases, detected_constructor_constructs) {
  using target = fixture::constructor_two_values;
  for_each_detection([](auto detection_type) {
    using detection = typename decltype(detection_type)::template type<target>;
    counting_context context;

    auto made = detection::template construct<target>(ephemeral_scope, context,
                                                      context);
    EXPECT_EQ(made.integer, 42);
    EXPECT_EQ(made.real, 2.5f);

    alignas(target) unsigned char storage[sizeof(target)];
    detection::template construct<target>(storage, ephemeral_scope, context,
                                          context);
    auto *placed = std::launder(reinterpret_cast<target *>(storage));
    EXPECT_EQ(placed->integer, 42);
    EXPECT_EQ(placed->real, 2.5f);
    std::destroy_at(placed);

    EXPECT_EQ(context.integers, 2);
    EXPECT_EQ(context.reals, 2);
  });
}

// Records the request the detected constructor's argument resolved.
struct request_recorder {
  template <typename T> static void request() {}

  template <typename T, typename Scope> T resolve(Scope, request_recorder &) {
    requested = &request<T>;
    if constexpr (std::is_reference_v<T>) {
      static std::remove_cv_t<std::remove_reference_t<T>> value;
      return static_cast<T>(value);
    } else {
      return std::remove_cv_t<T>{};
    }
  }

  void (*requested)() = nullptr;
};

template <typename Detection, typename P, typename Request>
void expect_request() {
  request_recorder recorder;
  using target = takes<P>;
  (void)Detection::template type<target>::template construct<target>(
      ephemeral_scope, recorder, recorder);
  EXPECT_EQ(recorder.requested, &request_recorder::request<Request>);
}

// A detected constructor requests each parameter in its own category, but an
// rvalue reference as the value; a copy-only value is passed on as a prvalue.
template <typename X> void expect_requests_in_parameter_category() {
  for_each_detection([](auto detection_type) {
    using detection = decltype(detection_type);
    expect_request<detection, X, X>();
    expect_request<detection, X &, X &>();
    expect_request<detection, const X &, const X &>();
    expect_request<detection, X &&, X>();
    expect_request<detection, X *, X *>();
    expect_request<detection, const X *, const X *>();
  });
}

TEST(constructor_detection_cases, arguments_request_the_parameter_category) {
  expect_requests_in_parameter_category<std::unique_ptr<int>>();
  expect_requests_in_parameter_category<std::shared_ptr<int>>();
  for_each_detection([](auto detection_type) {
    expect_request<decltype(detection_type), copy_only, copy_only>();
  });
}

} // namespace
} // namespace dingo
