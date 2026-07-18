//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#include <dingo/container.h>
#include <dingo/factory/detail/constructor_detection_msvc.hpp>
#include <dingo/registration/constructor.h>
#include <dingo/storage/external.h>
#include <dingo/storage/shared.h>
#include <dingo/storage/shared_cyclical.h>
#include <dingo/storage/unique.h>

#include <gtest/gtest.h>

#include <initializer_list>

#include "support/assert.h"
#include "support/class.h"
#include "support/containers.h"

namespace dingo {
namespace detail {
struct unique_class {
  unique_class(const unique_class &) = delete;
  unique_class(unique_class &&) {}
  unique_class(std::nullptr_t) {}
  unique_class(void *) {}
};

struct shared_class {
  shared_class(const shared_class &) {}
  shared_class(shared_class &&) {}
  shared_class(std::nullptr_t) {}
  shared_class(void *) {}
};

} // namespace detail

using unique_class = std::unique_ptr<int>; // detail::unique_class;
using shared_class = std::shared_ptr<int>; // detail::shared_class;

struct trait_limited_detected {
  trait_limited_detected(int, float) {}
  trait_limited_detected(int, float, double) {}
};

template <> struct constructor_detection_traits<trait_limited_detected> {
  static constexpr size_t max_arity = 2;
};

struct typed_config {};
struct typed_copy_only_config {
  typed_copy_only_config() = default;
  typed_copy_only_config(const typed_copy_only_config &) = default;
  typed_copy_only_config(typed_copy_only_config &&) = delete;
};
struct typed_move_only_config {
  typed_move_only_config() = default;
  typed_move_only_config(const typed_move_only_config &) = delete;
  typed_move_only_config(typed_move_only_config &&) = default;
};
struct testing_struct {
  testing_struct(int integer_arg, float real_arg)
      : integer(integer_arg), real(real_arg) {}

  int integer;
  float real;
};
struct typed_lvalue_reference {
  typed_lvalue_reference(typed_config &) {}
};
struct typed_const_lvalue_reference {
  typed_const_lvalue_reference(const typed_config &) {}
};
struct typed_rvalue_reference {
  typed_rvalue_reference(typed_config &&) {}
};
struct typed_value {
  typed_value(typed_config) {}
};
struct typed_copy_only_value {
  typed_copy_only_value(typed_copy_only_config) {}
};
struct typed_move_only_value {
  typed_move_only_value(typed_move_only_config) {}
};
struct typed_selector {};
struct typed_selected {
  typed_selected(
      detail::selected<typed_config, detail::type_selector<typed_selector>>) {}
};
struct typed_annotation {};
struct typed_annotated {
  typed_annotated(annotated<typed_config &, typed_annotation>) {}
};
struct typed_incomplete_config;
struct typed_incomplete_reference {
  typed_incomplete_reference(typed_incomplete_config &) {}
};
struct typed_selected_incomplete_reference {
  typed_selected_incomplete_reference(
      detail::selected<typed_incomplete_config &,
                       detail::type_selector<typed_selector>>) {}
};
struct typed_aggregate {
  int number;
  typed_config &config;
};
struct typed_defaulted {};
struct typed_selected_by_typedef {
  DINGO_CONSTRUCTOR(typed_selected_by_typedef(double, const char *)) {}
};
struct typedef_selected_dependency {
  DINGO_CONSTRUCTOR(typedef_selected_dependency()) : value(1) {}
  explicit typedef_selected_dependency(int) : value(2) {}

  int value;
};
template <>
struct is_auto_constructible<typedef_selected_dependency> : std::true_type {};

struct typedef_selected_consumer {
  explicit typedef_selected_consumer(typedef_selected_dependency dependency)
      : value(dependency.value) {}

  int value;
};
struct typed_generic {
  template <typename... Args> typed_generic(Args &&...) {}
};
struct typed_ambiguous {
  typed_ambiguous(int) {}
  typed_ambiguous(float) {}
};
struct typed_explicit {
  explicit typed_explicit(int) {}
};
struct typed_initializer_list {
  typed_initializer_list(std::initializer_list<int>) {}
};
struct typed_same_arity_overload {
  typed_same_arity_overload(int, float) {}
  typed_same_arity_overload(float, int) {}
};

struct category_typed_test {};
struct unresolved_typed_test {};

template <typename T, size_t Arity>
using constructor_signature_arguments_by_category_t =
    typename detail::constructor_signature_recovery_msvc_impl<
        T, category_typed_test, detail::list_initialization, Arity,
        std::make_index_sequence<Arity>>::type;

template <typename T, typename Sequence> struct repeated_argument_list;

template <typename T, size_t... Is>
struct repeated_argument_list<T, std::index_sequence<Is...>> {
  using type = type_list<detail::repeated_type<T, Is>...>;
};

template <typename T, size_t Arity>
using repeated_argument_list_t =
    typename repeated_argument_list<T, std::make_index_sequence<Arity>>::type;

struct typed_construction_container {};

struct typed_construction_context {
  template <typename T>
  T resolve(construction_scope, typed_construction_container &) {
    static_assert(std::is_same_v<T, int> || std::is_same_v<T, float>);
    if constexpr (std::is_same_v<T, int>) {
      ++integer_resolutions;
      return 42;
    } else {
      ++float_resolutions;
      return 2.5f;
    }
  }

  int integer_resolutions = 0;
  int float_resolutions = 0;
};

struct copy_only_construction_context {
  template <typename T>
  T resolve(construction_scope, typed_construction_container &) {
    static_assert(std::is_same_v<T, typed_copy_only_config>);
    return {};
  }
};

template <typename T>
inline constexpr bool signature_matches_shape_construction_v =
    constructor_detection<T, detail::constructor_shape>::kind !=
        detail::constructor_kind::concrete ||
    (constructor_detection<T, detail::constructor_signature>::kind ==
         constructor_detection<T, detail::constructor_shape>::kind &&
     constructor_detection<T, detail::constructor_signature>::arity ==
         constructor_detection<T, detail::constructor_shape>::arity &&
     !std::is_void_v<typename constructor_detection<
         T, detail::constructor_signature>::arguments>);

template <template <typename, typename, template <typename...> typename,
                    size_t> typename Detection>
inline constexpr bool constructor_arity_cases_v =
    Detection<testing_struct, detail::constructor_shape,
              detail::list_initialization, 2>::kind ==
        detail::constructor_kind::concrete &&
    Detection<testing_struct, detail::constructor_shape,
              detail::list_initialization, 2>::arity == 2 &&
    Detection<typed_defaulted, detail::constructor_shape,
              detail::list_initialization, 2>::kind ==
        detail::constructor_kind::concrete &&
    Detection<typed_defaulted, detail::constructor_shape,
              detail::list_initialization, 2>::arity == 0 &&
    Detection<typed_same_arity_overload, detail::constructor_shape,
              detail::list_initialization, 2>::kind ==
        detail::constructor_kind::invalid;

template <typename U> struct arg_rvalue_reference {
  template <typename T,
            typename = std::enable_if_t<!std::is_same_v<std::decay_t<T>, U>>>
  operator T &&() const {
    throw std::runtime_error("operator T&&()");
  }
};

template <typename U> struct arg_const_lvalue_reference {
  template <typename T,
            typename = std::enable_if_t<!std::is_same_v<std::decay_t<T>, U>>>
  operator const T &() const {
    throw std::runtime_error("operator const T&()");
  }
};

template <typename U> struct arg_lvalue_reference {
  template <typename T,
            typename = std::enable_if_t<!std::is_same_v<std::decay_t<T>, U>>>
  operator T &() const {
    throw std::runtime_error("operator T&()");
  }
};

template <typename U> struct arg_value {
  template <typename T,
            typename = std::enable_if_t<!std::is_same_v<std::decay_t<T>, U>>>
  operator T() {
    throw std::runtime_error("operator T()");
  }
};

template <typename U> struct arg_pointer {
  template <typename T,
            typename = std::enable_if_t<!std::is_same_v<std::decay_t<T>, U>>>
  operator T *() {
    throw std::runtime_error("operator T*()");
  }
};

template <typename U>
struct arg : arg_value<U>,
             arg_const_lvalue_reference<U>,
             arg_lvalue_reference<U>,
             arg_pointer<U>

// TODO: for some compilers, a presence of rvalue reference is needed
// so the code is compiled without ambiguity. That does not mean the operator
// T&&() will actually be called.
#if (defined(_MSC_VER) && DINGO_CXX_STANDARD == 17) || __GNUC__ == 12 ||       \
    __GNUC__ == 13
    ,
             arg_rvalue_reference<U>
#endif
{
};

#if !defined(_MSC_VER) || DINGO_CXX_VERSION > 17
TEST(constructor_detection_test, unique_arg_value) {
  struct T {
    T(unique_class) {}
  };

  try {
    T instance((arg<T>()));
  } catch (const std::runtime_error &e) {
    ASSERT_STREQ(e.what(), "operator T()");
  }
}
#endif

TEST(constructor_detection_test, unique_arg_lvalue_reference) {
  struct T {
    T(unique_class &) {}
  };

  try {
    T instance((arg<T>()));
  } catch (const std::runtime_error &e) {
    ASSERT_STREQ(e.what(), "operator T&()");
  }
}

TEST(constructor_detection_test, unique_arg_const_lvalue_reference) {
  struct T {
    T(const unique_class &) {}
  };

  try {
    T instance((arg<T>()));
  } catch (const std::runtime_error &e) {
    ASSERT_STREQ(e.what(), "operator const T&()");
  }
}

TEST(constructor_detection_test, unique_arg_rvalue_reference) {
  struct T {
    T(unique_class &&) {}
  };

  try {
    T instance((arg<T>()));
  } catch (const std::runtime_error &e) {
#if __GNUC__ == 12 || __GNUC__ == 13
    ASSERT_STREQ(e.what(), "operator T&&()");
#else
    ASSERT_STREQ(e.what(), "operator T()");
#endif
  }
}

TEST(constructor_detection_test, unique_arg_pointer) {
  struct T {
    T(unique_class *) {}
  };

  try {
    T instance((arg<T>()));
  } catch (const std::runtime_error &e) {
    ASSERT_STREQ(e.what(), "operator T*()");
  }
}

#if !defined(_MSC_VER) || DINGO_CXX_VERSION > 17
TEST(constructor_detection_test, shared_arg_value) {
  struct T {
    T(shared_class) {}
  };

  try {
    T instance((arg<T>()));
  } catch (const std::runtime_error &e) {
    ASSERT_STREQ(e.what(), "operator T()");
  }
}
#endif

TEST(constructor_detection_test, shared_arg_lvalue_reference) {
  struct T {
    T(shared_class &) {}
  };

  try {
    T instance((arg<T>()));
  } catch (const std::runtime_error &e) {
    ASSERT_STREQ(e.what(), "operator T&()");
  }
}

TEST(constructor_detection_test, shared_arg_const_lvalue_reference) {
  struct T {
    T(const shared_class &) {}
  };

  try {
    T instance((arg<T>()));
  } catch (const std::runtime_error &e) {
    ASSERT_STREQ(e.what(), "operator const T&()");
  }
}

TEST(constructor_detection_test, shared_arg_rvalue_reference) {
  struct T {
    T(shared_class &&) {}
  };

  try {
    T instance((arg<T>()));
  } catch (const std::runtime_error &e) {
#if __GNUC__ == 12 || __GNUC__ == 13
    ASSERT_STREQ(e.what(), "operator T&&()");
#else
    ASSERT_STREQ(e.what(), "operator T()");
#endif
  }
}

TEST(constructor_detection_test, constructor_arguments_type_list) {
  struct Explicit {
    Explicit(int, float) {}
  };
  struct Detected {
    Detected(int, float) {}
  };
  struct SelectedByTypedef {
    DINGO_CONSTRUCTOR(SelectedByTypedef(int, float)) {}
  };
  struct Defaulted {};

  static_assert(
      std::is_same_v<typename constructor<Explicit(int, float)>::arguments,
                     type_list<int, float>>);
  static_assert(constructor_detection<Detected>::arity == 2);
  static_assert(constructor_detection<Detected>::kind ==
                detail::constructor_kind::concrete);
  static_assert(
      std::is_same_v<typename constructor_detection<
                         Detected, detail::constructor_signature>::arguments,
                     type_list<int, float>>);
  static_assert(std::is_same_v<
                typename constructor_detection<SelectedByTypedef>::arguments,
                type_list<int, float>>);
  static_assert(
      std::is_same_v<typename constructor_detection<Defaulted>::arguments,
                     type_list<>>);
}

TEST(constructor_detection_test,
     constructor_detection_traits_limit_public_search) {
  static_assert(constructor_detection<trait_limited_detected>::arity == 2);
  static_assert(constructor_detection<trait_limited_detected>::kind ==
                detail::constructor_kind::concrete);
#if defined(_MSC_VER)
  static_assert(detail::constructor_detection_msvc<
                    trait_limited_detected, detail::constructor_shape,
                    detail::list_initialization, 3>::arity == 3);
#else
  static_assert(detail::constructor_detection<
                    trait_limited_detected, detail::constructor_shape,
                    detail::list_initialization, 3>::arity == 3);
#endif
}

TEST(constructor_detection_test, constructor_arity_implementations) {
#if !defined(_MSC_VER)
  static_assert(constructor_arity_cases_v<detail::constructor_detection>);
#endif
  static_assert(constructor_arity_cases_v<detail::constructor_detection_msvc>);
  static_assert(!detail::constructor_probe_msvc<
                typed_same_arity_overload, detail::constructor_shape,
                detail::opaque_constructor_argument,
                detail::list_initialization, detail::invalid_arity>::value);
}

TEST(constructor_detection_test, defaults_to_constructor_shape) {
  static_assert(
      std::is_same_v<
          constructor_detection<testing_struct>,
          constructor_detection<testing_struct, detail::constructor_shape>>);
}

TEST(constructor_detection_test,
     auto_construction_uses_the_public_constructor_selection) {
  container<> container;
  EXPECT_EQ(container.construct<typedef_selected_consumer>().value, 1);
}

TEST(constructor_detection_test, constructor_signature_type_list) {
#if !defined(_MSC_VER)
  using unresolved_arguments = typename detail::constructor_signature_arguments<
      testing_struct, unresolved_typed_test, std::make_index_sequence<2>>::type;
  static_assert(std::is_void_v<unresolved_arguments>);
#endif

  static_assert(
      constructor_detection<testing_struct, detail::constructor_shape>::arity ==
      2);
  static_assert(std::is_void_v<typename constructor_detection<
                    testing_struct, detail::constructor_shape>::arguments>);
  static_assert(constructor_detection<testing_struct,
                                      detail::constructor_signature>::arity ==
                2);
  static_assert(constructor_detection<testing_struct,
                                      detail::constructor_signature>::kind ==
                detail::constructor_kind::concrete);
  static_assert(std::is_same_v<
                typename constructor_detection<
                    testing_struct, detail::constructor_signature>::arguments,
                type_list<int, float>>);
  static_assert(
      std::is_same_v<
          typename constructor_detection<
              typed_lvalue_reference, detail::constructor_signature>::arguments,
          type_list<typed_config &>>);
  static_assert(std::is_same_v<typename constructor_detection<
                                   typed_const_lvalue_reference,
                                   detail::constructor_signature>::arguments,
                               type_list<const typed_config &>>);
  static_assert(
      std::is_same_v<
          typename constructor_detection<
              typed_rvalue_reference, detail::constructor_signature>::arguments,
          type_list<typed_config>>);
  static_assert(
      std::is_same_v<
          typename constructor_detection<
              typed_copy_only_value, detail::constructor_signature>::arguments,
          type_list<typed_copy_only_config>>);
  static_assert(
      std::is_same_v<
          typename constructor_detection<
              typed_move_only_value, detail::constructor_signature>::arguments,
          type_list<typed_move_only_config>>);
  static_assert(
      std::is_same_v<typename constructor_detection<
                         typed_value, detail::constructor_signature>::arguments,
                     type_list<typed_config>>);
  static_assert(
      std::is_same_v<
          typename constructor_detection<
              typed_selected, detail::constructor_signature>::arguments,
          type_list<detail::selected<typed_config,
                                     detail::type_selector<typed_selector>>>>);
  static_assert(std::is_same_v<
                typename constructor_detection<
                    typed_annotated, detail::constructor_signature>::arguments,
                type_list<annotated<typed_config &, typed_annotation>>>);
  static_assert(std::is_same_v<typename constructor_detection<
                                   typed_incomplete_reference,
                                   detail::constructor_signature>::arguments,
                               type_list<typed_incomplete_config &>>);
  static_assert(
      std::is_same_v<
          typename constructor_detection<
              typed_selected_incomplete_reference,
              detail::constructor_signature>::arguments,
          type_list<detail::selected<typed_incomplete_config &,
                                     detail::type_selector<typed_selector>>>>);
  static_assert(std::is_same_v<
                typename constructor_detection<
                    typed_aggregate, detail::constructor_signature>::arguments,
                type_list<int, typed_config &>>);

#if !defined(_MSC_VER)
  // Exercise the category-keyed MSVC algorithm locally without replacing
  // the combined probe used by this compiler.
  static_assert(
      std::is_same_v<
          constructor_signature_arguments_by_category_t<testing_struct, 2>,
          type_list<int, float>>);
  static_assert(std::is_same_v<constructor_signature_arguments_by_category_t<
                                   typed_lvalue_reference, 1>,
                               type_list<typed_config &>>);
  static_assert(std::is_same_v<constructor_signature_arguments_by_category_t<
                                   typed_const_lvalue_reference, 1>,
                               type_list<const typed_config &>>);
  static_assert(std::is_same_v<constructor_signature_arguments_by_category_t<
                                   typed_rvalue_reference, 1>,
                               type_list<typed_config>>);
  static_assert(std::is_same_v<
                constructor_signature_arguments_by_category_t<typed_value, 1>,
                type_list<typed_config>>);
  static_assert(std::is_same_v<constructor_signature_arguments_by_category_t<
                                   typed_copy_only_value, 1>,
                               type_list<typed_copy_only_config>>);
  static_assert(std::is_same_v<constructor_signature_arguments_by_category_t<
                                   typed_move_only_value, 1>,
                               type_list<typed_move_only_config>>);
  static_assert(
      std::is_same_v<
          constructor_signature_arguments_by_category_t<typed_selected, 1>,
          type_list<detail::selected<typed_config,
                                     detail::type_selector<typed_selector>>>>);
  static_assert(
      std::is_same_v<
          constructor_signature_arguments_by_category_t<typed_annotated, 1>,
          type_list<annotated<typed_config &, typed_annotation>>>);
  static_assert(std::is_same_v<constructor_signature_arguments_by_category_t<
                                   typed_incomplete_reference, 1>,
                               type_list<typed_incomplete_config &>>);
  static_assert(
      std::is_same_v<
          constructor_signature_arguments_by_category_t<
              typed_selected_incomplete_reference, 1>,
          type_list<detail::selected<typed_incomplete_config &,
                                     detail::type_selector<typed_selector>>>>);
  static_assert(
      std::is_same_v<
          constructor_signature_arguments_by_category_t<typed_aggregate, 2>,
          type_list<int, typed_config &>>);
#endif

  static_assert(std::is_same_v<
                typename constructor_detection<
                    typed_defaulted, detail::constructor_signature>::arguments,
                type_list<>>);
  static_assert(std::is_same_v<typename constructor_detection<
                                   typed_selected_by_typedef,
                                   detail::constructor_signature>::arguments,
                               type_list<double, const char *>>);
  static_assert(constructor_detection<typed_generic,
                                      detail::constructor_signature>::kind ==
                detail::constructor_kind::generic);
  static_assert(std::is_same_v<
                typename constructor_detection<
                    typed_generic, detail::constructor_signature>::arguments,
                void>);
  static_assert(std::is_same_v<
                typename constructor_detection<
                    typed_ambiguous, detail::constructor_signature>::arguments,
                void>);
  static_assert(std::is_same_v<
                typename constructor_detection<
                    typed_explicit, detail::constructor_signature>::arguments,
                type_list<int>>);
  static_assert(std::is_same_v<
                typename constructor_detection<
                    testing_struct, detail::constructor_signature>::arguments,
                typename constructor_detection<
                    testing_struct, detail::constructor_signature>::arguments>);
  static_assert(constructor_detection<typed_initializer_list>::kind ==
                detail::constructor_kind::concrete);
  static_assert(constructor_detection<typed_initializer_list>::arity ==
                DINGO_CONSTRUCTOR_DETECTION_ARGS);
  static_assert(
      std::is_same_v<
          typename constructor_detection<
              typed_initializer_list, detail::constructor_signature>::arguments,
          repeated_argument_list_t<int, DINGO_CONSTRUCTOR_DETECTION_ARGS>>);
  static_assert(constructor_detection<typed_same_arity_overload>::kind ==
                detail::constructor_kind::invalid);
  static_assert(std::is_same_v<typename constructor_detection<
                                   typed_same_arity_overload,
                                   detail::constructor_signature>::arguments,
                               void>);

  static_assert(signature_matches_shape_construction_v<testing_struct>);
  static_assert(signature_matches_shape_construction_v<typed_lvalue_reference>);
  static_assert(
      signature_matches_shape_construction_v<typed_const_lvalue_reference>);
  static_assert(signature_matches_shape_construction_v<typed_rvalue_reference>);
  static_assert(signature_matches_shape_construction_v<typed_value>);
  static_assert(signature_matches_shape_construction_v<typed_copy_only_value>);
  static_assert(signature_matches_shape_construction_v<typed_move_only_value>);
  static_assert(signature_matches_shape_construction_v<typed_selected>);
  static_assert(signature_matches_shape_construction_v<typed_annotated>);
  static_assert(
      signature_matches_shape_construction_v<typed_incomplete_reference>);
  static_assert(signature_matches_shape_construction_v<typed_aggregate>);
  static_assert(signature_matches_shape_construction_v<typed_defaulted>);
  static_assert(signature_matches_shape_construction_v<typed_initializer_list>);
}

TEST(constructor_detection_test,
     constructor_signature_constructs_with_detected_argument_types) {
  using detection =
      constructor_detection<testing_struct, detail::constructor_signature>;
  typed_construction_context context;
  typed_construction_container container;

  auto instance =
      detection::construct<testing_struct>(ephemeral_scope, context, container);
  EXPECT_EQ(instance.integer, 42);
  EXPECT_FLOAT_EQ(instance.real, 2.5f);

  alignas(testing_struct) unsigned char storage[sizeof(testing_struct)];
  detection::construct<testing_struct>(storage, ephemeral_scope, context,
                                       container);
  auto *placed = reinterpret_cast<testing_struct *>(storage);
  EXPECT_EQ(placed->integer, 42);
  EXPECT_FLOAT_EQ(placed->real, 2.5f);
  placed->~testing_struct();

  EXPECT_EQ(context.integer_resolutions, 2);
  EXPECT_EQ(context.float_resolutions, 2);

  copy_only_construction_context copy_only_context;
  (void)constructor_detection<typed_copy_only_value,
                              detail::constructor_signature>::
      construct<typed_copy_only_value>(ephemeral_scope, copy_only_context,
                                       container);
}

} // namespace dingo
