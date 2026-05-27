//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#include "storage/external_common.h"

namespace dingo {

TYPED_TEST_SUITE(external_unique_test, container_types, );

TYPED_TEST(external_unique_test, unique_ptr_ref) {
    using container_type = TypeParam;
    auto c = std::make_unique<Class>();

    container_type container;
    container.template register_type<scope<external>,
                                     storage<std::unique_ptr<Class>&>,
                                     interfaces<Class, IClass>>(c);
    AssertClass(container.template resolve<Class&>());
    AssertClass(container.template resolve<const Class&>());
    AssertClass(container.template resolve<Class*>());
    AssertClass(container.template resolve<const Class*>());
    AssertClass(*container.template resolve<std::unique_ptr<Class>&>());
    AssertClass(*container.template resolve<const std::unique_ptr<Class>&>());
    AssertClass(container.template resolve<IClass&>());
    AssertClass(container.template resolve<const IClass&>());
    AssertClass(container.template resolve<IClass*>());
    AssertClass(container.template resolve<const IClass*>());

    ASSERT_THROW(container.template resolve<std::unique_ptr<IClass>&>(),
                 type_not_convertible_exception);
}

TYPED_TEST(external_unique_test,
           exception_message_type_not_convertible_unique_ptr_interface_ref) {
    using container_type = TypeParam;
    auto c = std::make_unique<Class>();

    container_type container;
    container.template register_type<scope<external>,
                                     storage<std::unique_ptr<Class>&>,
                                     interfaces<Class, IClass>>(c);

    try {
        (void)container.template resolve<std::unique_ptr<IClass>&>();
        FAIL() << "expected type_not_convertible_exception";
    } catch (const type_not_convertible_exception& e) {
        std::string expected = "type is not convertible to ";
        expected += type_name<std::unique_ptr<IClass>&>();
        expected += " from ";
        expected += type_name<std::unique_ptr<Class>&>();
        ASSERT_STREQ(e.what(), expected.c_str());
    }
}

TYPED_TEST(external_unique_test, unique_ptr_move) {
    using container_type = TypeParam;
    auto c = std::make_unique<Class>();

    container_type container;
    container.template register_type<scope<external>,
                                     storage<std::unique_ptr<Class>>,
                                     interfaces<Class, IClass>>(std::move(c));
    AssertClass(container.template resolve<Class&>());
    AssertClass(container.template resolve<const Class&>());
    AssertClass(container.template resolve<Class*>());
    AssertClass(container.template resolve<const Class*>());
    AssertClass(*container.template resolve<std::unique_ptr<Class>&>());
    AssertClass(*container.template resolve<const std::unique_ptr<Class>&>());
    AssertClass(container.template resolve<IClass&>());
    AssertClass(container.template resolve<const IClass&>());
    AssertClass(container.template resolve<IClass*>());
    AssertClass(container.template resolve<const IClass*>());

    ASSERT_THROW(container.template resolve<std::unique_ptr<IClass>&>(),
                 type_not_convertible_exception);
}

// TODO: this excercises resolving code without creating temporaries,
// yet there is no way how to test it
TYPED_TEST(external_unique_test, unique_ptr_interface_move) {
    using container_type = TypeParam;
    auto c = std::make_unique<Class>();

    container_type container;
    container.template register_type<
        scope<external>, storage<std::unique_ptr<Class>>, interfaces<IClass>>(
        std::move(c));
    AssertClass(*container.template resolve<std::unique_ptr<IClass>&>());
    AssertClass(container.template resolve<IClass&>());
}

TYPED_TEST(external_unique_test,
           exception_message_type_not_convertible_unique_ptr_value) {
    using container_type = TypeParam;
    auto c = std::make_unique<Class>();

    container_type container;
    container.template register_type<scope<external>,
                                     storage<std::unique_ptr<Class>>,
                                     interfaces<Class, IClass>>(std::move(c));

    try {
        (void)container.template resolve<Class>();
        FAIL() << "expected type_not_convertible_exception";
    } catch (const type_not_convertible_exception& e) {
        std::string expected = "type is not convertible to ";
        expected += type_name<Class>();
        expected += " from ";
        expected += type_name<std::unique_ptr<Class>>();
        ASSERT_STREQ(e.what(), expected.c_str());
    }
}

TYPED_TEST(external_unique_test, optional_ref) {
    using container_type = TypeParam;
    auto c = std::make_optional<Class>();

    container_type container;
    container
        .template register_type<scope<external>, storage<std::optional<Class>&>,
                                interfaces<Class, IClass>>(c);
    ASSERT_EQ(&container.template resolve<std::optional<Class>&>(), &c);
    ASSERT_EQ(container.template resolve<Class*>(), std::addressof(c.value()));
    AssertClass(*container.template resolve<std::optional<Class>&>());
    AssertClass(container.template resolve<Class&>());
    AssertClass(container.template resolve<IClass&>());
}

TYPED_TEST(external_unique_test, optional_move) {
    using container_type = TypeParam;
    auto c = std::make_optional<Class>();

    container_type container;
    container
        .template register_type<scope<external>, storage<std::optional<Class>>,
                                interfaces<Class, IClass>>(std::move(c));
    AssertClass(*container.template resolve<std::optional<Class>&>());
    AssertClass(container.template resolve<Class&>());
    AssertClass(container.template resolve<IClass&>());
}

TYPED_TEST(external_unique_test,
           exception_message_type_not_convertible_optional_interface_ref) {
    using container_type = TypeParam;
    struct Base {};
    struct Derived : Base {};

    auto c = std::make_optional<Derived>();

    container_type container;
    container.template register_type<
        scope<external>, storage<std::optional<Derived>&>, interfaces<Base>>(c);

    try {
        (void)container.template resolve<std::optional<Base>&>();
        FAIL() << "expected type_not_convertible_exception";
    } catch (const type_not_convertible_exception& e) {
        std::string expected = "type is not convertible to ";
        expected += type_name<std::optional<Base>&>();
        expected += " from ";
        expected += type_name<std::optional<Derived>&>();
        ASSERT_STREQ(e.what(), expected.c_str());
    }
}

TYPED_TEST(external_unique_test,
           exception_message_type_not_convertible_optional_value) {
    using container_type = TypeParam;
    auto c = std::make_optional<Class>();

    container_type container;
    container
        .template register_type<scope<external>, storage<std::optional<Class>>,
                                interfaces<Class, IClass>>(std::move(c));

    try {
        (void)container.template resolve<Class>();
        FAIL() << "expected type_not_convertible_exception";
    } catch (const type_not_convertible_exception& e) {
        std::string expected = "type is not convertible to ";
        expected += type_name<Class>();
        expected += " from ";
        expected += type_name<std::optional<Class>>();
        ASSERT_STREQ(e.what(), expected.c_str());
    }
}

} // namespace dingo
