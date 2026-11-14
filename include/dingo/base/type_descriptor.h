//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/base/config.h>

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
  // Diagnostics only. Compilers may spell one type differently per
  // translation unit, so identity is carried by `tag`, never by this text.
  std::string_view raw_name;
  // Address of the per-type detail::type_tag<T>; null for pointer levels.
  const void *tag = nullptr;
  type_cv_flags cv = type_cv_flags::none;
  type_reference_kind reference = type_reference_kind::none;
  // Descriptor of the pointed-to type; null unless this is a pointer level.
  const type_descriptor *pointee = nullptr;
};

constexpr bool operator==(type_descriptor lhs, type_descriptor rhs) {
  if (lhs.tag != rhs.tag || lhs.cv != rhs.cv ||
      lhs.reference != rhs.reference) {
    return false;
  }

  if (lhs.pointee == nullptr || rhs.pointee == nullptr) {
    return lhs.pointee == rhs.pointee;
  }

  return *lhs.pointee == *rhs.pointee;
}

template <typename T> constexpr std::string_view raw_type_name();
template <typename T> constexpr type_descriptor describe_type();
inline void append_type_name(std::string &name, type_descriptor descriptor);

namespace detail {

// One object per type, merged across translation units by vague linkage, so
// its address identifies the type regardless of how a compiler spells it. It
// holds its own address: a linker may fold identical data, as MSVC's
// /OPT:ICF does, and no two tags are identical.
struct type_tag_object {
  const void *self;
};

template <typename T> inline const type_tag_object type_tag{&type_tag<T>};

constexpr bool same_type_shape(type_descriptor lhs, type_descriptor rhs) {
  if (lhs.reference != rhs.reference ||
      (lhs.pointee == nullptr) != (rhs.pointee == nullptr)) {
    return false;
  }

  if (lhs.pointee == nullptr) {
    return lhs.tag == rhs.tag;
  }

  return same_type_shape(*lhs.pointee, *rhs.pointee);
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

// The text a compiler's signature puts before and after T.
#if defined(__clang__)
inline constexpr std::string_view wrapped_type_name_prefix = "[T = ";
inline constexpr std::string_view wrapped_type_name_suffix = "]";
#elif defined(__GNUC__)
inline constexpr std::string_view wrapped_type_name_prefix = "with T = ";
inline constexpr std::string_view wrapped_type_name_suffix = ";";
#elif defined(_MSC_VER)
inline constexpr std::string_view wrapped_type_name_prefix =
    "wrapped_type_name<";
inline constexpr std::string_view wrapped_type_name_suffix = ">(void)";
#else
inline constexpr std::string_view wrapped_type_name_prefix = "";
inline constexpr std::string_view wrapped_type_name_suffix = "";
#endif

// Compiler type-name signatures have fixed text around T. Locate that text
// once instead of repeating two constexpr linear searches for every type.
constexpr auto wrapped_type_name_sample = wrapped_type_name<void>();
constexpr auto wrapped_type_name_prefix_pos =
    wrapped_type_name_sample.find(wrapped_type_name_prefix);

static_assert(wrapped_type_name_prefix_pos != std::string_view::npos,
              "failed to parse compiler type name prefix");

constexpr auto wrapped_type_name_start =
    wrapped_type_name_prefix_pos + wrapped_type_name_prefix.size();
constexpr auto wrapped_type_name_suffix_pos = wrapped_type_name_sample.find(
    wrapped_type_name_suffix, wrapped_type_name_start);

static_assert(wrapped_type_name_suffix_pos != std::string_view::npos,
              "failed to parse compiler type name suffix");

constexpr auto wrapped_type_name_trailer_size =
    wrapped_type_name_sample.size() - wrapped_type_name_suffix_pos;

// The name of a type in a compiler's signature, without the text around it:
// a function of no type, so that a type instantiates only its signature.
constexpr std::string_view unwrapped_type_name(std::string_view wrapped) {
  return wrapped.substr(wrapped_type_name_start,
                        wrapped.size() - wrapped_type_name_trailer_size -
                            wrapped_type_name_start);
}

template <typename T>
inline constexpr std::string_view raw_type_name_v =
    unwrapped_type_name(wrapped_type_name<T>());

template <typename T> constexpr std::string_view raw_type_name() {
  return raw_type_name_v<T>;
}

constexpr type_cv_flags operator|(type_cv_flags lhs, type_cv_flags rhs) {
  return static_cast<type_cv_flags>(static_cast<std::uint8_t>(lhs) |
                                    static_cast<std::uint8_t>(rhs));
}

constexpr bool has_cv_flag(type_cv_flags flags, type_cv_flags flag) {
  return (static_cast<std::uint8_t>(flags) & static_cast<std::uint8_t>(flag)) !=
         0;
}

constexpr bool contains_cv_flags(type_cv_flags flags, type_cv_flags required) {
  return (static_cast<std::uint8_t>(flags) &
          static_cast<std::uint8_t>(required)) ==
         static_cast<std::uint8_t>(required);
}

constexpr bool is_qualification_conversion(type_descriptor source,
                                           type_descriptor target, size_t depth,
                                           bool intermediate_const) {
  // Top-level pointer qualifiers do not affect a pointer value conversion.
  const bool compare_cv = depth != 0 ||
                          source.reference != type_reference_kind::none ||
                          source.pointee == nullptr;
  if (compare_cv && !contains_cv_flags(target.cv, source.cv)) {
    return false;
  }

  // Adding qualifiers below the first pointee requires every intervening
  // pointer to be const, as in T** -> const T* const*.
  if (compare_cv && source.cv != target.cv && depth > 1 &&
      !intermediate_const) {
    return false;
  }

  if (source.pointee == nullptr) {
    return true;
  }

  const bool next_intermediate_const =
      intermediate_const &&
      (depth == 0 || has_cv_flag(target.cv, type_cv_flags::is_const));
  return is_qualification_conversion(*source.pointee, *target.pointee,
                                     depth + 1, next_intermediate_const);
}

constexpr bool qualifies_as(type_descriptor source, type_descriptor target) {
  if (!same_type_shape(source, target)) {
    return false;
  }

  // Plain value requests intentionally accept every top-level cv variant.
  if (source.reference == type_reference_kind::none &&
      source.pointee == nullptr) {
    return true;
  }

  return is_qualification_conversion(source, target, 0, true);
}

constexpr type_cv_flags make_type_cv_flags(bool is_const, bool is_volatile) {
  type_cv_flags flags = type_cv_flags::none;
  if (is_const) {
    flags = flags | type_cv_flags::is_const;
  }
  if (is_volatile) {
    flags = flags | type_cv_flags::is_volatile;
  }
  return flags;
}

template <typename T>
inline constexpr type_cv_flags type_cv_flags_v =
    make_type_cv_flags(std::is_const_v<T>, std::is_volatile_v<T>);

// The descriptor of a type that is not a reference, as constant data of one
// class for the type. A pointer level refers to the descriptor of its
// pointee as data, so comparing descriptors at runtime never calls through a
// function pointer.
template <typename T, bool = std::is_pointer_v<T>> struct type_descriptor_of {
  static constexpr type_descriptor value{
      raw_type_name_v<std::remove_cv_t<T>>, &type_tag<std::remove_cv_t<T>>,
      type_cv_flags_v<T>, type_reference_kind::none, nullptr};
};

template <typename T> struct type_descriptor_of<T, true> {
  static constexpr type_descriptor value{
      {},
      nullptr,
      type_cv_flags_v<T>,
      type_reference_kind::none,
      &type_descriptor_of<std::remove_pointer_t<T>>::value};
};

template <typename T>
inline constexpr type_descriptor type_descriptor_v =
    type_descriptor_of<T>::value;

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
    append_described_type_name(name, *descriptor.pointee);
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

namespace detail {
constexpr type_descriptor with_reference_kind(type_descriptor descriptor,
                                              bool lvalue, bool rvalue) {
  descriptor.reference = lvalue   ? type_reference_kind::lvalue
                         : rvalue ? type_reference_kind::rvalue
                                  : type_reference_kind::none;
  return descriptor;
}

// The descriptor of a type with its reference kind, made once at compile
// time, of the descriptor of the type it refers to: a function computing it
// would be emitted for every type a message names.
template <typename T>
inline constexpr type_descriptor described_type_v =
    with_reference_kind(type_descriptor_of<std::remove_reference_t<T>>::value,
                        std::is_lvalue_reference_v<T>,
                        std::is_rvalue_reference_v<T>);
} // namespace detail

template <typename T> constexpr type_descriptor describe_type() {
  return detail::described_type_v<T>;
}

inline void append_type_name(std::string &name, type_descriptor descriptor) {
  detail::append_described_type_name(name, descriptor);
}

} // namespace dingo
