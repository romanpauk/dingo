//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#include <dingo/container.h>
#include <dingo/factory/callable.h>
#include <dingo/storage/shared.h>
#include <dingo/storage/shared_cyclical.h>
#include <dingo/storage/unique.h>

#include <gtest/gtest.h>

#include "assert.h"
#include "class.h"
#include "containers.h"
#include "test.h"

namespace dingo {
template <typename T> struct multibindings_test : public test<T> {};
TYPED_TEST_SUITE(multibindings_test, container_types, );

TYPED_TEST(multibindings_test, multiple_interfaces_shared_value) {
    using container_type = TypeParam;

    container_type container;
    container.template register_type<scope<shared>, storage<Class>,
                                     interfaces<IClass, IClass1, IClass2>>();

    ASSERT_TRUE(dynamic_cast<Class*>(&container.template resolve<IClass&>()));
    ASSERT_TRUE(dynamic_cast<Class*>(&container.template resolve<IClass1&>()));
    ASSERT_TRUE(dynamic_cast<Class*>(container.template resolve<IClass2*>()));
}

TYPED_TEST(multibindings_test, multiple_interfaces_shared_cyclical_value) {
    using container_type = TypeParam;

    container_type container;
    container.template register_type<scope<shared_cyclical>, storage<Class>,
                                     interfaces<IClass1, IClass2>>();

    ASSERT_TRUE(dynamic_cast<Class*>(&container.template resolve<IClass1&>()));
    ASSERT_TRUE(dynamic_cast<Class*>(container.template resolve<IClass2*>()));
}

TYPED_TEST(multibindings_test, multiple_interfaces_shared_shared_ptr) {
    using container_type = TypeParam;

    container_type container;
    container
        .template register_type<scope<shared>, storage<std::shared_ptr<Class>>,
                                interfaces<IClass, IClass1, IClass2>>();

    ASSERT_TRUE(dynamic_cast<Class*>(&container.template resolve<IClass&>()));
    ASSERT_TRUE(dynamic_cast<Class*>(
        container.template resolve<std::shared_ptr<IClass>>().get()));
    ASSERT_TRUE(dynamic_cast<Class*>(&container.template resolve<IClass1&>()));
    ASSERT_TRUE(dynamic_cast<Class*>(
        container.template resolve<std::shared_ptr<IClass1>>().get()));
    ASSERT_TRUE(dynamic_cast<Class*>(&container.template resolve<IClass2&>()));
}

TYPED_TEST(multibindings_test, multiple_interfaces_shared_cyclical_shared_ptr) {
    using container_type = TypeParam;

    container_type container;
    container.template register_type<scope<shared_cyclical>,
                                     storage<std::shared_ptr<Class>>,
                                     interfaces<IClass1, IClass2>>();

    // TODO: virtual base issues with cyclical_shared
    // BOOST_TEST(dynamic_cast<C*>(&container.resolve< IClass& >()));
    // BOOST_TEST(dynamic_cast<C*>(container.resolve< std::shared_ptr< IClass >&
    // >().get()));
    ASSERT_TRUE(dynamic_cast<Class*>(&container.template resolve<IClass1&>()));
    ASSERT_TRUE(dynamic_cast<Class*>(
        container.template resolve<std::shared_ptr<IClass1>>().get()));
    ASSERT_TRUE(dynamic_cast<Class*>(&container.template resolve<IClass2&>()));
}

TYPED_TEST(multibindings_test, multiple_interfaces_shared_unique_ptr) {
    using container_type = TypeParam;

    container_type container;
    container
        .template register_type<scope<shared>, storage<std::unique_ptr<Class>>,
                                interfaces<IClass, IClass1, IClass2>>();

    ASSERT_TRUE(dynamic_cast<Class*>(container.template resolve<IClass*>()));
    ASSERT_TRUE(dynamic_cast<Class*>(container.template resolve<IClass1*>()));
    ASSERT_TRUE(dynamic_cast<Class*>(container.template resolve<IClass2*>()));
}

TYPED_TEST(multibindings_test, multiple_interfaces_unique_shared_ptr) {
    using container_type = TypeParam;

    container_type container;
    container
        .template register_type<scope<unique>, storage<std::shared_ptr<Class>>,
                                interfaces<IClass, IClass1, IClass2>>();

    ASSERT_TRUE(dynamic_cast<Class*>(
        container.template resolve<std::shared_ptr<IClass>>().get()));
    ASSERT_TRUE(dynamic_cast<Class*>(
        container.template resolve<std::shared_ptr<IClass1>>().get()));
    ASSERT_TRUE(dynamic_cast<Class*>(
        container.template resolve<std::shared_ptr<IClass2>>().get()));
}

TYPED_TEST(multibindings_test, multiple_interfaces_unique_unique_ptr) {
    using container_type = TypeParam;

    container_type container;
    container
        .template register_type<scope<unique>, storage<std::unique_ptr<Class>>,
                                interfaces<IClass, IClass1, IClass2>>();

    ASSERT_TRUE(dynamic_cast<Class*>(
        container.template resolve<std::unique_ptr<IClass>>().get()));
    ASSERT_TRUE(dynamic_cast<Class*>(
        container.template resolve<std::unique_ptr<IClass1>>().get()));
    ASSERT_TRUE(dynamic_cast<Class*>(
        container.template resolve<std::unique_ptr<IClass2>>().get()));
}

TYPED_TEST(multibindings_test, register_type_collection_shared_value) {
    using container_type = TypeParam;

    container_type container;
    container.template register_type_collection<
        scope<unique>, storage<std::vector<IClass*>>>();

    container.template register_type<scope<shared>, storage<ClassTag<0>>,
                                     interfaces<IClass>>();
    container.template register_type<scope<shared>, storage<ClassTag<1>>,
                                     interfaces<IClass>>();

    auto classes = container.template resolve<std::vector<IClass*>>();
    ASSERT_EQ(classes.size(), 2);
}

TYPED_TEST(multibindings_test, register_type_collection_shared_ptr) {
    using container_type = TypeParam;

    container_type container;
    container.template register_type_collection<
    scope<unique>, storage<std::vector<std::shared_ptr<IClass>>>>();
    container.template register_type<scope<shared>,
                                     storage<std::shared_ptr<ClassTag<0>>>,
                                     interfaces<IClass>>();
    container.template register_type<scope<shared>,
                                     storage<std::shared_ptr<ClassTag<1>>>,
                                     interfaces<IClass>>();

    auto classes =
        container.template resolve<std::vector<std::shared_ptr<IClass>>>();
    ASSERT_EQ(classes.size(), 2);
}

TYPED_TEST(multibindings_test, register_type_collection_unique_ptr) {
    using container_type = TypeParam;

    container_type container;
    container.template register_type_collection<
        scope<unique>, storage<std::vector<std::unique_ptr<IClass>>>>();
    container.template register_type<scope<unique>,
                                     storage<std::unique_ptr<ClassTag<0>>>,
                                     interfaces<IClass>>();
    container.template register_type<scope<unique>,
                                     storage<std::unique_ptr<ClassTag<1>>>,
                                     interfaces<IClass>>();

    std::vector<std::unique_ptr<IClass>> classes =
        container.template resolve<std::vector<std::unique_ptr<IClass>>>();
    ASSERT_EQ(classes.size(), 2);

    ASSERT_THROW(container.template resolve<std::unique_ptr<IClass*>>(), type_ambiguous_exception);
}

TYPED_TEST(multibindings_test, register_type_collection_mapping_shared_ptr) {
    using container_type = TypeParam;

    container_type container;
    container.template register_type_collection<
        scope<unique>,
        storage<std::map<std::type_index, std::shared_ptr<IClass>>>>(
        [](auto& collection, auto&& value) {
            collection.emplace(typeid(*value.get()), std::move(value));
        });
    container.template register_type<scope<unique>,
                                     storage<std::shared_ptr<ClassTag<0>>>,
                                     interfaces<IClass>>();
    container.template register_type<scope<unique>,
                                     storage<std::shared_ptr<ClassTag<1>>>,
                                     interfaces<IClass>>();

    std::map<std::type_index, std::shared_ptr<IClass>> classes =
        container.template resolve<
            std::map<std::type_index, std::shared_ptr<IClass>>>();
    ASSERT_EQ(classes.size(), 2);
}

TYPED_TEST(multibindings_test, register_type_collection_mapping_unique_ptr) {
    using container_type = TypeParam;

    container_type container;
    container.template register_type_collection<
        scope<unique>,
        storage<std::map<std::type_index, std::unique_ptr<IClass>>>>(
        [](auto& collection, auto&& value) {
            collection.emplace(typeid(*value.get()), std::move(value));
        });
    container.template register_type<scope<unique>,
                                     storage<std::unique_ptr<ClassTag<0>>>,
                                     interfaces<IClass>>();
    container.template register_type<scope<unique>,
                                     storage<std::unique_ptr<ClassTag<1>>>,
                                     interfaces<IClass>>();

    std::map<std::type_index, std::unique_ptr<IClass>> classes =
        container.template resolve<
            std::map<std::type_index, std::unique_ptr<IClass>>>();
    ASSERT_EQ(classes.size(), 2);
}

} // namespace dingo
