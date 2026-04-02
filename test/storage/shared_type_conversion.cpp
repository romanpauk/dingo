//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#include <dingo/exceptions.h>
#include <dingo/resolution/type_conversion.h>

#include <gtest/gtest.h>

#include <variant>

namespace dingo {
namespace {
struct converter_variant_a {
    explicit converter_variant_a(int init) : value(init) {}
    int value;
};

struct converter_variant_b {
    explicit converter_variant_b(float init) : value(init) {}
    float value;
};

struct converter_variant_unrelated {
    int value;
};

struct converter_factory {
    std::variant<converter_variant_a, converter_variant_b>& value;

    template <typename Context> auto& resolve(Context&) { return value; }
};

struct converter_pointer_factory {
    std::variant<converter_variant_a, converter_variant_b>* value;

    template <typename Context> auto* resolve(Context&) { return value; }
};

struct converter_context {};
} // namespace

TEST(type_conversion_test,
     alternative_projectors_reject_unrelated_targets_with_runtime_exception) {
    std::variant<converter_variant_a, converter_variant_b> value(
        std::in_place_type<converter_variant_a>, 7);
    converter_factory lvalue_factory{value};
    converter_pointer_factory pointer_factory{&value};
    converter_context context;

    ASSERT_THROW(
        (type_conversion<converter_variant_unrelated,
                         detail::lvalue_source<
                             std::variant<converter_variant_a,
                                          converter_variant_b>>>::apply(
            lvalue_factory, context, detail::make_lvalue_source(value),
            describe_type<converter_variant_unrelated&>(),
            describe_type<
                std::variant<converter_variant_a, converter_variant_b>>())),
        type_not_convertible_exception);

    ASSERT_THROW(
        (type_conversion<converter_variant_unrelated*,
                         detail::pointer_source<
                             std::variant<converter_variant_a,
                                          converter_variant_b>>>::apply(
            pointer_factory, context, detail::make_pointer_source(&value),
            describe_type<converter_variant_unrelated*>(),
            describe_type<
                std::variant<converter_variant_a, converter_variant_b>*>())),
        type_not_convertible_exception);
}
} // namespace dingo
