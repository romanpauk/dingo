//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/config.h>

#include <dingo/exceptions.h>
#include <dingo/factory/constructor_detection.h>
#include <dingo/type/type_list.h>

#include <string>

namespace dingo {
namespace detail {

template <typename Tag> constexpr const char* constructor_detection_tag_name() {
    return "unknown";
}

template <> constexpr const char* constructor_detection_tag_name<reference>() {
    return "reference";
}

template <>
constexpr const char* constructor_detection_tag_name<reference_exact>() {
    return "reference_exact";
}

template <> constexpr const char* constructor_detection_tag_name<value>() {
    return "value";
}

template <> constexpr const char* constructor_detection_tag_name<automatic>() {
    return "automatic";
}

template <typename Dependency> struct constructor_dependency_formatter {
    static void append(std::string& message) {
        append_type_name(message, describe_type<Dependency>());
    }
};

template <typename ConstructedType, typename DetectionTag, std::size_t Position>
struct constructor_dependency_formatter<
    ir::detected_constructor_dependency<ConstructedType, DetectionTag,
                                        Position>> {
    static void append(std::string& message) {
        append_text(message, constructor_detection_tag_name<DetectionTag>(),
                    " slot ",
                    std::to_string(static_cast<unsigned long long>(Position)));
    }
};

template <typename Dependency>
void append_constructor_dependency(std::string& message,
                                   type_list_iterator<Dependency>) {
    constructor_dependency_formatter<Dependency>::append(message);
}

template <typename Dependencies>
void append_constructor_dependencies(std::string& message) {
    bool first = true;
    for_each(Dependencies{}, [&](auto dependency) {
        if (!first) {
            message += ", ";
        }
        append_constructor_dependency(message, dependency);
        first = false;
    });
}

template <typename ConstructedType, typename Dependencies>
void append_constructor_invocation(
    std::string& message,
    ir::constructor_invocation<ConstructedType, Dependencies>) {
    append_type_name(message, describe_type<ConstructedType>());
    message += "(";
    append_constructor_dependencies<Dependencies>(message);
    message += ")";
}

template <typename ConstructedType, typename DetectionTag, typename Dependencies>
void append_constructor_invocation(
    std::string& message,
    ir::detected_constructor_invocation<ConstructedType, DetectionTag,
                                        Dependencies>) {
    append_type_name(message, describe_type<ConstructedType>());
    message += "(";
    append_constructor_dependencies<Dependencies>(message);
    message += ")";
}

template <typename Invocation>
void append_constructor_invocation(std::string& message) {
    append_constructor_invocation(message, Invocation{});
}

} // namespace detail
} // namespace dingo
