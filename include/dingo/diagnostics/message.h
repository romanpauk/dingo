//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

// The text of a diagnostic: a message template whose `{name}` placeholders are
// replaced by the values of the named fields.

#include <cstddef>
#include <initializer_list>
#include <string>
#include <string_view>

namespace dingo {

struct message_field {
  std::string_view name;
  std::string_view value;
};

inline std::string format_message(std::string_view pattern,
                                  std::initializer_list<message_field> fields) {
  std::string text;
  std::size_t index = 0;
  while (index < pattern.size()) {
    const std::size_t open = pattern.find('{', index);
    if (open == std::string_view::npos) {
      break;
    }
    const std::size_t close = pattern.find('}', open);
    text += pattern.substr(index, open - index);
    const std::string_view name = pattern.substr(open + 1, close - open - 1);
    for (const message_field &field : fields) {
      if (field.name == name) {
        text += field.value;
      }
    }
    index = close + 1;
  }
  text += pattern.substr(index);
  return text;
}

} // namespace dingo
