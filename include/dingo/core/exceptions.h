//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/core/config.h>
#include <dingo/core/none.h>
#include <dingo/type/type_descriptor.h>

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

struct type_index_out_of_range_exception : exception {
  using exception::exception;
};

struct virtual_pointer_exception : exception {
  using exception::exception;
};

#ifdef _DEBUG
struct arena_allocation_exception : exception {
  using exception::exception;
};
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

inline void append_text(std::string &) {}

template <typename First, typename... Rest>
void append_text(std::string &message, First &&first, Rest &&...rest) {
  append_text_part(message, std::forward<First>(first));
  append_text(message, std::forward<Rest>(rest)...);
}

template <typename Context>
void append_type_path_text(std::string &message, const Context &context) {
  if (context.has_type_path()) {
    std::string resolution_path;
    context.append_type_path(resolution_path);
    message += " (required by ";
    message += resolution_path;
    message += ")";
  }
}

template <typename Request>
type_not_found_exception make_type_not_found_exception() {
  std::string message = "type not found: ";
  append_type_name(message, describe_type<Request>());
  return type_not_found_exception(std::move(message));
}

template <typename Request, typename Context>
type_not_found_exception make_type_not_found_exception(const Context &context) {
  std::string message = "type not found: ";
  append_type_name(message, describe_type<Request>());
  append_type_path_text(message, context);
  return type_not_found_exception(std::move(message));
}

template <typename Request, typename IdType>
type_not_found_exception make_type_not_found_exception() {
  std::string message = "type not found: ";
  append_text(message, describe_type<Request>(),
              " (index type: ", describe_type<IdType>(), ")");
  return type_not_found_exception(std::move(message));
}

template <typename Request, typename IdType, typename Context>
type_not_found_exception make_type_not_found_exception(const Context &context) {
  std::string message = "type not found: ";
  append_text(message, describe_type<Request>(),
              " (index type: ", describe_type<IdType>(), ")");
  append_type_path_text(message, context);
  return type_not_found_exception(std::move(message));
}

template <typename Selector>
void append_lookup_key_text(std::string &message,
                            const lookup_key<Selector> &) {
  if constexpr (!std::is_same_v<Selector, key_type<none_t>>) {
    append_text(message, " (index type: ", describe_type<Selector>(), ")");
  }
}

template <typename Request, typename Context, typename LookupKey>
type_not_found_exception make_type_not_found_exception(const Context &context,
                                                       const LookupKey &key) {
  std::string message = "type not found: ";
  append_type_name(message, describe_type<Request>());
  append_lookup_key_text(message, key);
  append_type_path_text(message, context);
  return type_not_found_exception(std::move(message));
}

template <typename Collection, typename ResolveType>
type_not_found_exception make_collection_type_not_found_exception() {
  std::string message = "type not found for collection ";
  append_text(message, describe_type<Collection>(),
              " (element type: ", describe_type<ResolveType>(), ")");
  return type_not_found_exception(std::move(message));
}

template <typename Request>
type_ambiguous_exception make_type_ambiguous_exception() {
  std::string message = "type resolution is ambiguous: ";
  append_type_name(message, describe_type<Request>());
  return type_ambiguous_exception(std::move(message));
}

template <typename Request, typename Context>
type_ambiguous_exception make_type_ambiguous_exception(const Context &context) {
  std::string message = "type resolution is ambiguous: ";
  append_type_name(message, describe_type<Request>());
  if (auto *active_type = context.active_type()) {
    message += " (required by ";
    append_type_name(message, *active_type);
    message += ")";
  }

  return type_ambiguous_exception(std::move(message));
}

inline type_not_convertible_exception
make_type_not_convertible_exception(type_descriptor target_type,
                                    type_descriptor source_type) {
  std::string message = "type is not convertible to ";
  append_type_name(message, target_type);
  message += " from ";
  append_type_name(message, source_type);

  return type_not_convertible_exception(std::move(message));
}

template <typename Context>
inline type_not_convertible_exception
make_type_not_convertible_exception(type_descriptor target_type,
                                    type_descriptor source_type,
                                    const Context &context) {
  std::string message = "type is not convertible to ";
  append_type_name(message, target_type);
  message += " from ";
  append_type_name(message, source_type);

  if (auto *active_type = context.active_type()) {
    message += " (required by ";
    append_type_name(message, *active_type);
    message += ")";
  }

  return type_not_convertible_exception(std::move(message));
}

template <typename Type>
type_recursion_exception make_type_recursion_exception() {
  std::string message = "recursive dependency detected while constructing: ";
  append_type_name(message, describe_type<Type>());
  return type_recursion_exception(std::move(message));
}

template <typename Type, typename Context>
type_recursion_exception make_type_recursion_exception(const Context &context) {
  std::string message = "recursive dependency detected: ";
  if (context.has_type_path()) {
    std::string resolution_path;
    context.append_type_path(resolution_path);
    message += resolution_path;
  }
  if (!context.has_type_path()) {
    append_type_name(message, describe_type<Type>());
  }

  return type_recursion_exception(std::move(message));
}

template <typename Selector>
void append_lookup_already_registered_key_text(std::string &message,
                                               const lookup_key<Selector> &) {
  if constexpr (std::is_same_v<Selector, key_type<none_t>>) {
    append_text(message, ", key type ", describe_type<none_t>());
  } else {
    append_text(message, ", key type ", describe_type<Selector>());
  }
}

template <typename Interface, typename Storage, typename LookupKey>
lookup_already_registered_exception
make_lookup_already_registered_exception(const LookupKey &key) {
  std::string message = "lookup already registered: interface ";
  append_text(message, describe_type<Interface>(), ", storage ",
              describe_type<Storage>());
  append_lookup_already_registered_key_text(message, key);
  return lookup_already_registered_exception(std::move(message));
}

template <typename Interface, typename Storage, typename LookupKey,
          typename BackendKey>
lookup_already_registered_exception
make_lookup_already_registered_exception(const LookupKey &key,
                                         const BackendKey &) {
  std::string message = "lookup already registered: interface ";
  append_text(message, describe_type<Interface>(), ", storage ",
              describe_type<Storage>());
  if constexpr (std::is_same_v<std::decay_t<BackendKey>, none_t>) {
    append_lookup_already_registered_key_text(message, key);
  } else {
    append_text(message, ", key type ",
                describe_type<std::decay_t<BackendKey>>());
  }
  return lookup_already_registered_exception(std::move(message));
}

template <typename Key>
type_index_out_of_range_exception
make_type_index_out_of_range_exception(Key, size_t) {
  std::string message = "type index out of range: key type ";
  append_text(message, describe_type<Key>());
  return type_index_out_of_range_exception(std::move(message));
}

} // namespace detail
} // namespace dingo
