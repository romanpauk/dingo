//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#include <dingo/core/exceptions.h>
#include <dingo/resolution/type_conversion.h>
#include <dingo/storage/shared.h>

#include <gtest/gtest.h>

#include <variant>
#include <vector>

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

struct destructor_order_value {
    destructor_order_value(int init, std::vector<int>& target_order)
        : value(init), order(&target_order) {}
    ~destructor_order_value() { order->push_back(value); }

    int value;
    std::vector<int>* order;
};

struct destructor_order_array_factory {
    std::vector<int>* order;

    template <typename Type, typename Context, typename Container>
    void construct(void* ptr, Context&, Container&) {
        auto* values = reinterpret_cast<Type*>(ptr);
        for (int i = 0; i != 3; ++i) {
            new (&(*values)[i]) destructor_order_value(i, *order);
        }
    }
};
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

TEST(type_conversion_test, rvalue_source_destroys_materialized_value) {
    std::vector<int> order;

    {
        auto source = detail::make_rvalue_source<destructor_order_value>(
            std::in_place,
            [&](void* ptr) { new (ptr) destructor_order_value(0, order); });
        (void)source;
    }

    ASSERT_EQ(order, (std::vector<int>{0}));
}

TEST(type_conversion_test,
     shared_fixed_array_storage_reset_destroys_elements_in_reverse_order) {
    std::vector<int> order;
    using array_type = destructor_order_value[3];
    using storage_type =
        detail::storage<shared, array_type, array_type,
                        destructor_order_array_factory,
                        detail::conversions<shared, array_type, array_type>>;
    converter_context context;
    converter_context container;

    storage_type storage(destructor_order_array_factory{&order});
    storage.resolve(context, container);
    storage.reset();

    ASSERT_EQ(order, (std::vector<int>{2, 1, 0}));
}
} // namespace dingo
