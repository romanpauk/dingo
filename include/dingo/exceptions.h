//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/config.h>
#include <dingo/type_descriptor.h>

#include <cassert>
#include <exception>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace dingo {
struct exception : std::exception {
    explicit exception(std::string message) : message_(std::move(message)) {
        assert(!message_.empty());
    }

    const char* what() const noexcept override {
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

struct type_not_constructed_exception : exception {
    using exception::exception;
};

struct type_already_registered_exception : exception {
    using exception::exception;
};

struct type_index_already_registered_exception : exception {
    using exception::exception;
};

struct type_index_out_of_range_exception : exception {
    using exception::exception;
};

struct type_context_overflow_exception : exception {
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

inline void append_text_part(std::string& message, type_descriptor descriptor) {
    append_type_name(message, descriptor);
}

template <typename Text>
void append_text_part(std::string& message, Text&& text) {
    message += std::forward<Text>(text);
}

inline void append_text(std::string&) {}

template <typename First, typename... Rest>
void append_text(std::string& message, First&& first, Rest&&... rest) {
    append_text_part(message, std::forward<First>(first));
    append_text(message, std::forward<Rest>(rest)...);
}

template <typename Key, typename = void> struct index_value_type {
    using type = Key;
};

template <typename Key>
struct index_value_type<Key, std::enable_if_t<std::is_enum_v<Key>>> {
    using type = std::underlying_type_t<Key>;
};

template <typename Key>
using index_value_type_t = typename index_value_type<Key>::type;

template <typename Key>
std::string index_value_to_string(Key key) {
    using value_type = index_value_type_t<Key>;
    if constexpr (std::is_signed_v<value_type>) {
        return std::to_string(static_cast<long long>(key));
    } else {
        return std::to_string(static_cast<unsigned long long>(key));
    }
}

template <typename Request>
type_not_found_exception make_type_not_found_exception() {
    std::string message = "type not found: ";
    append_type_name(message, describe_type<Request>());
    return type_not_found_exception(std::move(message));
}

template <typename Request, typename IdType>
type_not_found_exception make_type_not_found_exception() {
    std::string message = "type not found: ";
    append_text(message, describe_type<Request>(), " (index type: ",
                describe_type<IdType>(), ")");
    return type_not_found_exception(std::move(message));
}

template <typename Collection, typename ResolveType>
type_not_found_exception make_collection_type_not_found_exception() {
    std::string message = "type not found for collection ";
    append_text(message, describe_type<Collection>(), " (element type: ",
                describe_type<ResolveType>(), ")");
    return type_not_found_exception(std::move(message));
}

template <typename Request>
type_ambiguous_exception make_type_ambiguous_exception() {
    std::string message = "type resolution is ambiguous: ";
    append_type_name(message, describe_type<Request>());
    return type_ambiguous_exception(std::move(message));
}

inline type_not_convertible_exception make_type_not_convertible_exception(
    type_descriptor target_type, type_descriptor source_type) {
    std::string message = "type is not convertible to ";
    append_type_name(message, target_type);
    message += " from ";
    append_type_name(message, source_type);

    return type_not_convertible_exception(std::move(message));
}

template <typename Type> type_recursion_exception make_type_recursion_exception() {
    std::string message = "recursive dependency detected while constructing: ";
    append_type_name(message, describe_type<Type>());
    return type_recursion_exception(std::move(message));
}

template <typename Type, typename Context>
type_recursion_exception make_type_recursion_exception(const Context& context) {
    std::string message = "recursive dependency detected: ";
    context.append_type_path(message);
    if (!context.has_type_path()) {
        append_type_name(message, describe_type<Type>());
    }
    return type_recursion_exception(std::move(message));
}

template <typename Interface, typename Storage>
type_already_registered_exception make_type_already_registered_exception() {
    std::string message = "type already registered: interface ";
    append_text(message, describe_type<Interface>(), ", storage ",
                describe_type<Storage>());
    return type_already_registered_exception(std::move(message));
}

template <typename Interface, typename Storage, typename IdType>
type_index_already_registered_exception
make_type_index_already_registered_exception() {
    std::string message = "type index already registered: interface ";
    append_text(message, describe_type<Interface>(), ", storage ",
                describe_type<Storage>(), ", index type ",
                describe_type<IdType>());
    return type_index_already_registered_exception(std::move(message));
}

template <typename Key>
type_index_out_of_range_exception make_type_index_out_of_range_exception(
    Key key, size_t bound) {
    std::string message = "type index out of range: key type ";
    append_text(message, describe_type<Key>(), ", value ",
                index_value_to_string(key), ", bound ",
                std::to_string(static_cast<unsigned long long>(bound)));
    return type_index_out_of_range_exception(std::move(message));
}

} // namespace detail
} // namespace dingo
