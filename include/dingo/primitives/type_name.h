//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

// The names of types in diagnostics, spelled as the current implementation
// spells them: the compiler's spelling of a type that is not a pointer or a
// reference, with its cv-qualifiers in front, and pointers and references
// composed around it.

#include <dingo/base/mechanics.h>

#include <cstddef>
#include <string>
#include <string_view>
#include <type_traits>

namespace dingo::core {

namespace detail {
// The text around the type in the compiler's signature of type_name, found
// once from a sample type.
inline constexpr std::string_view type_name_sample = type_name<void>();

constexpr std::size_t find_text(std::string_view text,
                                std::string_view needle) {
  for (std::size_t index = 0; index + needle.size() <= text.size(); ++index) {
    if (text.substr(index, needle.size()) == needle) {
      return index;
    }
  }
  return text.size();
}

inline constexpr std::size_t type_name_prefix =
    find_text(type_name_sample, "void");
static_assert(type_name_prefix != type_name_sample.size(),
              "the compiler's signature of type_name does not name the type");
inline constexpr std::size_t type_name_trailer =
    type_name_sample.size() - type_name_prefix -
    std::string_view("void").size();
} // namespace detail

// The compiler's spelling of `Type`.
template <typename Type> constexpr std::string_view spelled_type_name() {
  constexpr std::string_view signature = type_name<Type>();
  return signature.substr(detail::type_name_prefix,
                          signature.size() - detail::type_name_prefix -
                              detail::type_name_trailer);
}

template <typename Type> void append_type_name(std::string &name) {
  if constexpr (std::is_lvalue_reference_v<Type>) {
    append_type_name<std::remove_reference_t<Type>>(name);
    name += "&";
  } else if constexpr (std::is_rvalue_reference_v<Type>) {
    append_type_name<std::remove_reference_t<Type>>(name);
    name += "&&";
  } else if constexpr (std::is_pointer_v<Type>) {
    append_type_name<std::remove_pointer_t<Type>>(name);
    name += "*";
    if constexpr (std::is_const_v<Type>) {
      name += " const";
    }
    if constexpr (std::is_volatile_v<Type>) {
      name += " volatile";
    }
  } else {
    if constexpr (std::is_const_v<Type>) {
      name += "const ";
    }
    if constexpr (std::is_volatile_v<Type>) {
      name += "volatile ";
    }
    name += spelled_type_name<std::remove_cv_t<Type>>();
  }
}

template <typename Type> std::string type_name_of() {
  std::string name;
  append_type_name<Type>(name);
  return name;
}

} // namespace dingo::core
