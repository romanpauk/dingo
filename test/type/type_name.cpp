//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#include <dingo/type/type_name.h>

#include <gtest/gtest.h>

#include <memory>
#include <optional>
#include <string_view>

namespace dingo {
namespace type_name_test_types {
struct value {};
} // namespace type_name_test_types

namespace {

constexpr std::string_view expected_type_name_value() {
#ifdef _MSC_VER
    return "struct dingo::type_name_test_types::value";
#else
    return "dingo::type_name_test_types::value";
#endif
}

TEST(type_name_test, value_type_name) {
    ASSERT_EQ(raw_type_name<type_name_test_types::value>(),
              expected_type_name_value());
}

TEST(type_name_test, const_reference_type_name) {
#ifdef _MSC_VER
    constexpr std::string_view expected =
        "const struct dingo::type_name_test_types::value&";
#else
    constexpr std::string_view expected =
        "const dingo::type_name_test_types::value&";
#endif

    ASSERT_EQ(type_name<const type_name_test_types::value&>(), expected);
}

TEST(type_name_test, pointer_type_name) {
#ifdef _MSC_VER
    constexpr std::string_view expected =
        "struct dingo::type_name_test_types::value*";
#else
    constexpr std::string_view expected =
        "dingo::type_name_test_types::value*";
#endif

    ASSERT_EQ(type_name<type_name_test_types::value*>(), expected);
}

TEST(type_name_test, double_pointer_type_name) {
#ifdef _MSC_VER
    constexpr std::string_view expected =
        "struct dingo::type_name_test_types::value**";
#else
    constexpr std::string_view expected =
        "dingo::type_name_test_types::value**";
#endif

    ASSERT_EQ(type_name<type_name_test_types::value**>(), expected);
}

TEST(type_name_test, pointer_reference_descriptor) {
    constexpr auto descriptor =
        describe_type<const type_name_test_types::value* const&>();

    ASSERT_EQ(descriptor.reference, type_reference_kind::lvalue);
    ASSERT_EQ(descriptor.cv, type_cv_flags::is_const);
    ASSERT_NE(descriptor.pointee, nullptr);

    const auto pointee = descriptor.pointee();
    ASSERT_EQ(pointee.reference, type_reference_kind::none);
    ASSERT_EQ(pointee.cv, type_cv_flags::is_const);
    ASSERT_EQ(pointee.raw_name, expected_type_name_value());
    ASSERT_EQ(pointee.pointee, nullptr);
}

TEST(type_name_test, nested_pointer_descriptor) {
    constexpr auto descriptor =
        describe_type<const type_name_test_types::value* const* volatile>();

    ASSERT_EQ(descriptor.reference, type_reference_kind::none);
    ASSERT_EQ(descriptor.cv, type_cv_flags::is_volatile);
    ASSERT_NE(descriptor.pointee, nullptr);

    const auto first = descriptor.pointee();
    ASSERT_EQ(first.reference, type_reference_kind::none);
    ASSERT_EQ(first.cv, type_cv_flags::is_const);
    ASSERT_NE(first.pointee, nullptr);

    const auto second = first.pointee();
    ASSERT_EQ(second.reference, type_reference_kind::none);
    ASSERT_EQ(second.cv, type_cv_flags::is_const);
    ASSERT_EQ(second.raw_name, expected_type_name_value());
    ASSERT_EQ(second.pointee, nullptr);
}

TEST(type_name_test, shared_ptr_type_name_contains_wrapper_and_pointee) {
    const auto name = type_name<std::shared_ptr<type_name_test_types::value>>();
    ASSERT_NE(name.find("shared_ptr"), std::string_view::npos);
    ASSERT_NE(name.find(expected_type_name_value()), std::string_view::npos);
}

TEST(type_name_test, optional_type_name_contains_wrapper_and_pointee) {
    const auto name = type_name<std::optional<type_name_test_types::value>>();
    ASSERT_NE(name.find("optional"), std::string_view::npos);
    ASSERT_NE(name.find(expected_type_name_value()), std::string_view::npos);
}

static_assert(detail::type_name_find("abcdef", "cd") == 2);
static_assert(detail::type_name_find("abcdef", "gh") ==
              detail::type_name_not_found);
static_assert(!raw_type_name<type_name_test_types::value>().empty());

} // namespace
} // namespace dingo
