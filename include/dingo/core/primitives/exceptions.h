//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

// The exceptions of the public API.

#include <cassert>
#include <exception>
#include <string>
#include <utility>

namespace dingo {

struct exception : std::exception {
  explicit exception(std::string message) : message_(std::move(message)) {
    assert(!message_.empty());
  }

  const char *what() const noexcept override { return message_.c_str(); }

protected:
  std::string message_;
};

struct type_not_found_exception : exception {
  using exception::exception;
};

struct type_not_convertible_exception : exception {
  using exception::exception;
};

struct type_ambiguous_exception : exception {
  using exception::exception;
};

struct type_recursion_exception : exception {
  using exception::exception;
};

struct lookup_already_registered_exception : exception {
  using exception::exception;
};

struct type_index_out_of_range_exception : exception {
  using exception::exception;
};

struct virtual_pointer_exception : exception {
  using exception::exception;
};

} // namespace dingo
