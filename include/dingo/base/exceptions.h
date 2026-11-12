//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/base/config.h>
#include <dingo/base/none.h>
#include <dingo/base/type_descriptor.h>

#include <cassert>
#include <exception>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace dingo {
template <typename T, auto... Values> struct key_type;

struct exception : std::exception {
  explicit exception(std::string message) : message_(std::move(message)) {
    assert(!message_.empty());
  }

  const char *what() const noexcept override {
    assert(!message_.empty());
    return message_.c_str();
  }

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

struct container_sealed_exception : exception {
  using exception::exception;
};

#ifdef _DEBUG
#endif

namespace detail {
template <typename T> struct lookup_key;

inline void append_text_part(std::string &message, type_descriptor descriptor) {
  append_type_name(message, descriptor);
}

template <typename Text>
void append_text_part(std::string &message, Text &&text) {
  message += std::forward<Text>(text);
}

template <typename... Parts>
void append_text(std::string &message, Parts &&...parts) {
  (append_text_part(message, std::forward<Parts>(parts)), ...);
}

// The resolution path that led to the failing request.
template <typename Context>
void append_type_path_text(std::string &message, const Context &context) {
  if (context.has_type_path()) {
    message += " (required by ";
    context.append_type_path(message);
    message += ")";
  }
}

// The type whose construction made the failing request.
template <typename Context>
void append_active_type_text(std::string &message, const Context &context) {
  if (auto *active_type = context.active_type()) {
    append_text(message, " (required by ", *active_type, ")");
  }
}

// The messages name types by their descriptors, so that a message is made by
// one function for every type it names, rather than by one per type.

// The index type of a lookup key, or null for a key without one.
template <typename Selector>
constexpr const type_descriptor *
lookup_key_index_type(const lookup_key<Selector> &) {
  if constexpr (std::is_same_v<Selector, key_type<none_t>>) {
    return nullptr;
  } else {
    return &described_type_v<Selector>;
  }
}

template <typename Context>
type_not_found_exception
make_type_not_found_exception(type_descriptor request, const Context &context,
                              const type_descriptor *index_type) {
  std::string message = "type not found: ";
  append_type_name(message, request);
  if (index_type != nullptr) {
    append_text(message, " (index type: ", *index_type, ")");
  }
  append_type_path_text(message, context);
  return type_not_found_exception(std::move(message));
}

template <typename Collection, typename ResolveType>
type_not_found_exception make_collection_type_not_found_exception() {
  std::string message = "type not found for collection ";
  append_text(message, detail::described_type_v<Collection>,
              " (element type: ", detail::described_type_v<ResolveType>, ")");
  return type_not_found_exception(std::move(message));
}

template <typename Context>
type_ambiguous_exception make_type_ambiguous_exception(type_descriptor request,
                                                       const Context &context) {
  std::string message = "type resolution is ambiguous: ";
  append_type_name(message, request);
  append_active_type_text(message, context);
  return type_ambiguous_exception(std::move(message));
}

inline type_not_convertible_exception
make_type_not_convertible_exception(type_descriptor target_type,
                                    type_descriptor source_type) {
  std::string message = "type is not convertible to ";
  append_text(message, target_type, " from ", source_type);
  return type_not_convertible_exception(std::move(message));
}

template <typename Context>
inline type_not_convertible_exception
make_type_not_convertible_exception(type_descriptor target_type,
                                    type_descriptor source_type,
                                    const Context &context) {
  std::string message = "type is not convertible to ";
  append_text(message, target_type, " from ", source_type);
  append_active_type_text(message, context);
  return type_not_convertible_exception(std::move(message));
}

template <typename Context>
type_recursion_exception make_type_recursion_exception(type_descriptor type,
                                                       const Context &context) {
  std::string message = "recursive dependency detected: ";
  if (context.has_type_path()) {
    context.append_type_path(message);
  } else {
    append_type_name(message, type);
  }

  return type_recursion_exception(std::move(message));
}

// The exceptions a resolution raises, thrown by a function of their own: a
// throw is a call where it is raised, rather than the code that makes and
// throws the exception, once in every function that raises it. The functions
// are not inlined, so that a resolution that may throw stays small enough to
// be inlined itself.
[[noreturn]] DINGO_NOINLINE inline void
throw_type_not_convertible(type_descriptor target_type,
                           type_descriptor source_type) {
  throw make_type_not_convertible_exception(target_type, source_type);
}

template <typename Context>
[[noreturn]] DINGO_NOINLINE void
throw_type_not_convertible(type_descriptor target_type,
                           type_descriptor source_type,
                           const Context &context) {
  throw make_type_not_convertible_exception(target_type, source_type, context);
}

template <typename Context>
[[noreturn]] DINGO_NOINLINE void
throw_type_not_found(type_descriptor request, const Context &context,
                     const type_descriptor *index_type) {
  throw make_type_not_found_exception(request, context, index_type);
}

template <typename Context>
[[noreturn]] DINGO_NOINLINE void throw_type_ambiguous(type_descriptor request,
                                                      const Context &context) {
  throw make_type_ambiguous_exception(request, context);
}

template <typename Context>
[[noreturn]] DINGO_NOINLINE void throw_type_recursion(type_descriptor type,
                                                      const Context &context) {
  throw make_type_recursion_exception(type, context);
}

// The key type a registration is looked up by: the type of the key it is
// registered with, or the selector of its lookup key.
template <typename BackendKey, typename Selector>
constexpr type_descriptor registration_key_type(const lookup_key<Selector> &) {
  if constexpr (!std::is_same_v<std::decay_t<BackendKey>, none_t>) {
    return detail::described_type_v<std::decay_t<BackendKey>>;
  } else if constexpr (std::is_same_v<Selector, key_type<none_t>>) {
    return detail::described_type_v<none_t>;
  } else {
    return detail::described_type_v<Selector>;
  }
}

inline std::string make_lookup_registration_message(
    const char *prefix, type_descriptor interface_type, type_descriptor storage,
    type_descriptor key_type) {
  std::string message = prefix;
  append_text(message, interface_type, ", storage ", storage, ", key type ",
              key_type);
  return message;
}

inline lookup_already_registered_exception
make_lookup_already_registered_exception(type_descriptor interface_type,
                                         type_descriptor storage,
                                         type_descriptor key_type) {
  return lookup_already_registered_exception(
      make_lookup_registration_message("lookup already registered: interface ",
                                       interface_type, storage, key_type));
}

inline container_sealed_exception
make_container_sealed_exception(type_descriptor interface_type,
                                type_descriptor storage,
                                type_descriptor key_type) {
  return container_sealed_exception(make_lookup_registration_message(
      "container sealed by resolve, lookup can not be replaced: interface ",
      interface_type, storage, key_type));
}

} // namespace detail
} // namespace dingo
