//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#include "storage/shared_common.h"

namespace dingo {

TYPED_TEST_SUITE(shared_test, container_types, );

TYPED_TEST(shared_test, value) {
    using container_type = TypeParam;

    {
        container_type container;
        container.template register_type<scope<shared>, storage<Class>,
                                         interfaces<Class, IClass>>();

        AssertClass(*container.template resolve<Class*>());
        AssertClass(*container.template resolve<const Class*>());
        AssertClass(container.template resolve<Class&>());
        AssertClass(container.template resolve<const Class&>());

        AssertTypeNotConvertible<
            Class, type_list<std::shared_ptr<Class>, std::unique_ptr<Class>>>(
            container);

        ASSERT_EQ(Class::Constructor, 1);
        ASSERT_EQ(Class::Destructor, 0);
        ASSERT_EQ(Class::CopyConstructor, 0);
        ASSERT_EQ(Class::MoveConstructor, 0);
    }

    {
        ASSERT_EQ(Class::Destructor, Class::GetTotalInstances());
    }
}

TYPED_TEST(shared_test, value_interface) {
    using container_type = TypeParam;

    container_type container;
    container.template register_type<scope<shared>, storage<Class>,
                                     interfaces<IClass>>();

    AssertClass(container.template resolve<IClass&>());
    AssertClass(container.template resolve<const IClass&>());
    AssertClass(container.template resolve<IClass*>());
    AssertClass(container.template resolve<const IClass*>());
}

TYPED_TEST(shared_test, ptr) {
    using container_type = TypeParam;

    {
        container_type container;
        container.template register_type<scope<shared>, storage<Class*>,
                                         interfaces<Class, IClass>>();

        AssertClass(*container.template resolve<Class*>());
        AssertClass(*container.template resolve<const Class*>());
        AssertClass(container.template resolve<Class&>());
        AssertClass(container.template resolve<const Class&>());

        AssertTypeNotConvertible<
            Class, type_list<std::shared_ptr<Class>, std::unique_ptr<Class>>>(
            container);

        ASSERT_EQ(Class::Constructor, 1);
        ASSERT_EQ(Class::Destructor, 0);
        ASSERT_EQ(Class::CopyConstructor, 0);
        ASSERT_EQ(Class::MoveConstructor, 0);
    }

    {
        ASSERT_EQ(Class::Destructor, Class::GetTotalInstances());
    }
}

TYPED_TEST(shared_test, ptr_interface) {
    using container_type = TypeParam;

    container_type container;
    container.template register_type<scope<shared>, storage<Class*>,
                                     interfaces<IClass>>();

    AssertClass(container.template resolve<IClass&>());
    AssertClass(container.template resolve<const IClass&>());
    AssertClass(container.template resolve<IClass*>());
    AssertClass(container.template resolve<const IClass*>());
}

TYPED_TEST(shared_test, ptr_interface_deletes_owner_not_converted_pointer) {
    using container_type = TypeParam;

    pointer_conversion_owner::destructors = 0;

    {
        container_type container;
        container.template register_type<scope<shared>,
                                         storage<pointer_conversion_owner*>,
                                         interfaces<pointer_conversion_view>>();

        auto* view = container.template resolve<pointer_conversion_view*>();
        ASSERT_NE(view, nullptr);
        EXPECT_EQ(pointer_conversion_owner::destructors, 0);
    }

    EXPECT_EQ(pointer_conversion_owner::destructors, 1);
}

TYPED_TEST(shared_test, shared_ptr) {
    using container_type = TypeParam;

    {
        container_type container;
        container.template register_type<scope<shared>,
                                         storage<std::shared_ptr<Class>>,
                                         interfaces<Class, IClass>>();

        AssertClass(*container.template resolve<Class*>());
        AssertClass(*container.template resolve<const Class*>());
        AssertClass(container.template resolve<Class&>());
        AssertClass(container.template resolve<const Class&>());
        AssertClass(*container.template resolve<std::shared_ptr<Class>>());
        AssertClass(
            *container.template resolve<std::shared_ptr<const Class>>());
        AssertClass(*container.template resolve<std::shared_ptr<Class>&>());
        AssertClass(
            *container.template resolve<const std::shared_ptr<Class>&>());
        AssertClass(
            *container.template resolve<const std::shared_ptr<const Class>&>());
        AssertClass(**container.template resolve<std::shared_ptr<Class>*>());
        AssertClass(
            **container.template resolve<const std::shared_ptr<Class>*>());
        AssertClass(
            **container
                  .template resolve<const std::shared_ptr<const Class>*>());

        AssertTypeNotConvertible<Class, type_list<std::unique_ptr<Class>>>(
            container);

        ASSERT_EQ(Class::Constructor, 1);
        ASSERT_EQ(Class::Destructor, 0);
        ASSERT_EQ(Class::CopyConstructor, 0);
        ASSERT_EQ(Class::MoveConstructor, 0);
    }

    {
        ASSERT_EQ(Class::Destructor, Class::GetTotalInstances());
    }
}

TYPED_TEST(shared_test, shared_ptr_multiple_interface) {
    using container_type = TypeParam;

    container_type container;
    container
        .template register_type<scope<shared>, storage<std::shared_ptr<Class>>,
                                interfaces<Class, IClass>>();

    AssertClass(*container.template resolve<std::shared_ptr<Class>&>());
    AssertClass(container.template resolve<std::shared_ptr<IClass>>());
    AssertClass(*container.template resolve<std::shared_ptr<IClass>&>());
}

// TODO: this excercises resolving code without creating temporaries,
// yet there is no way how to test it
TYPED_TEST(shared_test, shared_ptr_single_interface) {
    using container_type = TypeParam;

    container_type container;
    container.template register_type<
        scope<shared>, storage<std::shared_ptr<Class>>, interfaces<IClass>>();

    AssertClass(container.template resolve<std::shared_ptr<IClass>>());
    AssertClass(*container.template resolve<std::shared_ptr<IClass>&>());
}

TYPED_TEST(shared_test, shared_ptr_nested_unique_ptr) {
    using container_type = TypeParam;

    container_type container;
    container.template register_type<
        scope<shared>, storage<std::shared_ptr<std::unique_ptr<Class>>>,
        interfaces<Class>>();

    auto& outer =
        container.template resolve<std::shared_ptr<std::unique_ptr<Class>>&>();
    auto& inner = container.template resolve<std::unique_ptr<Class>&>();
    auto* leaf = container.template resolve<Class*>();

    AssertClass(*inner);
    AssertClass(*leaf);
    ASSERT_EQ(outer.get(), std::addressof(inner));
    ASSERT_EQ(inner.get(), leaf);
    ASSERT_THROW(container.template resolve<std::unique_ptr<Class>>(),
                 type_not_convertible_exception);
}

TYPED_TEST(shared_test, shared_ptr_nested_unique_ptr_interface) {
    using container_type = TypeParam;

    container_type container;
    container.template register_type<
        scope<shared>, storage<std::shared_ptr<std::unique_ptr<Class>>>,
        interfaces<IClass>>();

    auto& leaf = container.template resolve<IClass&>();
    auto* ptr = container.template resolve<IClass*>();

    AssertClass(leaf);
    AssertClass(*ptr);
    ASSERT_EQ(ptr, std::addressof(leaf));
    ASSERT_THROW(container.template resolve<std::unique_ptr<IClass>&>(),
                 type_not_convertible_exception);
    ASSERT_THROW(container.template resolve<std::shared_ptr<IClass>>(),
                 type_not_convertible_exception);
    ASSERT_THROW(
        container.template resolve<std::shared_ptr<std::unique_ptr<IClass>>&>(),
        type_not_convertible_exception);
}

TYPED_TEST(shared_test, unique_ptr) {
    using container_type = TypeParam;

    {
        container_type container;
        container.template register_type<scope<shared>,
                                         storage<std::unique_ptr<Class>>,
                                         interfaces<Class>>();

        AssertClass(*container.template resolve<Class*>());
        AssertClass(*container.template resolve<const Class*>());
        AssertClass(container.template resolve<const Class&>());
        AssertClass(*container.template resolve<std::unique_ptr<Class>&>());
        AssertClass(
            *container.template resolve<const std::unique_ptr<Class>&>());
        AssertClass(
            *container.template resolve<const std::unique_ptr<const Class>&>());
        AssertClass(**container.template resolve<std::unique_ptr<Class>*>());
        AssertClass(
            **container.template resolve<const std::unique_ptr<Class>*>());
        AssertClass(
            **container
                  .template resolve<const std::unique_ptr<const Class>*>());

        AssertTypeNotConvertible<Class, type_list<std::unique_ptr<Class>>>(
            container);

        ASSERT_EQ(Class::Constructor, 1);
        ASSERT_EQ(Class::Destructor, 0);
        ASSERT_EQ(Class::CopyConstructor, 0);
        ASSERT_EQ(Class::MoveConstructor, 0);
    }

    {
        ASSERT_EQ(Class::Destructor, Class::GetTotalInstances());
    }
}

TYPED_TEST(shared_test, unique_ptr_interface) {
    using container_type = TypeParam;

    container_type container;
    container.template register_type<
        scope<shared>, storage<std::unique_ptr<Class>>, interfaces<IClass>>();
    AssertClass(container.template resolve<IClass*>());
    AssertClass(container.template resolve<IClass&>());
    AssertClass(*container.template resolve<std::unique_ptr<IClass>&>());
}

TYPED_TEST(shared_test, optional) {
    using container_type = TypeParam;

    {
        container_type container;
        container.template register_type<scope<shared>,
                                         storage<std::optional<Class>>,
                                         interfaces<Class, IClass>>();

        AssertClass(container.template resolve<Class*>());
        AssertClass(container.template resolve<const Class*>());
        AssertClass(container.template resolve<Class&>());
        AssertClass(container.template resolve<const Class&>());
        AssertClass(*container.template resolve<std::optional<Class>&>());
        AssertClass(*container.template resolve<const std::optional<Class>&>());
        AssertClass(**container.template resolve<std::optional<Class>*>());
        AssertClass(
            **container.template resolve<const std::optional<Class>*>());

        ASSERT_EQ(Class::Constructor, 1);
        ASSERT_EQ(Class::MoveConstructor, 1);
        ASSERT_EQ(Class::Destructor, 1);
        ASSERT_EQ(Class::CopyConstructor, 0);
    }

    {
        ASSERT_EQ(Class::Destructor, Class::GetTotalInstances());
    }
}

TYPED_TEST(shared_test, optional_interface) {
    using container_type = TypeParam;

    container_type container;
    container.template register_type<
        scope<shared>, storage<std::optional<Class>>, interfaces<IClass>>();
    AssertClass(container.template resolve<IClass*>());
    AssertClass(container.template resolve<IClass&>());
}

} // namespace dingo
