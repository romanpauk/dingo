//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#include "storage/external_common.h"

namespace dingo {

TYPED_TEST_SUITE(external_test, container_types, );

TYPED_TEST(external_test, value) {
    using container_type = TypeParam;
    Class c;

    container_type container;
    container.template register_type<scope<external>, storage<Class>,
                                     interfaces<Class, IClass>>(std::move(c));

    AssertClass(*container.template resolve<Class*>());
    AssertClass(*container.template resolve<const Class*>());
    AssertClass(container.template resolve<Class&>());
    AssertClass(container.template resolve<const Class&>());
    AssertClass(container.template resolve<IClass&>());
    AssertClass(container.template resolve<const IClass&>());
    AssertClass(container.template resolve<IClass*>());
    AssertClass(container.template resolve<const IClass*>());
    ASSERT_THROW(container.template resolve<std::optional<Class>>(),
                 type_not_convertible_exception);
}

TYPED_TEST(external_test, ref) {
    using container_type = TypeParam;
    struct Mutable {
        int value;
    };
    Class c;
    Mutable object{1};

    container_type container;
    container.template register_type<scope<external>, storage<Class&>,
                                     interfaces<Class, IClass>>(c);
    container.template register_type<scope<external>, storage<Mutable&>>(
        object);

    ASSERT_EQ(container.template resolve<Class*>(), &c);
    AssertClass(*container.template resolve<Class*>());
    AssertClass(*container.template resolve<const Class*>());
    AssertClass(container.template resolve<Class&>());
    AssertClass(container.template resolve<const Class&>());
    AssertClass(container.template resolve<IClass&>());
    AssertClass(container.template resolve<const IClass&>());
    AssertClass(container.template resolve<IClass*>());
    AssertClass(container.template resolve<const IClass*>());

    auto& injected = container.template resolve<Mutable&>();
    ASSERT_EQ(&injected, &object);
    EXPECT_EQ(injected.value, 1);

    object.value = 2;
    EXPECT_EQ(injected.value, 2);

    injected.value = 3;
    EXPECT_EQ(object.value, 3);
}

TYPED_TEST(external_test, ptr) {
    using container_type = TypeParam;
    Class c;

    container_type container;
    container.template register_type<scope<external>, storage<Class*>,
                                     interfaces<Class, IClass>>(&c);

    ASSERT_EQ(container.template resolve<Class*>(), &c);
    AssertClass(*container.template resolve<Class*>());
    AssertClass(*container.template resolve<const Class*>());
    AssertClass(container.template resolve<Class&>());
    AssertClass(container.template resolve<const Class&>());
    AssertClass(container.template resolve<IClass&>());
    AssertClass(container.template resolve<const IClass&>());
    AssertClass(container.template resolve<IClass*>());
    AssertClass(container.template resolve<const IClass*>());

    ASSERT_THROW(container.template resolve<std::unique_ptr<Class>>(),
                 type_not_convertible_exception);
    ASSERT_THROW(container.template resolve<std::unique_ptr<IClass>>(),
                 type_not_convertible_exception);
    ASSERT_THROW(container.template resolve<std::shared_ptr<Class>>(),
                 type_not_convertible_exception);
    ASSERT_THROW(container.template resolve<std::shared_ptr<IClass>>(),
                 type_not_convertible_exception);
}

// TODO: this excercises resolving code without creating temporaries,
// yet there is no way how to test it
TYPED_TEST(external_test, shared_ptr_interface) {
    using container_type = TypeParam;
    auto c = std::make_shared<Class>();

    container_type container;
    container.template register_type<
        scope<external>, storage<std::shared_ptr<Class>>, interfaces<IClass>>(
        c);
    AssertClass(container.template resolve<IClass&>());
    AssertClass(container.template resolve<IClass*>());
    AssertClass(*container.template resolve<std::shared_ptr<IClass>>());
}

TYPED_TEST(external_test,
           exception_message_type_not_convertible_shared_ptr_interface) {
    using container_type = TypeParam;
    auto c = std::make_shared<Class>();

    container_type container;
    container.template register_type<
        scope<external>, storage<std::shared_ptr<Class>>, interfaces<IClass>>(
        c);

    try {
        (void)container.template resolve<std::unique_ptr<IClass>>();
        FAIL() << "expected type_not_convertible_exception";
    } catch (const type_not_convertible_exception& e) {
        std::string expected = "type is not convertible to ";
        expected += type_name<std::unique_ptr<IClass>>();
        expected += " from ";
        expected += type_name<std::shared_ptr<Class>>();
        ASSERT_STREQ(e.what(), expected.c_str());
    }
}

TYPED_TEST(external_test, shared_ptr) {
    using container_type = TypeParam;
    auto c = std::make_shared<Class>();

    container_type container;
    container.template register_type<scope<external>,
                                     storage<std::shared_ptr<Class>>,
                                     interfaces<Class, IClass>>(c);
    AssertClass(container.template resolve<Class&>());
    AssertClass(container.template resolve<const Class&>());
    AssertClass(*container.template resolve<Class*>());
    AssertClass(*container.template resolve<const Class*>());
    AssertClass(*container.template resolve<std::shared_ptr<Class>>());
    AssertClass(**container.template resolve<std::shared_ptr<Class>*>());
    AssertClass(*container.template resolve<std::shared_ptr<const Class>>());
    AssertClass(*container.template resolve<const std::shared_ptr<Class>>());
    AssertClass(**container.template resolve<const std::shared_ptr<Class>*>());
    AssertClass(
        *container.template resolve<const std::shared_ptr<const Class>>());
    AssertClass(*container.template resolve<std::shared_ptr<Class>&>());
    AssertClass(*container.template resolve<std::shared_ptr<const Class>&>());
    AssertClass(*container.template resolve<const std::shared_ptr<Class>&>());
    AssertClass(
        *container.template resolve<const std::shared_ptr<const Class>&>());
    AssertClass(container.template resolve<IClass&>());
    AssertClass(container.template resolve<const IClass&>());
    AssertClass(container.template resolve<IClass*>());
    AssertClass(container.template resolve<const IClass*>());
    AssertClass(*container.template resolve<std::shared_ptr<IClass>>());
    AssertClass(*container.template resolve<const std::shared_ptr<IClass>>());
    AssertClass(*container.template resolve<std::shared_ptr<const IClass>>());
    AssertClass(
        *container.template resolve<const std::shared_ptr<const IClass>>());
}

TYPED_TEST(external_test, shared_ptr_ref) {
    using container_type = TypeParam;
    auto c = std::make_shared<Class>();

    container_type container;
    container.template register_type<scope<external>,
                                     storage<std::shared_ptr<Class>&>,
                                     interfaces<Class, IClass>>(c);
    AssertClass(container.template resolve<Class&>());
    AssertClass(container.template resolve<const Class&>());
    AssertClass(container.template resolve<Class*>());
    AssertClass(container.template resolve<const Class*>());
    AssertClass(container.template resolve<std::shared_ptr<Class>>());
    AssertClass(container.template resolve<std::shared_ptr<const Class>>());
    AssertClass(*container.template resolve<const std::shared_ptr<Class>>());
    AssertClass(*container.template resolve<std::shared_ptr<Class>&>());
    AssertClass(*container.template resolve<const std::shared_ptr<Class>&>());
    AssertClass(container.template resolve<std::shared_ptr<IClass>>());
    AssertClass(*container.template resolve<const std::shared_ptr<IClass>>());
    AssertClass(container.template resolve<std::shared_ptr<const IClass>>());
    AssertClass(container.template resolve<IClass&>());
    AssertClass(container.template resolve<const IClass&>());
    AssertClass(container.template resolve<IClass*>());
    AssertClass(container.template resolve<const IClass*>());
}

TYPED_TEST(external_test,
           exception_message_type_not_convertible_shared_ptr_interface_ref) {
    using container_type = TypeParam;
    auto c = std::make_shared<Class>();

    container_type container;
    container.template register_type<
        scope<external>, storage<std::shared_ptr<Class>&>, interfaces<IClass>>(
        c);

    try {
        (void)container.template resolve<std::unique_ptr<IClass>>();
        FAIL() << "expected type_not_convertible_exception";
    } catch (const type_not_convertible_exception& e) {
        std::string expected = "type is not convertible to ";
        expected += type_name<std::unique_ptr<IClass>>();
        expected += " from ";
        expected += type_name<std::shared_ptr<Class>&>();
        ASSERT_STREQ(e.what(), expected.c_str());
    }
}

} // namespace dingo
