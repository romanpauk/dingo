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
struct override_source {
    int value;
};

struct override_target {
    int value;
};

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

using converter_variant =
    std::variant<converter_variant_a, converter_variant_b>;

struct borrowed_converter_variant {
    converter_variant value;
};

struct converter_factory {
    converter_variant& value;

    template <typename Context> auto& resolve(Context&) { return value; }
};

struct converter_pointer_factory {
    converter_variant* value;

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

template <>
struct type_conversion<override_target,
                       detail::lvalue_source<override_source>> {
    template <typename Factory, typename Context, typename SourceCapability>
    static override_target apply(Factory&, Context&, SourceCapability&& source,
                                 type_descriptor, type_descriptor) {
        return {detail::materialized_reference(source).value + 1};
    }
};

template <> struct type_traits<borrowed_converter_variant> {
    static constexpr bool enabled = true;
    static constexpr bool is_pointer_like = false;
    static constexpr bool is_value_borrowable = true;

    using value_type = converter_variant;

    template <typename> using rebind_t = borrowed_converter_variant;

    template <typename> static constexpr bool is_handle_rebindable = false;

    template <typename> static constexpr bool is_rebindable = false;

    static converter_variant& borrow(borrowed_converter_variant& value) {
        return value.value;
    }
};

namespace {
TEST(type_conversion_test, user_defined_type_conversion_specialization_wins) {
    override_source value{41};
    converter_context context;
    converter_context factory;

    auto converted = type_conversion<override_target,
                                     detail::lvalue_source<override_source>>::
        apply(factory, context, detail::make_lvalue_source(value),
              describe_type<override_target>(),
              describe_type<override_source>());

    ASSERT_EQ(converted.value, 42);
}

TEST(type_conversion_test, alternative_exact_match_returns_materialized_value) {
    converter_variant value(std::in_place_type<converter_variant_a>, 7);
    converter_context context;
    converter_context factory;

    auto& reference =
        type_conversion<converter_variant,
                        detail::lvalue_source<converter_variant>>::
            apply(factory, context, detail::make_lvalue_source(value),
                  describe_type<converter_variant&>(),
                  describe_type<converter_variant>());
    auto* pointer = type_conversion<converter_variant*,
                                    detail::lvalue_source<converter_variant>>::
        apply(factory, context, detail::make_lvalue_source(value),
              describe_type<converter_variant*>(),
              describe_type<converter_variant>());

    ASSERT_EQ(std::addressof(reference), std::addressof(value));
    ASSERT_EQ(pointer, std::addressof(value));
}

TEST(type_conversion_test,
     alternative_unique_match_extracts_value_reference_and_pointer) {
    converter_variant lvalue(std::in_place_type<converter_variant_a>, 7);
    converter_context context;
    converter_context factory;

    auto& reference =
        type_conversion<converter_variant_a,
                        detail::lvalue_source<converter_variant>>::
            apply(factory, context, detail::make_lvalue_source(lvalue),
                  describe_type<converter_variant_a&>(),
                  describe_type<converter_variant>());
    auto* pointer = type_conversion<converter_variant_a*,
                                    detail::lvalue_source<converter_variant>>::
        apply(factory, context, detail::make_lvalue_source(lvalue),
              describe_type<converter_variant_a*>(),
              describe_type<converter_variant>());
    auto rvalue = type_conversion<converter_variant_a,
                                  detail::rvalue_source<converter_variant>>::
        apply(factory, context,
              detail::make_rvalue_source(converter_variant(
                  std::in_place_type<converter_variant_a>, 9)),
              describe_type<converter_variant_a>(),
              describe_type<converter_variant>());

    ASSERT_EQ(reference.value, 7);
    ASSERT_EQ(pointer, std::addressof(reference));
    ASSERT_EQ(rvalue.value, 9);
}

TEST(type_conversion_test,
     alternative_missing_match_rejects_value_reference_and_pointer) {
    converter_variant value(std::in_place_type<converter_variant_a>, 7);
    converter_context context;
    converter_context factory;

    ASSERT_THROW((type_conversion<converter_variant_unrelated,
                                  detail::rvalue_source<converter_variant>>::
                      apply(factory, context,
                            detail::make_rvalue_source(converter_variant(
                                std::in_place_type<converter_variant_a>, 7)),
                            describe_type<converter_variant_unrelated>(),
                            describe_type<converter_variant>())),
                 type_not_convertible_exception);
    ASSERT_THROW((type_conversion<converter_variant_unrelated,
                                  detail::lvalue_source<converter_variant>>::
                      apply(factory, context, detail::make_lvalue_source(value),
                            describe_type<converter_variant_unrelated&>(),
                            describe_type<converter_variant>())),
                 type_not_convertible_exception);
    ASSERT_THROW((type_conversion<converter_variant_unrelated*,
                                  detail::lvalue_source<converter_variant>>::
                      apply(factory, context, detail::make_lvalue_source(value),
                            describe_type<converter_variant_unrelated*>(),
                            describe_type<converter_variant>())),
                 type_not_convertible_exception);
}

TEST(type_conversion_test,
     borrowed_wrapper_converts_value_reference_and_pointer) {
    borrowed_converter_variant value{
        converter_variant(std::in_place_type<converter_variant_a>, 7)};
    converter_context context;
    converter_context factory;

    auto& exact =
        type_conversion<converter_variant,
                        detail::lvalue_source<borrowed_converter_variant>>::
            apply(factory, context, detail::make_lvalue_source(value),
                  describe_type<converter_variant&>(),
                  describe_type<borrowed_converter_variant>());
    auto& alternative =
        type_conversion<converter_variant_a,
                        detail::lvalue_source<borrowed_converter_variant>>::
            apply(factory, context, detail::make_lvalue_source(value),
                  describe_type<converter_variant_a&>(),
                  describe_type<borrowed_converter_variant>());
    auto* alternative_pointer =
        type_conversion<converter_variant_a*,
                        detail::lvalue_source<borrowed_converter_variant>>::
            apply(factory, context, detail::make_lvalue_source(value),
                  describe_type<converter_variant_a*>(),
                  describe_type<borrowed_converter_variant>());

    ASSERT_EQ(std::addressof(exact), std::addressof(value.value));
    ASSERT_EQ(alternative.value, 7);
    ASSERT_EQ(alternative_pointer, std::addressof(alternative));
}

TEST(type_conversion_test,
     alternative_projectors_reject_unrelated_targets_with_runtime_exception) {
    converter_variant value(std::in_place_type<converter_variant_a>, 7);
    converter_factory lvalue_factory{value};
    converter_pointer_factory pointer_factory{&value};
    converter_context context;

    ASSERT_THROW(
        (type_conversion<converter_variant_unrelated,
                         detail::lvalue_source<converter_variant>>::
             apply(lvalue_factory, context, detail::make_lvalue_source(value),
                   describe_type<converter_variant_unrelated&>(),
                   describe_type<converter_variant>())),
        type_not_convertible_exception);

    ASSERT_THROW((type_conversion<converter_variant_unrelated*,
                                  detail::pointer_source<converter_variant>>::
                      apply(pointer_factory, context,
                            detail::make_pointer_source(&value),
                            describe_type<converter_variant_unrelated*>(),
                            describe_type<converter_variant*>())),
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
} // namespace
} // namespace dingo
