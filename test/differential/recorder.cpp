//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

// Differential-testing recorder for the generated matrix tests. Assertions in
// matrix/common/assertions.h report every observation here; when the
// DINGO_MATRIX_RECORD_FILE environment variable names a file, the
// observations are appended to it as JSON Lines. See the record format in
// the differential harness documentation.

#if !defined(DINGO_MATRIX_RECORD)
#error "recorder.cpp requires DINGO_MATRIX_RECORD"
#endif

#include <gtest/gtest.h>

// The matrix assertion macros intentionally shadow the GoogleTest ones; this
// translation unit only needs the recording declarations from the header.
#undef FAIL
#undef EXPECT_TRUE
#undef ASSERT_TRUE
#undef ASSERT_FALSE
#undef ASSERT_EQ
#undef ASSERT_NE
#undef ASSERT_GT
#undef ASSERT_GE
#undef ASSERT_LE
#undef ASSERT_STREQ
#undef ASSERT_THROW

#include "matrix/common/assertions.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <mutex>
#include <string>
#include <utility>

namespace dingo::matrix::recording {
namespace {

// Normalizes a path so that it is identical between two build trees.
std::string normalize_path(const std::string &path) {
  static const std::string generated = "/generated/matrix/";
  static const std::string source = "/test/matrix/";
  if (auto position = path.find(generated); position != std::string::npos) {
    return path.substr(position + 1);
  }
  if (auto position = path.find(source); position != std::string::npos) {
    return path.substr(position + 1);
  }
  auto slash = path.find_last_of("/\\");
  return slash == std::string::npos ? path : path.substr(slash + 1);
}

bool is_path_delimiter(char c) {
  return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '"' ||
         c == '\'' || c == '(' || c == ')' || c == '<' || c == '>' ||
         c == ',' || c == ';' || c == '[' || c == ']' || c == '`';
}

// Normalizes every absolute path in a free-form message.
std::string normalize_message(const std::string &message) {
  std::string result;
  std::size_t i = 0;
  while (i < message.size()) {
    if (message[i] == '/' && (i == 0 || is_path_delimiter(message[i - 1]) ||
                              message[i - 1] == '=')) {
      std::size_t end = i;
      while (end < message.size() && !is_path_delimiter(message[end])) {
        ++end;
      }
      std::string token = message.substr(i, end - i);
      if (token.find('/', 1) != std::string::npos) {
        result += normalize_path(token);
      } else {
        result += token;
      }
      i = end;
    } else {
      result += message[i++];
    }
  }
  return result;
}

void append_escaped(std::string &out, const std::string &text) {
  static const char hex[] = "0123456789abcdef";
  const auto put_unit = [&](unsigned unit) {
    out += "\\u00";
    out += hex[(unit >> 4) & 0xf];
    out += hex[unit & 0xf];
  };
  out += '"';
  std::size_t i = 0;
  while (i < text.size()) {
    const auto c = static_cast<unsigned char>(text[i]);
    if (c == '"') {
      out += "\\\"";
    } else if (c == '\\') {
      out += "\\\\";
    } else if (c == '\n') {
      out += "\\n";
    } else if (c == '\r') {
      out += "\\r";
    } else if (c == '\t') {
      out += "\\t";
    } else if (c == '\b') {
      out += "\\b";
    } else if (c == '\f') {
      out += "\\f";
    } else if (c < 0x20 || c == 0x7f) {
      put_unit(c);
    } else if (c < 0x80) {
      out += static_cast<char>(c);
    } else {
      // Copy well-formed UTF-8 sequences; escape stray bytes individually so
      // that the output is always valid JSON.
      std::size_t length = 0;
      if (c >= 0xc2 && c <= 0xdf) {
        length = 2;
      } else if (c >= 0xe0 && c <= 0xef) {
        length = 3;
      } else if (c >= 0xf0 && c <= 0xf4) {
        length = 4;
      }
      bool valid = length != 0 && i + length <= text.size();
      for (std::size_t k = 1; valid && k < length; ++k) {
        valid = (static_cast<unsigned char>(text[i + k]) & 0xc0) == 0x80;
      }
      if (valid && length == 3) {
        const auto second = static_cast<unsigned char>(text[i + 1]);
        valid = !(c == 0xe0 && second < 0xa0) && !(c == 0xed && second > 0x9f);
      } else if (valid && length == 4) {
        const auto second = static_cast<unsigned char>(text[i + 1]);
        valid = !(c == 0xf0 && second < 0x90) && !(c == 0xf4 && second > 0x8f);
      }
      if (valid) {
        out.append(text, i, length);
        i += length;
        continue;
      }
      put_unit(c);
    }
    ++i;
  }
  out += '"';
}

class recorder {
public:
  static recorder &instance() {
    static recorder value;
    return value;
  }

  bool enabled() const { return file_ != nullptr; }

  void open(const char *path) {
    file_ = std::fopen(path, "wb");
    if (!file_) {
      std::fprintf(stderr, "dingo matrix recorder: cannot open '%s'\n", path);
      std::abort();
    }
  }

  void begin_case(std::string name) {
    std::lock_guard<std::mutex> lock(mutex_);
    case_ = std::move(name);
    in_case_ = true;
    occurrences_.clear();
    first_failure_.clear();
    has_failure_ = false;
    skip_message_.clear();
  }

  void note_part_result(const testing::TestPartResult &result) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (result.skipped()) {
      if (skip_message_.empty()) {
        skip_message_ = result.message();
      }
    } else if (result.failed() && !has_failure_) {
      has_failure_ = true;
      first_failure_ = result.message();
    }
  }

  void end_case(const testing::TestInfo &info) {
    std::lock_guard<std::mutex> lock(mutex_);
    const char *status = "passed";
    std::string message;
    if (info.result()->Skipped()) {
      status = "skipped";
      message = skip_message_;
    } else if (info.result()->Failed()) {
      status = "failed";
      message = first_failure_;
    }
    std::string line = "{\"kind\": \"case\", \"case\": ";
    append_escaped(line, case_);
    line += ", \"status\": \"";
    line += status;
    line += "\", \"message\": ";
    append_escaped(line, normalize_message(message));
    line += "}\n";
    write(line);
    std::fflush(file_);
    in_case_ = false;
  }

  void check(const char *file, int line, const char *op,
             const std::string &expression, const char *result,
             const operand &left, const operand &right) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!in_case_) {
      return;
    }
    std::string site = normalize_path(file) + ":" + std::to_string(line);
    const auto occurrence = occurrences_[site]++;

    std::string out = "{\"kind\": \"check\", \"case\": ";
    append_escaped(out, case_);
    out += ", \"site\": ";
    append_escaped(out, site);
    out += ", \"occurrence\": " + std::to_string(occurrence);
    out += ", \"op\": ";
    append_escaped(out, op);
    out += ", \"expr\": ";
    append_escaped(out, expression);
    out += ", \"result\": ";
    const std::string text = result;
    if (text == "true" || text == "false") {
      out += text;
    } else {
      append_escaped(out, text);
    }
    out += ", \"left\": ";
    append_operand(out, left);
    out += ", \"right\": ";
    append_operand(out, right);
    out += "}\n";
    write(out);
  }

private:
  recorder() = default;

  static void append_operand(std::string &out, const operand &value) {
    if (value.present) {
      append_escaped(out, value.text);
    } else {
      out += "null";
    }
  }

  void write(const std::string &line) {
    std::fwrite(line.data(), 1, line.size(), file_);
  }

  std::FILE *file_ = nullptr;
  std::mutex mutex_;
  std::string case_;
  bool in_case_ = false;
  std::map<std::string, std::size_t> occurrences_;
  std::string first_failure_;
  bool has_failure_ = false;
  std::string skip_message_;
};

class listener : public testing::EmptyTestEventListener {
public:
  void OnTestStart(const testing::TestInfo &info) override {
    recorder::instance().begin_case(std::string(info.test_suite_name()) + "." +
                                    info.name());
  }

  void OnTestPartResult(const testing::TestPartResult &result) override {
    recorder::instance().note_part_result(result);
  }

  void OnTestEnd(const testing::TestInfo &info) override {
    recorder::instance().end_case(info);
  }
};

struct registration {
  registration() {
    const char *path = std::getenv("DINGO_MATRIX_RECORD_FILE");
    if (!path || !*path) {
      return;
    }
    recorder::instance().open(path);
    testing::UnitTest::GetInstance()->listeners().Append(new listener);
  }
};

const registration registered;

} // namespace

void record_check(const char *file, int line, const char *op,
                  const std::string &expression, const char *result,
                  const operand &left, const operand &right) {
  auto &instance = recorder::instance();
  if (instance.enabled()) {
    instance.check(file, line, op, expression, result, left, right);
  }
}

} // namespace dingo::matrix::recording
