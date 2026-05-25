//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#include "storage/shared_common.h"

namespace dingo {

TYPED_TEST_SUITE(shared_variant_test, container_types, );

TYPED_TEST(shared_variant_test, variant_factory_selects_alternative) {
    using container_type = TypeParam;

    struct A {
        explicit A(int init) : value(init) {}
        int value;
    };
    struct B {
        explicit B(float init) : value(init) {}
        float value;
    };

    container_type container;
    container.template register_type<scope<unique>, storage<int>>();
    container.template register_type<scope<shared>, storage<std::variant<A, B>>,
                                     factory<constructor<A>>>();

    auto whole = container.template resolve<std::variant<A, B>>();
    ASSERT_TRUE(std::holds_alternative<A>(whole));
    EXPECT_EQ(std::get<A>(whole).value, 0);

    auto& value = container.template resolve<std::variant<A, B>&>();
    ASSERT_TRUE(std::holds_alternative<A>(value));
    EXPECT_EQ(std::get<A>(value).value, 0);

    auto copy = container.template resolve<A>();
    EXPECT_EQ(copy.value, 0);

    auto& selected = container.template resolve<A&>();
    EXPECT_EQ(selected.value, 0);
    EXPECT_EQ(&selected, &std::get<A>(value));

    auto& selected_const = container.template resolve<const A&>();
    EXPECT_EQ(selected_const.value, 0);
    EXPECT_EQ(&selected_const, &std::get<A>(value));

    auto* selected_ptr = container.template resolve<A*>();
    ASSERT_NE(selected_ptr, nullptr);
    EXPECT_EQ(selected_ptr, &std::get<A>(value));

    auto* selected_const_ptr = container.template resolve<const A*>();
    ASSERT_NE(selected_const_ptr, nullptr);
    EXPECT_EQ(selected_const_ptr, &std::get<A>(value));

    ASSERT_THROW(container.template resolve<B>(),
                 type_not_convertible_exception);
    ASSERT_THROW(container.template resolve<B&>(),
                 type_not_convertible_exception);
    ASSERT_THROW(container.template resolve<B*>(),
                 type_not_convertible_exception);
}

TYPED_TEST(shared_variant_test, variant_explicit_interfaces_override_defaults) {
    using container_type = TypeParam;

    struct A {
        explicit A(int init) : value(init) {}
        int value;
    };
    struct B {
        explicit B(float init) : value(init) {}
        float value;
    };

    container_type container;
    container.template register_type<scope<unique>, storage<int>>();
    container.template register_type<scope<shared>, storage<std::variant<A, B>>,
                                     factory<constructor<A>>, interfaces<A>>();

    auto copy = container.template resolve<A>();
    EXPECT_EQ(copy.value, 0);

    auto& selected = container.template resolve<A&>();
    EXPECT_EQ(selected.value, 0);

    auto* selected_ptr = container.template resolve<A*>();
    ASSERT_NE(selected_ptr, nullptr);
    EXPECT_EQ(selected_ptr->value, 0);

    ASSERT_THROW((container.template resolve<std::variant<A, B>>()),
                 type_not_found_exception);
    ASSERT_THROW((container.template resolve<std::variant<A, B>&>()),
                 type_not_found_exception);
    ASSERT_THROW((container.template resolve<std::variant<A, B>*>()),
                 type_not_found_exception);
    ASSERT_THROW(container.template resolve<B>(), type_not_found_exception);
}

TYPED_TEST(shared_variant_test,
           custom_alternative_type_factory_selects_alternative) {
    using container_type = TypeParam;

    container_type container;
    container.template register_type<scope<unique>, storage<int>>();
    container.template register_type<scope<shared>, storage<custom_sum>,
                                     factory<constructor<custom_sum_a>>>();

    auto whole = container.template resolve<custom_sum>();
    ASSERT_TRUE(std::holds_alternative<custom_sum_a>(whole.value));
    EXPECT_EQ(std::get<custom_sum_a>(whole.value).value, 0);

    auto& value = container.template resolve<custom_sum&>();
    ASSERT_TRUE(std::holds_alternative<custom_sum_a>(value.value));
    EXPECT_EQ(std::get<custom_sum_a>(value.value).value, 0);

    auto copy = container.template resolve<custom_sum_a>();
    EXPECT_EQ(copy.value, 0);

    auto& selected = container.template resolve<custom_sum_a&>();
    EXPECT_EQ(selected.value, 0);
    EXPECT_EQ(&selected, &std::get<custom_sum_a>(value.value));

    auto* selected_ptr = container.template resolve<custom_sum_a*>();
    ASSERT_NE(selected_ptr, nullptr);
    EXPECT_EQ(selected_ptr, &std::get<custom_sum_a>(value.value));

    ASSERT_THROW(container.template resolve<custom_sum_b>(),
                 type_not_convertible_exception);
    ASSERT_THROW(container.template resolve<custom_sum_b&>(),
                 type_not_convertible_exception);
    ASSERT_THROW(container.template resolve<custom_sum_b*>(),
                 type_not_convertible_exception);
}

TYPED_TEST(shared_variant_test,
           shared_ptr_variant_factory_selects_alternative) {
    using container_type = TypeParam;

    container_type container;
    container.template register_type<scope<unique>, storage<int>>();
    container.template register_type<
        scope<shared>, storage<std::shared_ptr<shared_ptr_variant>>,
        factory<function<&make_shared_ptr_variant>>>();

    auto whole = container.template resolve<shared_ptr_variant>();
    ASSERT_TRUE(std::holds_alternative<shared_ptr_variant_a>(whole));
    EXPECT_EQ(std::get<shared_ptr_variant_a>(whole).value, 0);

    auto& value = container.template resolve<shared_ptr_variant&>();
    ASSERT_TRUE(std::holds_alternative<shared_ptr_variant_a>(value));
    EXPECT_EQ(std::get<shared_ptr_variant_a>(value).value, 0);

    auto copy = container.template resolve<shared_ptr_variant_a>();
    EXPECT_EQ(copy.value, 0);

    auto& selected = container.template resolve<shared_ptr_variant_a&>();
    EXPECT_EQ(selected.value, 0);
    EXPECT_EQ(&selected, &std::get<shared_ptr_variant_a>(value));

    auto* selected_ptr = container.template resolve<shared_ptr_variant_a*>();
    ASSERT_NE(selected_ptr, nullptr);
    EXPECT_EQ(selected_ptr, &std::get<shared_ptr_variant_a>(value));

    ASSERT_THROW(container.template resolve<shared_ptr_variant_b>(),
                 type_not_convertible_exception);
    ASSERT_THROW(container.template resolve<shared_ptr_variant_b&>(),
                 type_not_convertible_exception);
    ASSERT_THROW(container.template resolve<shared_ptr_variant_b*>(),
                 type_not_convertible_exception);
}

TYPED_TEST(shared_variant_test,
           shared_ptr_custom_sum_factory_selects_alternative) {
    using container_type = TypeParam;

    container_type container;
    container.template register_type<scope<unique>, storage<int>>();
    container.template register_type<
        scope<shared>, storage<std::shared_ptr<custom_sum>>,
        factory<function<&make_shared_custom_sum>>>();

    auto whole = container.template resolve<custom_sum>();
    ASSERT_TRUE(std::holds_alternative<custom_sum_a>(whole.value));
    EXPECT_EQ(std::get<custom_sum_a>(whole.value).value, 0);

    auto& value = container.template resolve<custom_sum&>();
    ASSERT_TRUE(std::holds_alternative<custom_sum_a>(value.value));
    EXPECT_EQ(std::get<custom_sum_a>(value.value).value, 0);

    auto copy = container.template resolve<custom_sum_a>();
    EXPECT_EQ(copy.value, 0);

    auto& selected = container.template resolve<custom_sum_a&>();
    EXPECT_EQ(selected.value, 0);
    EXPECT_EQ(&selected, &std::get<custom_sum_a>(value.value));

    auto* selected_ptr = container.template resolve<custom_sum_a*>();
    ASSERT_NE(selected_ptr, nullptr);
    EXPECT_EQ(selected_ptr, &std::get<custom_sum_a>(value.value));

    ASSERT_THROW(container.template resolve<custom_sum_b>(),
                 type_not_convertible_exception);
    ASSERT_THROW(container.template resolve<custom_sum_b&>(),
                 type_not_convertible_exception);
    ASSERT_THROW(container.template resolve<custom_sum_b*>(),
                 type_not_convertible_exception);
}

TYPED_TEST(shared_variant_test,
           unique_ptr_variant_factory_selects_alternative) {
    using container_type = TypeParam;

    container_type container;
    container.template register_type<scope<unique>, storage<int>>();
    container.template register_type<
        scope<shared>, storage<std::unique_ptr<shared_ptr_variant>>,
        factory<function<&make_unique_ptr_variant>>>();

    auto whole = container.template resolve<shared_ptr_variant>();
    ASSERT_TRUE(std::holds_alternative<shared_ptr_variant_a>(whole));
    EXPECT_EQ(std::get<shared_ptr_variant_a>(whole).value, 0);

    auto& value = container.template resolve<shared_ptr_variant&>();
    ASSERT_TRUE(std::holds_alternative<shared_ptr_variant_a>(value));
    EXPECT_EQ(std::get<shared_ptr_variant_a>(value).value, 0);

    auto copy = container.template resolve<shared_ptr_variant_a>();
    EXPECT_EQ(copy.value, 0);

    auto& selected = container.template resolve<shared_ptr_variant_a&>();
    EXPECT_EQ(selected.value, 0);
    EXPECT_EQ(&selected, &std::get<shared_ptr_variant_a>(value));

    auto* selected_ptr = container.template resolve<shared_ptr_variant_a*>();
    ASSERT_NE(selected_ptr, nullptr);
    EXPECT_EQ(selected_ptr, &std::get<shared_ptr_variant_a>(value));

    ASSERT_THROW(container.template resolve<shared_ptr_variant_b>(),
                 type_not_convertible_exception);
    ASSERT_THROW(container.template resolve<shared_ptr_variant_b&>(),
                 type_not_convertible_exception);
    ASSERT_THROW(container.template resolve<shared_ptr_variant_b*>(),
                 type_not_convertible_exception);
}

TYPED_TEST(shared_variant_test,
           unique_ptr_variant_direct_construction_selects_alternative) {
    using container_type = TypeParam;

    container_type container;
    container.template register_type<scope<unique>, storage<int>>();
    container.template register_type<
        scope<shared>, storage<std::unique_ptr<shared_ptr_variant>>,
        factory<constructor<shared_ptr_variant_a>>>();

    auto whole = container.template resolve<shared_ptr_variant>();
    ASSERT_TRUE(std::holds_alternative<shared_ptr_variant_a>(whole));
    EXPECT_EQ(std::get<shared_ptr_variant_a>(whole).value, 0);

    auto& value = container.template resolve<shared_ptr_variant&>();
    ASSERT_TRUE(std::holds_alternative<shared_ptr_variant_a>(value));
    EXPECT_EQ(std::get<shared_ptr_variant_a>(value).value, 0);

    auto copy = container.template resolve<shared_ptr_variant_a>();
    EXPECT_EQ(copy.value, 0);

    auto& selected = container.template resolve<shared_ptr_variant_a&>();
    EXPECT_EQ(selected.value, 0);
    EXPECT_EQ(&selected, &std::get<shared_ptr_variant_a>(value));

    auto* selected_ptr = container.template resolve<shared_ptr_variant_a*>();
    ASSERT_NE(selected_ptr, nullptr);
    EXPECT_EQ(selected_ptr, &std::get<shared_ptr_variant_a>(value));

    ASSERT_THROW(container.template resolve<shared_ptr_variant_b>(),
                 type_not_convertible_exception);
    ASSERT_THROW(container.template resolve<shared_ptr_variant_b&>(),
                 type_not_convertible_exception);
    ASSERT_THROW(container.template resolve<shared_ptr_variant_b*>(),
                 type_not_convertible_exception);
}

} // namespace dingo
