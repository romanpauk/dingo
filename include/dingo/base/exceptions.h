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
#include <cstddef>
#include <exception>
#include <string_view>
#include <type_traits>
#include <utility>

namespace dingo {
template <typename T, auto... Values> struct key_type;

namespace detail {
// The text of the message of an exception, made as its parts are appended.
class message_text {
public:
  message_text() = default;
  message_text(std::string_view text) { *this += text; }
  message_text(const char *text) : message_text(std::string_view(text)) {}
  message_text(const message_text &other) { *this += other.view(); }
  message_text(message_text &&other) noexcept
      : data_(other.data_), size_(other.size_), capacity_(other.capacity_) {
    other.data_ = nullptr;
    other.size_ = 0;
    other.capacity_ = 0;
  }
  message_text &operator=(message_text other) noexcept {
    std::swap(data_, other.data_);
    std::swap(size_, other.size_);
    std::swap(capacity_, other.capacity_);
    return *this;
  }
  ~message_text() { delete[] data_; }

  message_text &operator+=(std::string_view text) {
    if (size_ + text.size() + 1 > capacity_) {
      grow(size_ + text.size() + 1);
    }
    for (char each : text) {
      data_[size_++] = each;
    }
    data_[size_] = '\0';
    return *this;
  }

  const char *c_str() const noexcept { return data_ != nullptr ? data_ : ""; }
  std::string_view view() const noexcept { return {c_str(), size_}; }
  bool empty() const noexcept { return size_ == 0; }

private:
  void grow(std::size_t needed) {
    std::size_t capacity = capacity_ * 2 > needed ? capacity_ * 2 : needed;
    if (capacity < 64) {
      capacity = 64;
    }
    char *data = new char[capacity];
    for (std::size_t index = 0; index < size_; ++index) {
      data[index] = data_[index];
    }
    delete[] data_;
    data_ = data;
    capacity_ = capacity;
  }

  char *data_ = nullptr;
  std::size_t size_ = 0;
  std::size_t capacity_ = 0;
};
} // namespace detail

struct exception : std::exception {
  explicit exception(detail::message_text message)
      : message_(std::move(message)) {
    assert(!message_.empty());
  }

  const char *what() const noexcept override {
    assert(!message_.empty());
    return message_.c_str();
  }

protected:
  detail::message_text message_;
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

struct lookup_key_out_of_range_exception : exception {
  using exception::exception;
};

#ifdef _DEBUG
#endif

namespace detail {
template <typename T> struct lookup_key;

inline void append_text_part(message_text &message,
                             type_descriptor descriptor) {
  append_type_name(message, descriptor);
}

template <typename Text>
void append_text_part(message_text &message, Text &&text) {
  message += std::forward<Text>(text);
}

template <typename... Parts>
void append_text(message_text &message, Parts &&...parts) {
  (append_text_part(message, std::forward<Parts>(parts)), ...);
}

// The resolution path that led to the failing request.
template <typename Context>
void append_path(message_text &message, const Context &context) {
  if (context.has_type_path()) {
    message += " (required by ";
    context.append_type_path(message);
    message += ")";
  }
}

// The type whose construction made the failing request.
template <typename Context>
void append_active_type(message_text &message, const Context &context) {
  if (auto *active_type = context.active_type()) {
    append_text(message, " (required by ", *active_type, ")");
  }
}

// The messages name types by their descriptors, so that a message is made by
// one function for every type it names, rather than by one per type.

// The index type of a lookup key, or null for a key without one.
template <typename Selector>
constexpr const type_descriptor *key_index(const lookup_key<Selector> &) {
  if constexpr (std::is_same_v<Selector, key_type<none_t>>) {
    return nullptr;
  } else {
    return &type_descriptor_v<Selector>;
  }
}

template <typename Context>
type_not_found_exception
make_type_not_found_exception(type_descriptor request, const Context &context,
                              const type_descriptor *index_type) {
  message_text message = "type not found: ";
  append_type_name(message, request);
  if (index_type != nullptr) {
    append_text(message, " (index type: ", *index_type, ")");
  }
  append_path(message, context);
  return type_not_found_exception(std::move(message));
}

template <typename Collection, typename ResolveType>
type_not_found_exception make_collection_type_not_found_exception() {
  message_text message = "type not found for collection ";
  append_text(message, detail::type_descriptor_v<Collection>,
              " (element type: ", detail::type_descriptor_v<ResolveType>, ")");
  return type_not_found_exception(std::move(message));
}

template <typename Context>
type_ambiguous_exception make_type_ambiguous_exception(type_descriptor request,
                                                       const Context &context) {
  message_text message = "type resolution is ambiguous: ";
  append_type_name(message, request);
  append_active_type(message, context);
  return type_ambiguous_exception(std::move(message));
}

inline type_not_convertible_exception
make_type_not_convertible_exception(type_descriptor target_type,
                                    type_descriptor source_type) {
  message_text message = "type is not convertible to ";
  append_text(message, target_type, " from ", source_type);
  return type_not_convertible_exception(std::move(message));
}

template <typename Context>
inline type_not_convertible_exception
make_type_not_convertible_exception(type_descriptor target_type,
                                    type_descriptor source_type,
                                    const Context &context) {
  message_text message = "type is not convertible to ";
  append_text(message, target_type, " from ", source_type);
  append_active_type(message, context);
  return type_not_convertible_exception(std::move(message));
}

template <typename Context>
type_recursion_exception make_type_recursion_exception(type_descriptor type,
                                                       const Context &context) {
  message_text message = "recursive dependency detected: ";
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
constexpr const type_descriptor &
registration_key_type(const lookup_key<Selector> &) {
  if constexpr (!std::is_same_v<std::decay_t<BackendKey>, none_t>) {
    return detail::type_descriptor_v<std::decay_t<BackendKey>>;
  } else if constexpr (std::is_same_v<Selector, key_type<none_t>>) {
    return detail::type_descriptor_v<none_t>;
  } else {
    return detail::type_descriptor_v<Selector>;
  }
}

inline message_text registration_message(const char *prefix,
                                         type_descriptor interface_type,
                                         type_descriptor storage,
                                         type_descriptor key_type) {
  message_text message = prefix;
  append_text(message, interface_type, ", storage ", storage, ", key type ",
              key_type);
  return message;
}

inline lookup_already_registered_exception
make_lookup_already_registered_exception(type_descriptor interface_type,
                                         type_descriptor storage,
                                         type_descriptor key_type) {
  return lookup_already_registered_exception(
      registration_message("lookup already registered: interface ",
                           interface_type, storage, key_type));
}

inline container_sealed_exception
make_container_sealed_exception(type_descriptor interface_type,
                                type_descriptor storage,
                                type_descriptor key_type) {
  return container_sealed_exception(registration_message(
      "container sealed by resolve, lookup can not be replaced: interface ",
      interface_type, storage, key_type));
}

} // namespace detail
} // namespace dingo
