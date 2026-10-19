//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>

namespace dingo::matrix {

class assertion_error : public std::runtime_error {
public:
  explicit assertion_error(const std::string &message)
      : std::runtime_error(message) {}
};

[[noreturn]] inline void assertion_failed(const std::string &expression,
                                          const char *file, int line) {
  throw assertion_error(std::string(file) + ":" + std::to_string(line) +
                        ": assertion failed: " + expression);
}

#if defined(DINGO_MATRIX_RECORD)
// Recording hooks for the differential harness. They are defined in
// test/differential/recorder.cpp, which is linked only into matrix test
// executables configured with DINGO_MATRIX_RECORD=ON.
namespace recording {

struct operand {
  bool present = false;
  std::string text;
};

// `result` is "true" or "false" for boolean outcomes, and "expected",
// "unexpected" or "none" for throw checks.
void record_check(const char *file, int line, const char *op,
                  const std::string &expression, const char *result,
                  const operand &left, const operand &right);

template <typename T> operand describe(const T &value) {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  operand result;
  if constexpr (std::is_same_v<type, bool>) {
    result.present = true;
    result.text = value ? "true" : "false";
  } else if constexpr (std::is_integral_v<type>) {
    result.present = true;
    if constexpr (std::is_signed_v<type>) {
      result.text = std::to_string(static_cast<long long>(value));
    } else {
      result.text = std::to_string(static_cast<unsigned long long>(value));
    }
  } else if constexpr (std::is_floating_point_v<type>) {
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "%.17g", static_cast<double>(value));
    if constexpr (sizeof(type) > sizeof(double)) {
      std::snprintf(buffer, sizeof(buffer), "%.17Lg",
                    static_cast<long double>(value));
    }
    result.present = true;
    result.text = buffer;
  } else if constexpr (std::is_enum_v<type>) {
    return describe(static_cast<std::underlying_type_t<type>>(value));
  } else if constexpr (std::is_same_v<type, std::string> ||
                       std::is_same_v<type, std::string_view>) {
    result.present = true;
    result.text.assign(value.data(), value.size());
  }
  return result;
}

inline operand describe_c_string(const char *value) {
  operand result;
  if (value) {
    result.present = true;
    result.text = value;
  }
  return result;
}

template <typename Left, typename Right>
void record_comparison(const char *op, bool held, const Left &left,
                       const Right &right, const char *left_expression,
                       const char *right_expression, const char *relation,
                       const char *file, int line) {
  record_check(file, line, op,
               std::string(left_expression) + " " + relation + " " +
                   right_expression,
               held ? "true" : "false", describe(left), describe(right));
}

} // namespace recording
#endif

class fail_expression {
public:
  fail_expression(const char *file, int line) : file_{file}, line_{line} {}

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4722)
#endif
  [[noreturn]] ~fail_expression() noexcept(false) {
#if defined(DINGO_MATRIX_RECORD)
    recording::record_check(file_, line_, "fail", "FAIL()", "false", {}, {});
#endif
    assertion_failed(message_, file_, line_);
  }
#ifdef _MSC_VER
#pragma warning(pop)
#endif

  fail_expression &operator<<(std::string_view value) {
    message_ += value;
    return *this;
  }

private:
  const char *file_;
  int line_;
  std::string message_;
};

template <typename Left, typename Right>
void assert_equal(Left &&left, Right &&right, const char *left_expression,
                  const char *right_expression, const char *file, int line) {
#if defined(DINGO_MATRIX_RECORD)
  const bool failed = !(left == right);
  recording::record_comparison("eq", !failed, left, right, left_expression,
                               right_expression, "==", file, line);
  if (failed) {
#else
  if (!(left == right)) {
#endif
    assertion_failed(std::string(left_expression) + " == " + right_expression,
                     file, line);
  }
}

template <typename Left, typename Right>
void assert_not_equal(Left &&left, Right &&right, const char *left_expression,
                      const char *right_expression, const char *file,
                      int line) {
#if defined(DINGO_MATRIX_RECORD)
  const bool failed = !(left != right);
  recording::record_comparison("ne", !failed, left, right, left_expression,
                               right_expression, "!=", file, line);
  if (failed) {
#else
  if (!(left != right)) {
#endif
    assertion_failed(std::string(left_expression) + " != " + right_expression,
                     file, line);
  }
}

template <typename Left, typename Right>
void assert_greater(Left &&left, Right &&right, const char *left_expression,
                    const char *right_expression, const char *file, int line) {
#if defined(DINGO_MATRIX_RECORD)
  const bool failed = !(left > right);
  recording::record_comparison("gt", !failed, left, right, left_expression,
                               right_expression, ">", file, line);
  if (failed) {
#else
  if (!(left > right)) {
#endif
    assertion_failed(std::string(left_expression) + " > " + right_expression,
                     file, line);
  }
}

template <typename Left, typename Right>
void assert_greater_equal(Left &&left, Right &&right,
                          const char *left_expression,
                          const char *right_expression, const char *file,
                          int line) {
#if defined(DINGO_MATRIX_RECORD)
  const bool failed = !(left >= right);
  recording::record_comparison("ge", !failed, left, right, left_expression,
                               right_expression, ">=", file, line);
  if (failed) {
#else
  if (!(left >= right)) {
#endif
    assertion_failed(std::string(left_expression) + " >= " + right_expression,
                     file, line);
  }
}

template <typename Left, typename Right>
void assert_less_equal(Left &&left, Right &&right, const char *left_expression,
                       const char *right_expression, const char *file,
                       int line) {
#if defined(DINGO_MATRIX_RECORD)
  const bool failed = !(left <= right);
  recording::record_comparison("le", !failed, left, right, left_expression,
                               right_expression, "<=", file, line);
  if (failed) {
#else
  if (!(left <= right)) {
#endif
    assertion_failed(std::string(left_expression) + " <= " + right_expression,
                     file, line);
  }
}

inline void assert_string_equal(const char *left, const char *right,
                                const char *left_expression,
                                const char *right_expression, const char *file,
                                int line) {
#if defined(DINGO_MATRIX_RECORD)
  const bool failed = std::strcmp(left, right) != 0;
  recording::record_check(
      file, line, "streq",
      std::string(left_expression) + " == " + right_expression,
      failed ? "false" : "true", recording::describe_c_string(left),
      recording::describe_c_string(right));
  if (failed) {
#else
  if (std::strcmp(left, right) != 0) {
#endif
    assertion_failed(std::string(left_expression) + " == " + right_expression,
                     file, line);
  }
}

} // namespace dingo::matrix

#if defined(DINGO_MATRIX_RECORD)
#define ASSERT_TRUE(expression)                                                \
  do {                                                                         \
    const bool dingo_matrix_assertion_failed = !(expression);                  \
    ::dingo::matrix::recording::record_check(                                  \
        __FILE__, __LINE__, "true", #expression,                               \
        dingo_matrix_assertion_failed ? "false" : "true", {}, {});             \
    if (dingo_matrix_assertion_failed) {                                       \
      ::dingo::matrix::assertion_failed(#expression, __FILE__, __LINE__);      \
    }                                                                          \
  } while (false)
#else
#define ASSERT_TRUE(expression)                                                \
  do {                                                                         \
    if (!(expression)) {                                                       \
      ::dingo::matrix::assertion_failed(#expression, __FILE__, __LINE__);      \
    }                                                                          \
  } while (false)
#endif

#define EXPECT_TRUE(expression) ASSERT_TRUE(expression)

#define FAIL() ::dingo::matrix::fail_expression(__FILE__, __LINE__)

#define ASSERT_FALSE(expression) ASSERT_TRUE(!(expression))

#define ASSERT_EQ(left, right)                                                 \
  do {                                                                         \
    ::dingo::matrix::assert_equal((left), (right), #left, #right, __FILE__,    \
                                  __LINE__);                                   \
  } while (false)

#define ASSERT_NE(left, right)                                                 \
  do {                                                                         \
    ::dingo::matrix::assert_not_equal((left), (right), #left, #right,          \
                                      __FILE__, __LINE__);                     \
  } while (false)

#define ASSERT_GT(left, right)                                                 \
  do {                                                                         \
    ::dingo::matrix::assert_greater((left), (right), #left, #right, __FILE__,  \
                                    __LINE__);                                 \
  } while (false)

#define ASSERT_GE(left, right)                                                 \
  do {                                                                         \
    ::dingo::matrix::assert_greater_equal((left), (right), #left, #right,      \
                                          __FILE__, __LINE__);                 \
  } while (false)

#define ASSERT_LE(left, right)                                                 \
  do {                                                                         \
    ::dingo::matrix::assert_less_equal((left), (right), #left, #right,         \
                                       __FILE__, __LINE__);                    \
  } while (false)

#define ASSERT_STREQ(left, right)                                              \
  do {                                                                         \
    ::dingo::matrix::assert_string_equal((left), (right), #left, #right,       \
                                         __FILE__, __LINE__);                  \
  } while (false)

#if defined(DINGO_MATRIX_RECORD)
#define ASSERT_THROW(statement, exception_type)                                \
  do {                                                                         \
    int dingo_matrix_assertion_outcome = 0;                                    \
    try {                                                                      \
      statement;                                                               \
    } catch (const exception_type &) {                                         \
      dingo_matrix_assertion_outcome = 1;                                      \
    } catch (...) {                                                            \
      dingo_matrix_assertion_outcome = 2;                                      \
    }                                                                          \
    ::dingo::matrix::recording::record_check(                                  \
        __FILE__, __LINE__, "throw", #statement " throws " #exception_type,    \
        dingo_matrix_assertion_outcome == 1                                    \
            ? "expected"                                                       \
            : (dingo_matrix_assertion_outcome == 2 ? "unexpected" : "none"),   \
        {}, {});                                                               \
    if (dingo_matrix_assertion_outcome == 2) {                                 \
      ::dingo::matrix::assertion_failed(                                       \
          "unexpected exception from " #statement, __FILE__, __LINE__);        \
    }                                                                          \
    if (dingo_matrix_assertion_outcome == 0) {                                 \
      ::dingo::matrix::assertion_failed("expected " #exception_type            \
                                        " from " #statement,                   \
                                        __FILE__, __LINE__);                   \
    }                                                                          \
  } while (false)
#else
#define ASSERT_THROW(statement, exception_type)                                \
  do {                                                                         \
    bool dingo_matrix_assertion_caught = false;                                \
    try {                                                                      \
      statement;                                                               \
    } catch (const exception_type &) {                                         \
      dingo_matrix_assertion_caught = true;                                    \
    } catch (...) {                                                            \
      ::dingo::matrix::assertion_failed(                                       \
          "unexpected exception from " #statement, __FILE__, __LINE__);        \
    }                                                                          \
    if (!dingo_matrix_assertion_caught) {                                      \
      ::dingo::matrix::assertion_failed("expected " #exception_type            \
                                        " from " #statement,                   \
                                        __FILE__, __LINE__);                   \
    }                                                                          \
  } while (false)
#endif
