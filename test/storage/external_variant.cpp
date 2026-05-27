//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#include "storage/external_common.h"

namespace dingo {

TYPED_TEST_SUITE(external_variant_test, container_types, );

TYPED_TEST(external_variant_test, variant_ref) {
    using container_type = TypeParam;
    struct A {
        explicit A(int init) : value(init) {}
        int value;
    };
    struct B {
        explicit B(float init) : value(init) {}
        float value;
    };

    std::variant<A, B> c(std::in_place_type<A>, 7);

    container_type container;
    container
        .template register_type<scope<external>, storage<std::variant<A, B>&>>(
            c);

    auto whole = container.template resolve<std::variant<A, B>>();
    ASSERT_TRUE(std::holds_alternative<A>(whole));
    EXPECT_EQ(std::get<A>(whole).value, 7);

    auto& value = container.template resolve<std::variant<A, B>&>();

    ASSERT_EQ(&value, &c);
    ASSERT_TRUE(std::holds_alternative<A>(value));
    EXPECT_EQ(std::get<A>(value).value, 7);

    auto copy = container.template resolve<A>();
    EXPECT_EQ(copy.value, 7);

    auto& selected = container.template resolve<A&>();
    EXPECT_EQ(selected.value, 7);
    EXPECT_EQ(&selected, &std::get<A>(c));

    auto& selected_const = container.template resolve<const A&>();
    EXPECT_EQ(selected_const.value, 7);
    EXPECT_EQ(&selected_const, &std::get<A>(c));

    auto* selected_ptr = container.template resolve<A*>();
    ASSERT_NE(selected_ptr, nullptr);
    EXPECT_EQ(selected_ptr, &std::get<A>(c));

    auto* selected_const_ptr = container.template resolve<const A*>();
    ASSERT_NE(selected_const_ptr, nullptr);
    EXPECT_EQ(selected_const_ptr, &std::get<A>(c));

    ASSERT_THROW(container.template resolve<B>(),
                 type_not_convertible_exception);
    ASSERT_THROW(container.template resolve<B&>(),
                 type_not_convertible_exception);
    ASSERT_THROW(container.template resolve<B*>(),
                 type_not_convertible_exception);
}

TYPED_TEST(external_variant_test,
           variant_explicit_interfaces_override_defaults) {
    using container_type = TypeParam;

    struct A {
        explicit A(int init) : value(init) {}
        int value;
    };
    struct B {
        explicit B(float init) : value(init) {}
        float value;
    };

    std::variant<A, B> c(std::in_place_type<A>, 11);

    container_type container;
    container.template register_type<
        scope<external>, storage<std::variant<A, B>&>, interfaces<A>>(c);

    auto copy = container.template resolve<A>();
    EXPECT_EQ(copy.value, 11);

    auto& selected = container.template resolve<A&>();
    EXPECT_EQ(selected.value, 11);
    EXPECT_EQ(&selected, &std::get<A>(c));

    auto* selected_ptr = container.template resolve<A*>();
    ASSERT_NE(selected_ptr, nullptr);
    EXPECT_EQ(selected_ptr, &std::get<A>(c));

    ASSERT_THROW((container.template resolve<std::variant<A, B>>()),
                 type_not_found_exception);
    ASSERT_THROW((container.template resolve<std::variant<A, B>&>()),
                 type_not_found_exception);
    ASSERT_THROW((container.template resolve<std::variant<A, B>*>()),
                 type_not_found_exception);
    ASSERT_THROW(container.template resolve<B>(), type_not_found_exception);
}

TYPED_TEST(external_variant_test, shared_ptr_variant_ref) {
    using container_type = TypeParam;

    auto handle = std::make_shared<shared_ptr_variant>(
        std::in_place_type<shared_ptr_variant_a>, 13);

    container_type container;
    container.template register_type<
        scope<external>, storage<std::shared_ptr<shared_ptr_variant>>>(handle);

    auto whole = container.template resolve<shared_ptr_variant>();
    ASSERT_TRUE(std::holds_alternative<shared_ptr_variant_a>(whole));
    EXPECT_EQ(std::get<shared_ptr_variant_a>(whole).value, 13);

    auto& value = container.template resolve<shared_ptr_variant&>();
    ASSERT_TRUE(std::holds_alternative<shared_ptr_variant_a>(value));
    EXPECT_EQ(std::get<shared_ptr_variant_a>(value).value, 13);

    auto copy = container.template resolve<shared_ptr_variant_a>();
    EXPECT_EQ(copy.value, 13);

    auto& selected = container.template resolve<shared_ptr_variant_a&>();
    EXPECT_EQ(selected.value, 13);
    EXPECT_EQ(&selected, &std::get<shared_ptr_variant_a>(*handle));

    auto* selected_ptr = container.template resolve<shared_ptr_variant_a*>();
    ASSERT_NE(selected_ptr, nullptr);
    EXPECT_EQ(selected_ptr, &std::get<shared_ptr_variant_a>(*handle));

    ASSERT_THROW(container.template resolve<shared_ptr_variant_b>(),
                 type_not_convertible_exception);
    ASSERT_THROW(container.template resolve<shared_ptr_variant_b&>(),
                 type_not_convertible_exception);
    ASSERT_THROW(container.template resolve<shared_ptr_variant_b*>(),
                 type_not_convertible_exception);
}

TYPED_TEST(external_variant_test, unique_ptr_variant_ref) {
    using container_type = TypeParam;

    auto handle = std::make_unique<shared_ptr_variant>(
        std::in_place_type<shared_ptr_variant_a>, 13);

    container_type container;
    container.template register_type<
        scope<external>, storage<std::unique_ptr<shared_ptr_variant>&>>(handle);

    ASSERT_THROW(container.template resolve<shared_ptr_variant>(),
                 type_not_convertible_exception);
    ASSERT_THROW(container.template resolve<shared_ptr_variant_a>(),
                 type_not_convertible_exception);

    auto& value = container.template resolve<shared_ptr_variant&>();
    ASSERT_TRUE(std::holds_alternative<shared_ptr_variant_a>(value));
    EXPECT_EQ(std::get<shared_ptr_variant_a>(value).value, 13);

    auto* value_ptr = container.template resolve<shared_ptr_variant*>();
    ASSERT_NE(value_ptr, nullptr);
    EXPECT_EQ(value_ptr, &*handle);

    auto& selected = container.template resolve<shared_ptr_variant_a&>();
    EXPECT_EQ(selected.value, 13);
    EXPECT_EQ(&selected, &std::get<shared_ptr_variant_a>(*handle));

    auto* selected_ptr = container.template resolve<shared_ptr_variant_a*>();
    ASSERT_NE(selected_ptr, nullptr);
    EXPECT_EQ(selected_ptr, &std::get<shared_ptr_variant_a>(*handle));

    ASSERT_THROW(container.template resolve<shared_ptr_variant_b>(),
                 type_not_convertible_exception);
    ASSERT_THROW(container.template resolve<shared_ptr_variant_b&>(),
                 type_not_convertible_exception);
    ASSERT_THROW(container.template resolve<shared_ptr_variant_b*>(),
                 type_not_convertible_exception);
}

TYPED_TEST(external_variant_test, unique_ptr_variant_move) {
    using container_type = TypeParam;

    auto handle = std::make_unique<shared_ptr_variant>(
        std::in_place_type<shared_ptr_variant_a>, 17);

    container_type container;
    container.template register_type<
        scope<external>, storage<std::unique_ptr<shared_ptr_variant>>>(
        std::move(handle));

    ASSERT_THROW(container.template resolve<shared_ptr_variant>(),
                 type_not_convertible_exception);
    ASSERT_THROW(container.template resolve<shared_ptr_variant_a>(),
                 type_not_convertible_exception);

    auto& value = container.template resolve<shared_ptr_variant&>();
    ASSERT_TRUE(std::holds_alternative<shared_ptr_variant_a>(value));
    EXPECT_EQ(std::get<shared_ptr_variant_a>(value).value, 17);

    auto* value_ptr = container.template resolve<shared_ptr_variant*>();
    ASSERT_NE(value_ptr, nullptr);
    EXPECT_EQ(value_ptr, &value);

    auto& selected = container.template resolve<shared_ptr_variant_a&>();
    EXPECT_EQ(selected.value, 17);
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
