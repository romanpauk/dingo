//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

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

class fail_expression {
public:
  fail_expression(const char *file, int line) : file_{file}, line_{line} {}

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4722)
#endif
  [[noreturn]] ~fail_expression() noexcept(false) {
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
  if (!(left == right)) {
    assertion_failed(std::string(left_expression) + " == " + right_expression,
                     file, line);
  }
}

template <typename Left, typename Right>
void assert_not_equal(Left &&left, Right &&right, const char *left_expression,
                      const char *right_expression, const char *file,
                      int line) {
  if (!(left != right)) {
    assertion_failed(std::string(left_expression) + " != " + right_expression,
                     file, line);
  }
}

template <typename Left, typename Right>
void assert_greater(Left &&left, Right &&right, const char *left_expression,
                    const char *right_expression, const char *file, int line) {
  if (!(left > right)) {
    assertion_failed(std::string(left_expression) + " > " + right_expression,
                     file, line);
  }
}

template <typename Left, typename Right>
void assert_greater_equal(Left &&left, Right &&right,
                          const char *left_expression,
                          const char *right_expression, const char *file,
                          int line) {
  if (!(left >= right)) {
    assertion_failed(std::string(left_expression) + " >= " + right_expression,
                     file, line);
  }
}

template <typename Left, typename Right>
void assert_less_equal(Left &&left, Right &&right, const char *left_expression,
                       const char *right_expression, const char *file,
                       int line) {
  if (!(left <= right)) {
    assertion_failed(std::string(left_expression) + " <= " + right_expression,
                     file, line);
  }
}

inline void assert_string_equal(const char *left, const char *right,
                                const char *left_expression,
                                const char *right_expression, const char *file,
                                int line) {
  if (std::strcmp(left, right) != 0) {
    assertion_failed(std::string(left_expression) + " == " + right_expression,
                     file, line);
  }
}

} // namespace dingo::matrix

#define ASSERT_TRUE(expression)                                                \
  do {                                                                         \
    if (!(expression)) {                                                       \
      ::dingo::matrix::assertion_failed(#expression, __FILE__, __LINE__);      \
    }                                                                          \
  } while (false)

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
