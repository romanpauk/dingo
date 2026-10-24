//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

// The message of a diagnostic reported when a program is compiled, built as
// a constant: the text of the diagnostic with the names of its subjects in
// its placeholders, spelled as type_name_of spells them. A compiler with
// user-generated static_assert messages prints it.

#include <dingo/diagnostics/type_name.h>

#include <cstddef>
#include <string_view>
#include <type_traits>

namespace dingo::core {

namespace detail {
// Writes `text` at `out` from `at`, or counts it when `out` is null, and
// returns where it ends.
constexpr std::size_t write_text(char *out, std::size_t at,
                                 std::string_view text) {
  for (char letter : text) {
    if (out != nullptr) {
      out[at] = letter;
    }
    ++at;
  }
  return at;
}
} // namespace detail

// The path of the types whose objects are being constructed, outermost first,
// as a subject of a diagnostic: spelled as the names of `Types` separated by
// " -> ", as a diagnostic reported at runtime spells the path.
template <typename... Types> struct dependency_path {};

template <typename Type>
constexpr std::size_t write_type_name(char *out, std::size_t at);

namespace detail {
template <typename Type> struct is_dependency_path : std::false_type {};

template <typename... Types>
struct is_dependency_path<dependency_path<Types...>> : std::true_type {};

template <typename First, typename... Rest>
constexpr std::size_t write_path(char *out, std::size_t at,
                                 dependency_path<First, Rest...> /*path*/) {
  at = write_type_name<First>(out, at);
  ((at = write_type_name<Rest>(out, write_text(out, at, " -> "))), ...);
  return at;
}
} // namespace detail

// Writes the name of `Type` as type_name_of spells it, as write_text writes.
template <typename Type>
constexpr std::size_t write_type_name(char *out, std::size_t at) {
  if constexpr (detail::is_dependency_path<Type>::value) {
    return detail::write_path(out, at, Type());
  } else if constexpr (std::is_lvalue_reference_v<Type>) {
    at = write_type_name<std::remove_reference_t<Type>>(out, at);
    return detail::write_text(out, at, "&");
  } else if constexpr (std::is_rvalue_reference_v<Type>) {
    at = write_type_name<std::remove_reference_t<Type>>(out, at);
    return detail::write_text(out, at, "&&");
  } else if constexpr (std::is_pointer_v<Type>) {
    at = write_type_name<std::remove_pointer_t<Type>>(out, at);
    at = detail::write_text(out, at, "*");
    if constexpr (std::is_const_v<Type>) {
      at = detail::write_text(out, at, " const");
    }
    if constexpr (std::is_volatile_v<Type>) {
      at = detail::write_text(out, at, " volatile");
    }
    return at;
  } else {
    if constexpr (std::is_const_v<Type>) {
      at = detail::write_text(out, at, "const ");
    }
    if constexpr (std::is_volatile_v<Type>) {
      at = detail::write_text(out, at, "volatile ");
    }
    return detail::write_text(out, at,
                              spelled_type_name<std::remove_cv_t<Type>>());
  }
}

// The message of the diagnostic whose text is `Text`, a diag_text with
// `pieces`, the text around its placeholders, with `Subjects` in them. It has
// the size() and data() a user-generated static_assert message takes.
template <typename Text, typename... Subjects> class formatted_message {
  static constexpr std::size_t write(char *out) {
    std::size_t at = detail::write_text(out, 0, Text::pieces[0]);
    std::size_t piece = 0;
    ((at = write_type_name<Subjects>(out, at),
      at = detail::write_text(out, at, Text::pieces[++piece])),
     ...);
    return at;
  }

  static constexpr std::size_t length = write(nullptr);

  struct buffer {
    char letters[length + 1];
  };

  static constexpr buffer text = [] {
    buffer made{};
    write(made.letters);
    return made;
  }();

public:
  constexpr std::size_t size() const { return length; }
  constexpr const char *data() const { return text.letters; }
  constexpr std::string_view view() const { return {text.letters, length}; }
};

} // namespace dingo::core
