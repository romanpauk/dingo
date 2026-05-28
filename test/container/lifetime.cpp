//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#include <dingo/container.h>
#include <dingo/storage/shared.h>
#include <dingo/storage/unique.h>

#include <gtest/gtest.h>

#include <memory>
#include <optional>

#include "support/assert.h"
#include "support/class.h"
#include "support/containers.h"
#include "support/test.h"

namespace dingo {

template <typename T> struct lifetime_test : public test<T> {};
TYPED_TEST_SUITE(lifetime_test, container_types, );

TYPED_TEST(lifetime_test, shared_value_keeps_one_instance_until_container_dies) {
    using container_type = TypeParam;

    {
        container_type container;
        container.template register_type<scope<shared>, storage<Class>,
                                         interfaces<Class, IClass>>();

        AssertClass(*container.template resolve<Class*>());
        AssertClass(container.template resolve<Class&>());
        AssertClass(container.template resolve<IClass&>());

        ASSERT_EQ(Class::Constructor, 1);
        ASSERT_EQ(Class::Destructor, 0);
        ASSERT_EQ(Class::CopyConstructor, 0);
        ASSERT_EQ(Class::MoveConstructor, 0);
    }

    ASSERT_EQ(Class::Destructor, Class::GetTotalInstances());
}

TYPED_TEST(lifetime_test, shared_optional_tracks_stored_value_materialization) {
    using container_type = TypeParam;

    {
        container_type container;
        container.template register_type<scope<shared>,
                                         storage<std::optional<Class>>,
                                         interfaces<Class, IClass>>();

        AssertClass(container.template resolve<Class&>());
        AssertClass(*container.template resolve<std::optional<Class>&>());

        ASSERT_EQ(Class::Constructor, 1);
        ASSERT_EQ(Class::MoveConstructor, 1);
        ASSERT_EQ(Class::Destructor, 1);
        ASSERT_EQ(Class::CopyConstructor, 0);
    }

    ASSERT_EQ(Class::Destructor, Class::GetTotalInstances());
}

TYPED_TEST(lifetime_test, unique_value_resolution_materializes_temporary) {
    using container_type = TypeParam;

    {
        container_type container;
        container.template register_type<scope<unique>, storage<Class>>();

        AssertClass(container.template resolve<Class&&>());

        ASSERT_EQ(Class::Constructor, 1);
        ASSERT_EQ(Class::MoveConstructor, 2);
        ASSERT_EQ(Class::CopyConstructor, 0);
        ASSERT_EQ(Class::Destructor, Class::GetTotalInstances());
    }

    ASSERT_EQ(Class::Destructor, Class::GetTotalInstances());
}

TYPED_TEST(lifetime_test, unique_ptr_resolution_does_not_copy_storage) {
    using container_type = TypeParam;

    {
        container_type container;
        container.template register_type<scope<unique>,
                                         storage<std::unique_ptr<Class>>,
                                         interfaces<Class, IClass>>();

        AssertClass(*container.template resolve<std::unique_ptr<Class>>());

        ASSERT_EQ(Class::Constructor, 1);
        ASSERT_EQ(Class::MoveConstructor, 0);
        ASSERT_EQ(Class::CopyConstructor, 0);
        ASSERT_EQ(Class::Destructor, Class::GetTotalInstances());
    }

    ASSERT_EQ(Class::Destructor, Class::GetTotalInstances());
}

TYPED_TEST(lifetime_test, unique_optional_tracks_value_materialization) {
    using container_type = TypeParam;

    {
        container_type container;
        container.template register_type<scope<unique>,
                                         storage<std::optional<Class>>,
                                         interfaces<Class>>();

        AssertClass(*container.template resolve<std::optional<Class>>());

        ASSERT_EQ(Class::Constructor, 1);
        ASSERT_EQ(Class::MoveConstructor, 1);
        ASSERT_EQ(Class::CopyConstructor, 1);
        ASSERT_EQ(Class::Destructor, Class::GetTotalInstances());
    }

    ASSERT_EQ(Class::Destructor, Class::GetTotalInstances());
}

} // namespace dingo
