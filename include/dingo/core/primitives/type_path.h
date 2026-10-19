//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

// The path of the types whose objects are being constructed on this thread,
// outermost first, which diagnostics report as what required a failed request.

#include <dingo/core/primitives/type_name.h>

#include <string>

namespace dingo::core {

class type_frame {
public:
  // Enters the construction of an object of type `Type`.
  template <typename Type> static type_frame of() {
    return type_frame(&append_type_name<Type>);
  }

  type_frame(const type_frame &) = delete;
  type_frame &operator=(const type_frame &) = delete;
  ~type_frame() { active_ = parent_; }

  static bool active() { return active_ != nullptr; }

  // Appends the name of the innermost type.
  static void append_innermost(std::string &text) { active_->append_(text); }

  // Appends the names of the types, outermost first, separated by " -> ".
  static void append_path(std::string &text) { append_from(active_, text); }

private:
  using append_function = void (*)(std::string &);

  explicit type_frame(append_function append)
      : append_(append), parent_(active_) {
    active_ = this;
  }

  static void append_from(const type_frame *frame, std::string &text) {
    if (frame->parent_ != nullptr) {
      append_from(frame->parent_, text);
      text += " -> ";
    }
    frame->append_(text);
  }

  append_function append_;
  const type_frame *parent_;
  static inline thread_local const type_frame *active_ = nullptr;
};

} // namespace dingo::core
