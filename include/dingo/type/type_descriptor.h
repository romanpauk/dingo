//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/core/config.h>

#include <cstdint>
#include <string>
#include <string_view>
#include <type_traits>

namespace dingo {
enum class type_cv_flags : std::uint8_t {
  none = 0,
  is_const = 1 << 0,
  is_volatile = 1 << 1,
};

enum class type_reference_kind : std::uint8_t {
  none = 0,
  lvalue = 1,
  rvalue = 2,
};

struct type_descriptor {
  std::string_view raw_name;
  type_cv_flags cv = type_cv_flags::none;
  type_reference_kind reference = type_reference_kind::none;
  type_descriptor (*pointee)() = nullptr;
};

constexpr bool operator==(type_descriptor lhs, type_descriptor rhs) {
  if (lhs.raw_name != rhs.raw_name || lhs.cv != rhs.cv ||
      lhs.reference != rhs.reference) {
    return false;
  }

  if (lhs.pointee == nullptr || rhs.pointee == nullptr) {
    return lhs.pointee == rhs.pointee;
  }

  return lhs.pointee() == rhs.pointee();
}

template <typename T> constexpr std::string_view raw_type_name();
template <typename T> constexpr type_descriptor describe_type();
inline void append_type_name(std::string &name, type_descriptor descriptor);

namespace detail {

constexpr size_t type_name_not_found = static_cast<size_t>(-1);

constexpr size_t type_name_find(std::string_view haystack,
                                std::string_view needle, size_t offset = 0) {
  if (needle.empty()) {
    return offset <= haystack.size() ? offset : type_name_not_found;
  }

  for (size_t i = offset; i + needle.size() <= haystack.size(); ++i) {
    size_t j = 0;
    for (; j < needle.size(); ++j) {
      if (haystack[i + j] != needle[j]) {
        break;
      }
    }
    if (j == needle.size()) {
      return i;
    }
  }

  return type_name_not_found;
}

template <typename T> constexpr std::string_view wrapped_type_name() {
#if defined(__clang__) || defined(__GNUC__)
  return __PRETTY_FUNCTION__;
#elif defined(_MSC_VER)
  return __FUNCSIG__;
#else
  return "unsupported compiler";
#endif
}

constexpr std::string_view wrapped_type_name_prefix() {
#if defined(__clang__)
  return "[T = ";
#elif defined(__GNUC__)
  return "with T = ";
#elif defined(_MSC_VER)
  return "wrapped_type_name<";
#else
  return "";
#endif
}

constexpr std::string_view wrapped_type_name_suffix() {
#if defined(__clang__)
  return "]";
#elif defined(__GNUC__)
  return ";";
#elif defined(_MSC_VER)
  return ">(void)";
#else
  return "";
#endif
}

// Compiler type-name signatures have fixed text around T. Locate that text
// once instead of repeating two constexpr linear searches for every type.
constexpr auto wrapped_type_name_sample = wrapped_type_name<void>();
constexpr auto wrapped_type_name_prefix_pos =
    type_name_find(wrapped_type_name_sample, wrapped_type_name_prefix());

static_assert(wrapped_type_name_prefix_pos != type_name_not_found,
              "failed to parse compiler type name prefix");

constexpr auto wrapped_type_name_start =
    wrapped_type_name_prefix_pos + wrapped_type_name_prefix().size();
constexpr auto wrapped_type_name_suffix_pos =
    type_name_find(wrapped_type_name_sample, wrapped_type_name_suffix(),
                   wrapped_type_name_start);

static_assert(wrapped_type_name_suffix_pos != type_name_not_found,
              "failed to parse compiler type name suffix");

constexpr auto wrapped_type_name_trailer_size =
    wrapped_type_name_sample.size() - wrapped_type_name_suffix_pos;

template <typename T> constexpr std::string_view raw_type_name() {
  constexpr auto wrapped = wrapped_type_name<T>();
  constexpr auto end = wrapped.size() - wrapped_type_name_trailer_size;
  return std::string_view(wrapped.data() + wrapped_type_name_start,
                          end - wrapped_type_name_start);
}

constexpr type_cv_flags operator|(type_cv_flags lhs, type_cv_flags rhs) {
  return static_cast<type_cv_flags>(static_cast<std::uint8_t>(lhs) |
                                    static_cast<std::uint8_t>(rhs));
}

constexpr bool has_cv_flag(type_cv_flags flags, type_cv_flags flag) {
  return (static_cast<std::uint8_t>(flags) & static_cast<std::uint8_t>(flag)) !=
         0;
}

template <typename T> constexpr type_cv_flags make_type_cv_flags() {
  type_cv_flags flags = type_cv_flags::none;

  if constexpr (std::is_const_v<T>) {
    flags = flags | type_cv_flags::is_const;
  }

  if constexpr (std::is_volatile_v<T>) {
    flags = flags | type_cv_flags::is_volatile;
  }

  return flags;
}

template <typename T> constexpr type_reference_kind make_type_reference_kind() {
  if constexpr (std::is_lvalue_reference_v<T>) {
    return type_reference_kind::lvalue;
  } else if constexpr (std::is_rvalue_reference_v<T>) {
    return type_reference_kind::rvalue;
  } else {
    return type_reference_kind::none;
  }
}

template <typename T> constexpr type_descriptor make_type_descriptor() {
  if constexpr (std::is_pointer_v<T>) {
    return {{},
            make_type_cv_flags<T>(),
            type_reference_kind::none,
            &make_type_descriptor<std::remove_pointer_t<T>>};
  } else {
    return {raw_type_name<std::remove_cv_t<T>>(), make_type_cv_flags<T>(),
            type_reference_kind::none, nullptr};
  }
}

inline void append_type_cv(std::string &name, type_cv_flags flags,
                           std::string_view separator = " ") {
  if (has_cv_flag(flags, type_cv_flags::is_const)) {
    name += separator;
    name += "const";
    separator = " ";
  }

  if (has_cv_flag(flags, type_cv_flags::is_volatile)) {
    name += separator;
    name += "volatile";
  }
}

inline void append_described_type_name(std::string &name,
                                       type_descriptor descriptor) {
  if (descriptor.pointee != nullptr) {
    append_described_type_name(name, descriptor.pointee());
    name += "*";
    append_type_cv(name, descriptor.cv);
  } else {
    if (has_cv_flag(descriptor.cv, type_cv_flags::is_const)) {
      name += "const ";
    }

    if (has_cv_flag(descriptor.cv, type_cv_flags::is_volatile)) {
      name += "volatile ";
    }

    name += descriptor.raw_name;
  }

  if (descriptor.reference == type_reference_kind::lvalue) {
    name += "&";
  } else if (descriptor.reference == type_reference_kind::rvalue) {
    name += "&&";
  }
}

} // namespace detail

template <typename T> constexpr std::string_view raw_type_name() {
  return detail::raw_type_name<T>();
}

template <typename T> constexpr type_descriptor describe_type() {
  auto descriptor = detail::make_type_descriptor<std::remove_reference_t<T>>();
  descriptor.reference = detail::make_type_reference_kind<T>();
  return descriptor;
}

inline void append_type_name(std::string &name, type_descriptor descriptor) {
  detail::append_described_type_name(name, descriptor);
}

} // namespace dingo
