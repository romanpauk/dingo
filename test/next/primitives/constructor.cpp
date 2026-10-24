//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

// Constructor detection: the constructor a factory calls for a type, found
// from the largest arity down, or the one the type declares, and its
// invocation with an argument resolved for every parameter.

#include <dingo/primitives/constructor.h>
#include <dingo/primitives/constructor_detection.h>
#include <dingo/primitives/type_name.h>
#include <dingo/registration/constructor.h>
#include <dingo/type/annotated.h>

#include <gtest/gtest.h>

#include <array>
#include <initializer_list>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>

// Types of external linkage, as the functions a signature probe declares for
// them would otherwise be internal and never defined.
namespace unrecorded {
struct two_values {
  two_values(int, float) {}
};
struct probe {};
} // namespace unrecorded

namespace {

using dingo::core::constructor_detection;
using dingo::core::constructor_kind;
using dingo::core::declared_constructor;
using dingo::core::detected_constructor;
using dingo::core::invoke_constructor;
using dingo::core::type_list;
using dingo::core::type_name_of;

struct two_values {
  two_values(int init_integer, float init_real)
      : integer(init_integer), real(init_real) {}
  int integer;
  float real;
};

struct two_or_three_values {
  two_or_three_values(int, float) {}
  two_or_three_values(int, float, double) {}
};

struct defaulted {};

struct empty_base {};
struct derived_aggregate : empty_base {};

// A default constructor, so the copy constructor is the only one of arity one.
struct copyable_only {
  copyable_only() {}
};

struct annotated_aggregate {
  using dingo_constructor_type = dingo::constructor<annotated_aggregate(int)>;
};

struct typedef_selected {
  typedef_selected(int, int, int) {}
  DINGO_CONSTRUCTOR(typedef_selected(double, const char *)) {}
};

TEST(next_constructor, detects_the_constructor_of_the_largest_arity) {
  static_assert(detected_constructor<two_values>::arity == 2);
  static_assert(detected_constructor<two_values>::kind ==
                constructor_kind::concrete);
  static_assert(detected_constructor<two_or_three_values>::arity == 3);
  // The search starts at its limit, so a limit below the largest arity finds
  // the largest constructor within it.
  static_assert(dingo::core::detail::find_arity<two_or_three_values, 2>() == 2);
  static_assert(dingo::core::detail::find_arity<two_values, 1>() ==
                dingo::core::detail::invalid_arity);
  static_assert(detected_constructor<defaulted>::arity == 0);
  // An aggregate with a base takes the base as its first initializer.
  static_assert(detected_constructor<derived_aggregate>::arity == 1);
  // The copy constructor is never detected.
  static_assert(detected_constructor<copyable_only>::arity == 0);
}

struct generic {
  template <typename... Args> generic(Args &&...) {}
};

struct one_of_two_types {
  explicit one_of_two_types(int) {}
  explicit one_of_two_types(float) {}
};

struct two_orders {
  two_orders(int, float) {}
  two_orders(float, int) {}
};

struct explicit_value {
  explicit explicit_value(int) {}
};

struct initializer_list_values {
  initializer_list_values(std::initializer_list<int>) {}
};

TEST(next_constructor, a_constructor_template_is_generic) {
  static_assert(detected_constructor<generic>::kind ==
                constructor_kind::generic);
}

TEST(next_constructor, constructors_of_the_same_arity_are_not_detected) {
  static_assert(detected_constructor<one_of_two_types>::kind ==
                constructor_kind::invalid);
  static_assert(detected_constructor<one_of_two_types>::arity == 0);
  static_assert(detected_constructor<two_orders>::kind ==
                constructor_kind::invalid);
  static_assert(detected_constructor<two_orders>::arity == 0);
}

TEST(next_constructor, detects_an_explicit_constructor) {
  static_assert(detected_constructor<explicit_value>::kind ==
                constructor_kind::concrete);
  static_assert(detected_constructor<explicit_value>::arity == 1);
}

TEST(next_constructor,
     an_initializer_list_constructor_takes_the_largest_arity_searched) {
  static_assert(detected_constructor<initializer_list_values>::kind ==
                constructor_kind::concrete);
  static_assert(detected_constructor<initializer_list_values>::arity ==
                DINGO_CONSTRUCTOR_DETECTION_ARGS);
}

template <typename Type, typename Backend> constexpr bool detects_as_default() {
  using detection =
      constructor_detection<Type, dingo::core::constructor_arity, Backend>;
  return detection::kind == detected_constructor<Type>::kind &&
         detection::arity == detected_constructor<Type>::arity;
}

// The detection that the engine uses is the type tree detection of the backend
// that variant_points.constructor_detection_msvc selects, and both backends
// agree on it. The matrix family constructor_detection crosses the backends and
// modes with the constructor type trees.
TEST(next_constructor, the_engine_detects_by_the_selected_backend) {
  using dingo::core::msvc_constructor_detection;
  using dingo::core::portable_constructor_detection;
  static_assert(
      std::is_same_v<
          dingo::core::default_constructor_detection,
          std::conditional_t<dingo::core::msvc_constructor_detection_variant,
                             msvc_constructor_detection,
                             portable_constructor_detection>>);
  static_assert(detects_as_default<two_values, msvc_constructor_detection>());
  static_assert(detects_as_default<generic, msvc_constructor_detection>());
  static_assert(
      detects_as_default<one_of_two_types, msvc_constructor_detection>());
  static_assert(detects_as_default<int, msvc_constructor_detection>());
  static_assert(
      detects_as_default<two_values, portable_constructor_detection>());
  static_assert(detects_as_default<generic, portable_constructor_detection>());
  static_assert(
      detects_as_default<one_of_two_types, portable_constructor_detection>());
  static_assert(detects_as_default<int, portable_constructor_detection>());
}

// The signature mode recovers the argument types of the detected constructor;
// the type tree mode reports them only for a constructor without arguments.
TEST(next_constructor, the_signature_mode_recovers_the_argument_types) {
  using dingo::core::constructor_arity;
  using dingo::core::constructor_signature;
  static_assert(
      std::is_same_v<
          constructor_detection<two_values, constructor_signature>::arguments,
          type_list<int, float>>);
  static_assert(std::is_same_v<
                constructor_detection<two_values, constructor_arity>::arguments,
                void>);
  static_assert(std::is_same_v<
                constructor_detection<defaulted, constructor_arity>::arguments,
                type_list<>>);
  static_assert(
      std::is_same_v<
          constructor_detection<generic, constructor_signature>::arguments,
          void>);
}

#if !defined(_MSC_VER)
// An argument whose category no probe recorded is not recovered, so neither is
// the signature.
TEST(next_constructor, an_unrecorded_argument_category_is_not_recovered) {
  static_assert(
      std::is_void_v<typename dingo::core::detail::signature_arguments<
          unrecorded::two_values, unrecorded::probe,
          std::make_index_sequence<2>>::type>);
}
#endif

TEST(next_constructor, value_initializes_a_scalar) {
  static_assert(detected_constructor<int>::arity == 0);
  static_assert(detected_constructor<int>::kind == constructor_kind::concrete);
  static_assert(detected_constructor<const int>::arity == 0);
  static_assert(detected_constructor<volatile int>::arity == 0);
  static_assert(detected_constructor<int *>::arity == 0);
}

TEST(next_constructor, takes_the_declared_constructor_arguments) {
  static_assert(!declared_constructor<two_values>::declared);
  static_assert(declared_constructor<typedef_selected>::declared);
  static_assert(
      std::is_same_v<declared_constructor<typedef_selected>::arguments,
                     type_list<double, const char *>>);
  static_assert(declared_constructor<annotated_aggregate>::declared);
  static_assert(
      std::is_same_v<declared_constructor<annotated_aggregate>::arguments,
                     type_list<int>>);
}

// Neither copyable, so an element is constructed only from the prvalue its
// argument resolves.
struct move_only {
  explicit move_only(int init) : value(init) {}
  move_only(const move_only &) = delete;
  move_only(move_only &&) = default;
  int value;
};

// Resolves every argument by its type: a handle of the next number, a
// move-only value, or an array, which it constructs by its detected
// constructor in turn.
struct counting_resolver {
  template <typename Type> Type argument() {
    if constexpr (std::is_same_v<Type, std::unique_ptr<int>>) {
      return std::make_unique<int>(++numbers);
    } else if constexpr (std::is_same_v<Type, move_only>) {
      ++resolutions;
      return move_only{7};
    } else {
      return invoke_constructor<Type>(*this, [](auto &&...args) {
        return Type{std::forward<decltype(args)>(args)...};
      });
    }
  }

  // The arrays of the tests take no reference.
  template <typename Request> Request resolve() {
    throw std::logic_error("a reference is not resolved");
  }

  int numbers = 0;
  int resolutions = 0;
};

template <typename Type> Type construct(counting_resolver &resolver) {
  return resolver.argument<Type>();
}

TEST(next_constructor, constructs_an_array_of_move_only_handles_in_order) {
  using array_type = std::array<std::unique_ptr<int>, 2>;
  static_assert(detected_constructor<array_type>::arity == 2);
  counting_resolver resolver;
  array_type values = construct<array_type>(resolver);
  ASSERT_NE(values[0], nullptr);
  ASSERT_NE(values[1], nullptr);
  EXPECT_EQ(*values[0], 1);
  EXPECT_EQ(*values[1], 2);
}

TEST(next_constructor, constructs_a_nested_array_of_move_only_handles) {
  using inner_array_type = std::array<std::unique_ptr<int>, 2>;
  using array_type = std::array<inner_array_type, 2>;
  static_assert(detected_constructor<array_type>::arity == 2);
  counting_resolver resolver;
  array_type values = construct<array_type>(resolver);
  EXPECT_EQ(*values[0][0], 1);
  EXPECT_EQ(*values[0][1], 2);
  EXPECT_EQ(*values[1][0], 3);
  EXPECT_EQ(*values[1][1], 4);
}

TEST(next_constructor, resolves_every_move_only_element_of_an_array) {
  using array_type = std::array<move_only, 2>;
  counting_resolver resolver;
  array_type values = construct<array_type>(resolver);
  EXPECT_EQ(values[0].value, 7);
  EXPECT_EQ(values[1].value, 7);
  EXPECT_EQ(resolver.resolutions, 2);
}

TEST(next_constructor, resolves_every_move_only_element_of_a_nested_array) {
  using array_type = std::array<std::array<move_only, 2>, 2>;
  counting_resolver resolver;
  array_type values = construct<array_type>(resolver);
  EXPECT_EQ(values[0][0].value, 7);
  EXPECT_EQ(values[0][1].value, 7);
  EXPECT_EQ(values[1][0].value, 7);
  EXPECT_EQ(values[1][1].value, 7);
  EXPECT_EQ(resolver.resolutions, 4);
}

// A type whose only constructor takes one argument of type `Parameter`.
template <typename Parameter> struct consumer {
  explicit consumer(Parameter) {}
};

struct regular {};

// A copy-only and a move-only type: the argument of either is constructed
// from the prvalue that the resolver returns.
struct copy_only {
  copy_only() = default;
  copy_only(const copy_only &) = default;
  copy_only(copy_only &&) = delete;
};

struct move_only_value {
  move_only_value() = default;
  move_only_value(const move_only_value &) = delete;
  move_only_value(move_only_value &&) = default;
};

struct tag {};

// Records how the argument of a constructor is requested: a value through
// `argument`, or a reference through `resolve`.
struct recording_resolver {
  template <typename Type> Type argument() {
    request = "argument " + type_name_of<Type>();
    if constexpr (std::is_default_constructible_v<Type>) {
      return Type{};
    } else {
      // An annotated value or reference is made from what it annotates.
      return Type(stored<std::remove_reference_t<
                      typename dingo::annotated_traits<Type>::type>>());
    }
  }

  template <typename Request> Request resolve() {
    request = "resolve " + type_name_of<Request>();
    return stored<std::remove_cv_t<std::remove_reference_t<Request>>>();
  }

  template <typename Type> static Type &stored() {
    static Type object{};
    return object;
  }

  std::string request;
};

// The request for the argument of the detected constructor of
// `consumer<Parameter>`.
template <typename Parameter> std::string constructor_request() {
  using type = consumer<Parameter>;
  static_assert(detected_constructor<type>::kind == constructor_kind::concrete);
  static_assert(detected_constructor<type>::arity == 1);
  recording_resolver resolver;
  (void)invoke_constructor<type>(resolver, [](auto &&...args) {
    return type{std::forward<decltype(args)>(args)...};
  });
  return resolver.request;
}

template <typename Type> std::string argument_request() {
  return "argument " + type_name_of<Type>();
}

template <typename Request> std::string reference_request() {
  return "resolve " + type_name_of<Request>();
}

// A value and an rvalue reference take a value, a pointer takes a value of
// its pointer type, and an lvalue reference is resolved as a reference.
template <typename Handle> void expect_category_requests() {
  EXPECT_EQ(constructor_request<Handle>(), argument_request<Handle>());
  EXPECT_EQ(constructor_request<Handle &>(), reference_request<Handle &>());
  EXPECT_EQ(constructor_request<const Handle &>(),
            reference_request<const Handle &>());
  EXPECT_EQ(constructor_request<Handle &&>(), argument_request<Handle>());
  EXPECT_EQ(constructor_request<Handle *>(), argument_request<Handle *>());
  EXPECT_EQ(constructor_request<const Handle *>(),
            argument_request<const Handle *>());
}

TEST(next_constructor, requests_an_argument_by_the_category_of_its_parameter) {
  expect_category_requests<std::unique_ptr<int>>();
  expect_category_requests<std::shared_ptr<int>>();
  expect_category_requests<regular>();
}

// A wrapper is requested whole, not as the value it converts from.
TEST(next_constructor, requests_a_wrapper_parameter_as_a_whole) {
  using regular_array = regular[2];
  using regular_or_copy_only = std::variant<regular, copy_only>;
  using shared_unique =
      std::variant<std::shared_ptr<regular>, std::unique_ptr<regular>>;
  using move_only_array = std::array<move_only_value, 2>;
  EXPECT_EQ(constructor_request<std::shared_ptr<regular>>(),
            argument_request<std::shared_ptr<regular>>());
  EXPECT_EQ(constructor_request<std::shared_ptr<regular> &>(),
            reference_request<std::shared_ptr<regular> &>());
  EXPECT_EQ(constructor_request<std::shared_ptr<const regular>>(),
            argument_request<std::shared_ptr<const regular>>());
  EXPECT_EQ(constructor_request<std::unique_ptr<const regular>>(),
            argument_request<std::unique_ptr<const regular>>());
  EXPECT_EQ(constructor_request<std::optional<regular> &>(),
            reference_request<std::optional<regular> &>());
  EXPECT_EQ(constructor_request<regular_array &>(),
            reference_request<regular_array &>());
  EXPECT_EQ(constructor_request<regular_or_copy_only>(),
            argument_request<regular_or_copy_only>());
  EXPECT_EQ(constructor_request<shared_unique>(),
            argument_request<shared_unique>());
  EXPECT_EQ(constructor_request<std::shared_ptr<regular_array>>(),
            argument_request<std::shared_ptr<regular_array>>());
  EXPECT_EQ(constructor_request<move_only_array>(),
            argument_request<move_only_array>());
}

TEST(next_constructor, requests_a_copy_only_or_move_only_value) {
  EXPECT_EQ(constructor_request<copy_only>(), argument_request<copy_only>());
  EXPECT_EQ(constructor_request<move_only_value>(),
            argument_request<move_only_value>());
}

TEST(next_constructor, requests_an_annotated_parameter_as_a_whole) {
  using annotated_reference = dingo::annotated<regular &, tag>;
  using annotated_pointer = dingo::annotated<regular *, tag>;
  using annotated_shared = dingo::annotated<std::shared_ptr<regular>, tag>;
  using annotated_shared_reference =
      dingo::annotated<std::shared_ptr<regular> &, tag>;
  EXPECT_EQ(constructor_request<annotated_reference>(),
            argument_request<annotated_reference>());
  EXPECT_EQ(constructor_request<annotated_pointer>(),
            argument_request<annotated_pointer>());
  EXPECT_EQ(constructor_request<annotated_shared>(),
            argument_request<annotated_shared>());
  EXPECT_EQ(constructor_request<annotated_shared_reference>(),
            argument_request<annotated_shared_reference>());
}

} // namespace
