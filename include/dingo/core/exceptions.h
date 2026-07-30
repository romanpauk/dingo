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

// Keep message formatting independent of request types so error paths do not
// multiply the same string-building code across every resolution instantiation.
struct resolution_path {
  bool present = false;
  std::string text;
};

template <typename Context>
resolution_path get_resolution_path(const Context &context) {
  resolution_path path;
  path.present = context.has_type_path();
  if (path.present) {
    context.append_type_path(path.text);
  }
  return path;
}

inline void append_resolution_path(std::string &message,
                                   const resolution_path &path) {
  if (!path.present) {
    return;
  }

  message += " (required by ";
  message += path.text;
  message += ")";
}

DINGO_NOINLINE inline type_not_found_exception
make_type_not_found_exception(type_descriptor request_type,
                              const type_descriptor *index_type,
                              const resolution_path &path) {
  std::string message = "type not found: ";
  append_type_name(message, request_type);
  if (index_type != nullptr) {
    message += " (index type: ";
    append_type_name(message, *index_type);
    message += ")";
  }
  append_resolution_path(message, path);
  return type_not_found_exception(std::move(message));
}

template <typename Request>
type_not_found_exception make_type_not_found_exception() {
  return make_type_not_found_exception(describe_type<Request>(), nullptr, {});
}

template <typename Request, typename Context>
type_not_found_exception make_type_not_found_exception(const Context &context) {
  const auto path = get_resolution_path(context);
  return make_type_not_found_exception(describe_type<Request>(), nullptr, path);
}

template <typename Request, typename IdType>
type_not_found_exception make_type_not_found_exception() {
  const auto index_type = describe_type<IdType>();
  return make_type_not_found_exception(describe_type<Request>(), &index_type,
                                       {});
}

template <typename Request, typename IdType, typename Context>
type_not_found_exception make_type_not_found_exception(const Context &context) {
  const auto index_type = describe_type<IdType>();
  const auto path = get_resolution_path(context);
  return make_type_not_found_exception(describe_type<Request>(), &index_type,
                                       path);
}

template <typename Request, typename Context, typename Selector>
type_not_found_exception
make_type_not_found_exception(const Context &context,
                              const lookup_key<Selector> &) {
  const auto path = get_resolution_path(context);
  if constexpr (std::is_same_v<Selector, key_type<none_t>>) {
    return make_type_not_found_exception(describe_type<Request>(), nullptr,
                                         path);
  } else {
    const auto index_type = describe_type<Selector>();
    return make_type_not_found_exception(describe_type<Request>(), &index_type,
                                         path);
  }
}

DINGO_NOINLINE inline type_not_found_exception
make_collection_type_not_found_exception(type_descriptor collection_type,
                                         type_descriptor resolve_type) {
  std::string message = "type not found for collection ";
  append_type_name(message, collection_type);
  message += " (element type: ";
  append_type_name(message, resolve_type);
  message += ")";
  return type_not_found_exception(std::move(message));
}

template <typename Collection, typename ResolveType>
type_not_found_exception make_collection_type_not_found_exception() {
  return make_collection_type_not_found_exception(describe_type<Collection>(),
                                                  describe_type<ResolveType>());
}

DINGO_NOINLINE inline type_ambiguous_exception
make_type_ambiguous_exception(type_descriptor request_type,
                              const type_descriptor *active_type) {
  std::string message = "type resolution is ambiguous: ";
  append_type_name(message, request_type);
  if (active_type != nullptr) {
    message += " (required by ";
    append_type_name(message, *active_type);
    message += ")";
  }
  return type_ambiguous_exception(std::move(message));
}

template <typename Request>
type_ambiguous_exception make_type_ambiguous_exception() {
  return make_type_ambiguous_exception(describe_type<Request>(), nullptr);
}

template <typename Request, typename Context>
type_ambiguous_exception make_type_ambiguous_exception(const Context &context) {
  return make_type_ambiguous_exception(describe_type<Request>(),
                                       context.active_type());
}

DINGO_NOINLINE inline type_not_convertible_exception
make_type_not_convertible_exception(type_descriptor target_type,
                                    type_descriptor source_type,
                                    const type_descriptor *active_type) {
  std::string message = "type is not convertible to ";
  append_type_name(message, target_type);
  message += " from ";
  append_type_name(message, source_type);

  if (active_type != nullptr) {
    message += " (required by ";
    append_type_name(message, *active_type);
    message += ")";
  }

  return type_not_convertible_exception(std::move(message));
}

inline type_not_convertible_exception
make_type_not_convertible_exception(type_descriptor target_type,
                                    type_descriptor source_type) {
  return make_type_not_convertible_exception(target_type, source_type, nullptr);
}

template <typename Context>
inline type_not_convertible_exception
make_type_not_convertible_exception(type_descriptor target_type,
                                    type_descriptor source_type,
                                    const Context &context) {
  return make_type_not_convertible_exception(target_type, source_type,
                                             context.active_type());
}

DINGO_NOINLINE inline type_recursion_exception
make_type_recursion_exception(type_descriptor type) {
  std::string message = "recursive dependency detected while constructing: ";
  append_type_name(message, type);
  return type_recursion_exception(std::move(message));
}

DINGO_NOINLINE inline type_recursion_exception
make_type_recursion_exception(type_descriptor type,
                              const resolution_path &path) {
  std::string message = "recursive dependency detected: ";
  if (path.present) {
    message += path.text;
  } else {
    append_type_name(message, type);
  }

  return type_recursion_exception(std::move(message));
}

template <typename Type>
type_recursion_exception make_type_recursion_exception() {
  return make_type_recursion_exception(describe_type<Type>());
}

template <typename Type, typename Context>
type_recursion_exception make_type_recursion_exception(const Context &context) {
  const auto path = get_resolution_path(context);
  return make_type_recursion_exception(describe_type<Type>(), path);
}

DINGO_NOINLINE inline lookup_already_registered_exception
make_lookup_already_registered_exception(type_descriptor interface_type,
                                         type_descriptor storage_type,
                                         type_descriptor key_type) {
  std::string message = "lookup already registered: interface ";
  append_type_name(message, interface_type);
  message += ", storage ";
  append_type_name(message, storage_type);
  message += ", key type ";
  append_type_name(message, key_type);
  return lookup_already_registered_exception(std::move(message));
}

template <typename Interface, typename Storage, typename Selector>
lookup_already_registered_exception
make_lookup_already_registered_exception(const lookup_key<Selector> &) {
  if constexpr (std::is_same_v<Selector, dingo::key_type<none_t>>) {
    return make_lookup_already_registered_exception(describe_type<Interface>(),
                                                    describe_type<Storage>(),
                                                    describe_type<none_t>());
  } else {
    return make_lookup_already_registered_exception(describe_type<Interface>(),
                                                    describe_type<Storage>(),
                                                    describe_type<Selector>());
  }
}

template <typename Interface, typename Storage, typename Selector,
          typename BackendKey>
lookup_already_registered_exception
make_lookup_already_registered_exception(const lookup_key<Selector> &key,
                                         const BackendKey &) {
  if constexpr (std::is_same_v<std::decay_t<BackendKey>, none_t>) {
    return make_lookup_already_registered_exception<Interface, Storage>(key);
  } else {
    return make_lookup_already_registered_exception(
        describe_type<Interface>(), describe_type<Storage>(),
        describe_type<std::decay_t<BackendKey>>());
  }
}

DINGO_NOINLINE inline type_index_out_of_range_exception
make_type_index_out_of_range_exception(type_descriptor key_type) {
  std::string message = "type index out of range: key type ";
  append_type_name(message, key_type);
  return type_index_out_of_range_exception(std::move(message));
}

template <typename Key>
type_index_out_of_range_exception
make_type_index_out_of_range_exception(Key, size_t) {
  return make_type_index_out_of_range_exception(describe_type<Key>());
}

} // namespace detail
} // namespace dingo
