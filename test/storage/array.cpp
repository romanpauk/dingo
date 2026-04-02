//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#include <dingo/resolution/conversion_cache.h>
#include <dingo/container.h>
#include <dingo/factory/callable.h>
#include <dingo/factory/constructor.h>
#include <dingo/resolution/interface_storage_traits.h>
#include <dingo/type/rebind_type.h>
#include <dingo/rtti/rtti.h>
#include <dingo/rtti/typeid_provider.h>
#include <dingo/storage/external.h>
#include <dingo/storage/shared.h>
#include <dingo/storage/unique.h>
#include <dingo/registration/type_registration.h>

#include <gtest/gtest.h>

#include "support/assert.h"
#include "support/class.h"
#include "support/containers.h"
#include "support/test.h"

namespace dingo {

template <typename T> struct array_shared_test : public test<T> {};
TYPED_TEST_SUITE(array_shared_test, container_types, );

TYPED_TEST(array_shared_test, raw_array) {
    using container_type = TypeParam;

    {
        container_type container;
        container.template register_type<scope<shared>, storage<Class[2]>>();

        auto* value = container.template resolve<Class*>();
        ASSERT_NE(value, nullptr);
        AssertClass(value);
        AssertClass(value + 1);
        ASSERT_EQ(value, container.template resolve<Class*>());
        ASSERT_THROW(container.template resolve<Class&>(),
                     type_not_convertible_exception);

        ASSERT_EQ(Class::Constructor, 2);
        ASSERT_EQ(Class::Destructor, 0);
        ASSERT_EQ(Class::CopyConstructor, 0);
        ASSERT_EQ(Class::MoveConstructor, 0);
    }

    ASSERT_EQ(Class::Destructor, Class::GetTotalInstances());
}

TYPED_TEST(array_shared_test, raw_array_exact_extent_queries) {
    using container_type = TypeParam;

    {
        container_type container;
        container.template register_type<scope<shared>, storage<Class[2]>>();
        container.template register_type<scope<shared>, storage<Class[3]>>();

        auto* array2 = container.template resolve<Class(*)[2]>();
        auto* array3 = container.template resolve<Class(*)[3]>();

        ASSERT_NE(array2, nullptr);
        ASSERT_NE(array3, nullptr);
        AssertClass((*array2)[0]);
        AssertClass((*array2)[1]);
        AssertClass((*array3)[0]);
        AssertClass((*array3)[1]);
        AssertClass((*array3)[2]);

        auto& ref2 = container.template resolve<Class(&)[2]>();
        auto& ref3 = container.template resolve<Class(&)[3]>();
        ASSERT_EQ(array2, std::addressof(ref2));
        ASSERT_EQ(array3, std::addressof(ref3));

        ASSERT_THROW(container.template resolve<Class*>(),
                     type_ambiguous_exception);
    }

    ASSERT_EQ(Class::Destructor, Class::GetTotalInstances());
}

TYPED_TEST(array_shared_test, raw_array_nd) {
    using container_type = TypeParam;

    {
        container_type container;
        container.template register_type<scope<shared>, storage<Class[2][3]>>();

        auto* rows = container.template resolve<Class(*)[3]>();
        ASSERT_NE(rows, nullptr);
        AssertClass(rows[0][0]);
        AssertClass(rows[0][1]);
        AssertClass(rows[0][2]);
        AssertClass(rows[1][0]);
        AssertClass(rows[1][1]);
        AssertClass(rows[1][2]);

        auto* exact = container.template resolve<Class(*)[2][3]>();
        ASSERT_EQ(exact, reinterpret_cast<Class(*)[2][3]>(rows));
        auto& ref = container.template resolve<Class(&)[2][3]>();
        ASSERT_EQ(std::addressof(ref), exact);

        ASSERT_THROW(container.template resolve<Class*>(),
                     type_not_convertible_exception);

        ASSERT_EQ(Class::Constructor, 6);
        ASSERT_EQ(Class::Destructor, 0);
        ASSERT_EQ(Class::CopyConstructor, 0);
        ASSERT_EQ(Class::MoveConstructor, 0);
    }

    ASSERT_EQ(Class::Destructor, Class::GetTotalInstances());
}

TYPED_TEST(array_shared_test, raw_array_nd_exact_extent_queries) {
    using container_type = TypeParam;

    {
        container_type container;
        container.template register_type<scope<shared>, storage<Class[2][3]>>();
        container.template register_type<scope<shared>, storage<Class[4][3]>>();

        auto* array2 = container.template resolve<Class(*)[2][3]>();
        auto* array4 = container.template resolve<Class(*)[4][3]>();

        ASSERT_NE(array2, nullptr);
        ASSERT_NE(array4, nullptr);
        AssertClass((*array2)[0][0]);
        AssertClass((*array2)[1][2]);
        AssertClass((*array4)[0][0]);
        AssertClass((*array4)[3][2]);

        auto& ref2 = container.template resolve<Class(&)[2][3]>();
        auto& ref4 = container.template resolve<Class(&)[4][3]>();
        ASSERT_EQ(array2, std::addressof(ref2));
        ASSERT_EQ(array4, std::addressof(ref4));

        ASSERT_THROW(container.template resolve<Class(*)[3]>(),
                     type_ambiguous_exception);
    }

    ASSERT_EQ(Class::Destructor, Class::GetTotalInstances());
}

TYPED_TEST(array_shared_test, shared_ptr_array) {
    using container_type = TypeParam;

    {
        container_type container;
        container.template register_type<
            scope<shared>, storage<std::shared_ptr<Class[]>>>(
            callable([] { return std::shared_ptr<Class[]>(new Class[2]); }));

        auto value = container.template resolve<std::shared_ptr<Class[]>>();
        ASSERT_NE(value, nullptr);
        AssertClass(value[0]);
        AssertClass(value[1]);

        auto& ref = container.template resolve<std::shared_ptr<Class[]>&>();
        ASSERT_EQ(ref.get(), value.get());
        auto* ptr = container.template resolve<Class*>();
        ASSERT_EQ(ptr, value.get());
        AssertClass(ptr);
        AssertClass(ptr + 1);

        ASSERT_EQ(Class::Constructor, 2);
        ASSERT_EQ(Class::Destructor, 0);
        ASSERT_EQ(Class::CopyConstructor, 0);
        ASSERT_EQ(Class::MoveConstructor, 0);
    }

    ASSERT_EQ(Class::Destructor, Class::GetTotalInstances());
}

TYPED_TEST(array_shared_test, unique_ptr_nested_array) {
    using container_type = TypeParam;

    {
        container_type container;
        container.template register_type<
            scope<shared>, storage<std::unique_ptr<Class[][2]>>>(
            callable([] { return std::unique_ptr<Class[][2]>(new Class[3][2]); }));

        auto& value = container.template resolve<std::unique_ptr<Class[][2]>&>();
        ASSERT_NE(value, nullptr);
        AssertClass(value[0][0]);
        AssertClass(value[0][1]);
        AssertClass(value[1][0]);
        AssertClass(value[1][1]);
        AssertClass(value[2][0]);
        AssertClass(value[2][1]);

        auto* rows = container.template resolve<Class(*)[2]>();
        ASSERT_EQ(rows, value.get());
        AssertClass(rows[0][0]);
        AssertClass(rows[2][1]);

        ASSERT_THROW(container.template resolve<Class*>(),
                     type_not_convertible_exception);
    }

    ASSERT_EQ(Class::Destructor, Class::GetTotalInstances());
}

TYPED_TEST(array_shared_test, shared_ptr_nested_array) {
    using container_type = TypeParam;

    {
        container_type container;
        container.template register_type<
            scope<shared>, storage<std::shared_ptr<Class[][2]>>>(
            callable([] { return std::shared_ptr<Class[][2]>(new Class[3][2]); }));

        auto value = container.template resolve<std::shared_ptr<Class[][2]>>();
        ASSERT_NE(value, nullptr);
        AssertClass(value[0][0]);
        AssertClass(value[0][1]);
        AssertClass(value[1][0]);
        AssertClass(value[1][1]);
        AssertClass(value[2][0]);
        AssertClass(value[2][1]);

        auto& ref = container.template resolve<std::shared_ptr<Class[][2]>&>();
        ASSERT_EQ(ref.get(), value.get());

        auto* rows = container.template resolve<Class(*)[2]>();
        ASSERT_EQ(rows, value.get());
        AssertClass(rows[0][0]);
        AssertClass(rows[2][1]);

        ASSERT_THROW(container.template resolve<Class*>(),
                     type_not_convertible_exception);
    }

    ASSERT_EQ(Class::Destructor, Class::GetTotalInstances());
}

TYPED_TEST(array_shared_test, shared_ptr_nd_array) {
    using container_type = TypeParam;

    {
        container_type container;
        container.template register_type<
            scope<shared>, storage<std::shared_ptr<Class[][2][3]>>>(
            callable([] {
                return std::shared_ptr<Class[][2][3]>(new Class[4][2][3]);
            }));

        auto value = container.template resolve<std::shared_ptr<Class[][2][3]>>();
        ASSERT_NE(value, nullptr);
        AssertClass(value[0][0][0]);
        AssertClass(value[0][1][2]);
        AssertClass(value[3][0][1]);
        AssertClass(value[3][1][2]);

        auto& ref =
            container.template resolve<std::shared_ptr<Class[][2][3]>&>();
        ASSERT_EQ(ref.get(), value.get());

        auto* blocks = container.template resolve<Class(*)[2][3]>();
        ASSERT_EQ(blocks, value.get());
        AssertClass(blocks[0][0][0]);
        AssertClass(blocks[3][1][2]);

        ASSERT_THROW(container.template resolve<Class(*)[3]>(),
                     type_not_found_exception);
        ASSERT_THROW(container.template resolve<Class*>(),
                     type_not_convertible_exception);
    }

    ASSERT_EQ(Class::Destructor, Class::GetTotalInstances());
}

template <typename T> struct array_unique_test : public test<T> {};
TYPED_TEST_SUITE(array_unique_test, container_types, );

TYPED_TEST(array_unique_test, raw_array) {
    using container_type = TypeParam;

    {
        container_type container;
        container.template register_type<scope<unique>, storage<Class[2]>>();

        auto* raw = container.template resolve<Class*>();
        ASSERT_NE(raw, nullptr);
        AssertClass(raw);
        AssertClass(raw + 1);
        delete[] raw;

        auto unique = container.template resolve<std::unique_ptr<Class[]>>();
        ASSERT_NE(unique, nullptr);
        AssertClass(unique[0]);
        AssertClass(unique[1]);

        auto shared = container.template resolve<std::shared_ptr<Class[]>>();
        ASSERT_NE(shared, nullptr);
        AssertClass(shared[0]);
        AssertClass(shared[1]);

        ASSERT_EQ(Class::Constructor, 6);
        ASSERT_EQ(Class::CopyConstructor, 0);
        ASSERT_EQ(Class::MoveConstructor, 0);
        ASSERT_EQ(Class::Destructor, 2);
    }

    ASSERT_EQ(Class::Destructor, Class::GetTotalInstances());
}

TYPED_TEST(array_unique_test, raw_array_exact_extent_queries) {
    using container_type = TypeParam;

    {
        container_type container;
        container.template register_type<scope<unique>, storage<Class[2]>>();
        container.template register_type<scope<unique>, storage<Class[3]>>();

        auto* array2 = container.template resolve<Class(*)[2]>();
        auto* array3 = container.template resolve<Class(*)[3]>();

        ASSERT_NE(array2, nullptr);
        ASSERT_NE(array3, nullptr);
        AssertClass((*array2)[0]);
        AssertClass((*array2)[1]);
        AssertClass((*array3)[0]);
        AssertClass((*array3)[1]);
        AssertClass((*array3)[2]);

        ASSERT_THROW(container.template resolve<Class*>(),
                     type_ambiguous_exception);

        delete[] reinterpret_cast<Class*>(array2);
        delete[] reinterpret_cast<Class*>(array3);
    }

    ASSERT_EQ(Class::Destructor, Class::GetTotalInstances());
}

TYPED_TEST(array_unique_test, raw_array_nd) {
    using container_type = TypeParam;

    {
        container_type container;
        container.template register_type<scope<unique>, storage<Class[2][3]>>();

        auto* rows = container.template resolve<Class(*)[3]>();
        ASSERT_NE(rows, nullptr);
        AssertClass(rows[0][0]);
        AssertClass(rows[0][1]);
        AssertClass(rows[0][2]);
        AssertClass(rows[1][0]);
        AssertClass(rows[1][1]);
        AssertClass(rows[1][2]);
        delete[] rows;

        auto unique = container.template resolve<std::unique_ptr<Class[][3]>>();
        ASSERT_NE(unique, nullptr);
        AssertClass(unique[0][0]);
        AssertClass(unique[0][2]);
        AssertClass(unique[1][0]);
        AssertClass(unique[1][2]);

        auto shared = container.template resolve<std::shared_ptr<Class[][3]>>();
        ASSERT_NE(shared, nullptr);
        AssertClass(shared[0][0]);
        AssertClass(shared[1][2]);

        ASSERT_EQ(Class::Constructor, 18);
        ASSERT_EQ(Class::CopyConstructor, 0);
        ASSERT_EQ(Class::MoveConstructor, 0);
        ASSERT_EQ(Class::Destructor, 6);
    }

    ASSERT_EQ(Class::Destructor, Class::GetTotalInstances());
}

TYPED_TEST(array_unique_test, raw_array_nd_exact_extent_queries) {
    using container_type = TypeParam;

    {
        container_type container;
        container.template register_type<scope<unique>, storage<Class[2][3]>>();
        container.template register_type<scope<unique>, storage<Class[4][3]>>();

        auto* array2 = container.template resolve<Class(*)[2][3]>();
        auto* array4 = container.template resolve<Class(*)[4][3]>();

        ASSERT_NE(array2, nullptr);
        ASSERT_NE(array4, nullptr);
        AssertClass((*array2)[0][0]);
        AssertClass((*array2)[1][2]);
        AssertClass((*array4)[0][0]);
        AssertClass((*array4)[3][2]);

        ASSERT_THROW(container.template resolve<Class(*)[3]>(),
                     type_ambiguous_exception);

        delete[] reinterpret_cast<Class(*)[3]>(array2);
        delete[] reinterpret_cast<Class(*)[3]>(array4);
    }

    ASSERT_EQ(Class::Destructor, Class::GetTotalInstances());
}

TYPED_TEST(array_unique_test, unique_ptr_array) {
    using container_type = TypeParam;

    {
        container_type container;
        container.template register_type<
            scope<unique>, storage<std::unique_ptr<Class[]>>>(
            callable([] { return std::unique_ptr<Class[]>(new Class[2]); }));

        auto value = container.template resolve<std::unique_ptr<Class[]>>();
        ASSERT_NE(value, nullptr);
        AssertClass(value[0]);
        AssertClass(value[1]);

        auto shared = container.template resolve<std::shared_ptr<Class[]>>();
        ASSERT_NE(shared, nullptr);
        AssertClass(shared[0]);
        AssertClass(shared[1]);

        ASSERT_THROW(container.template resolve<Class*>(),
                     type_not_convertible_exception);
    }

    ASSERT_EQ(Class::Destructor, Class::GetTotalInstances());
}

TYPED_TEST(array_unique_test, unique_ptr_nested_array) {
    using container_type = TypeParam;

    {
        container_type container;
        container.template register_type<
            scope<unique>, storage<std::unique_ptr<Class[][2]>>>(
            callable([] { return std::unique_ptr<Class[][2]>(new Class[3][2]); }));

        auto value = container.template resolve<std::unique_ptr<Class[][2]>>();
        ASSERT_NE(value, nullptr);
        AssertClass(value[0][0]);
        AssertClass(value[0][1]);
        AssertClass(value[1][0]);
        AssertClass(value[1][1]);
        AssertClass(value[2][0]);
        AssertClass(value[2][1]);

        auto shared = container.template resolve<std::shared_ptr<Class[][2]>>();
        ASSERT_NE(shared, nullptr);
        AssertClass(shared[0][0]);
        AssertClass(shared[2][1]);

        ASSERT_THROW(container.template resolve<Class(*)[2]>(),
                     type_not_found_exception);
        ASSERT_THROW(container.template resolve<Class*>(),
                     type_not_convertible_exception);
    }

    ASSERT_EQ(Class::Destructor, Class::GetTotalInstances());
}

TYPED_TEST(array_unique_test, unique_ptr_nd_array) {
    using container_type = TypeParam;

    {
        container_type container;
        container.template register_type<
            scope<unique>, storage<std::unique_ptr<Class[][2][3]>>>(
            callable([] {
                return std::unique_ptr<Class[][2][3]>(new Class[4][2][3]);
            }));

        auto value = container.template resolve<std::unique_ptr<Class[][2][3]>>();
        ASSERT_NE(value, nullptr);
        AssertClass(value[0][0][0]);
        AssertClass(value[0][1][2]);
        AssertClass(value[3][0][1]);
        AssertClass(value[3][1][2]);

        auto shared = container.template resolve<std::shared_ptr<Class[][2][3]>>();
        ASSERT_NE(shared, nullptr);
        AssertClass(shared[0][0][0]);
        AssertClass(shared[3][1][2]);

        ASSERT_THROW(container.template resolve<Class(*)[2][3]>(),
                     type_not_found_exception);
        ASSERT_THROW(container.template resolve<Class(*)[3]>(),
                     type_not_found_exception);
    }

    ASSERT_EQ(Class::Destructor, Class::GetTotalInstances());
}

template <typename T> struct array_external_test : public test<T> {};
TYPED_TEST_SUITE(array_external_test, container_types, );

TYPED_TEST(array_external_test, raw_array) {
    using container_type = TypeParam;
    Class values[2];

    container_type container;
    container.template register_type<scope<external>, storage<Class[2]>>(values);

    auto* value = container.template resolve<Class*>();
    ASSERT_EQ(value, values);
    AssertClass(value);
    AssertClass(value + 1);

    auto* exact = container.template resolve<Class(*)[2]>();
    ASSERT_EQ(exact, &values);
    auto& ref = container.template resolve<Class(&)[2]>();
    ASSERT_EQ(std::addressof(ref), &values);

    ASSERT_THROW(container.template resolve<Class&>(),
                 type_not_convertible_exception);
}

TYPED_TEST(array_external_test, raw_array_exact_extent_queries) {
    using container_type = TypeParam;
    Class values2[2];
    Class values3[3];

    container_type container;
    container.template register_type<scope<external>, storage<Class[2]>>(values2);
    container.template register_type<scope<external>, storage<Class[3]>>(values3);

    auto* array2 = container.template resolve<Class(*)[2]>();
    auto* array3 = container.template resolve<Class(*)[3]>();

    ASSERT_EQ(array2, &values2);
    ASSERT_EQ(array3, &values3);
    AssertClass((*array2)[0]);
    AssertClass((*array2)[1]);
    AssertClass((*array3)[0]);
    AssertClass((*array3)[1]);
    AssertClass((*array3)[2]);

    auto& ref2 = container.template resolve<Class(&)[2]>();
    auto& ref3 = container.template resolve<Class(&)[3]>();
    ASSERT_EQ(std::addressof(ref2), &values2);
    ASSERT_EQ(std::addressof(ref3), &values3);

    ASSERT_THROW(container.template resolve<Class*>(),
                 type_ambiguous_exception);
}

TYPED_TEST(array_external_test, raw_array_nd) {
    using container_type = TypeParam;
    Class values[2][3];

    container_type container;
    container.template register_type<scope<external>, storage<Class[2][3]>>(values);

    auto* rows = container.template resolve<Class(*)[3]>();
    ASSERT_EQ(rows, values);
    AssertClass(rows[0][0]);
    AssertClass(rows[0][1]);
    AssertClass(rows[0][2]);
    AssertClass(rows[1][0]);
    AssertClass(rows[1][1]);
    AssertClass(rows[1][2]);

    auto* exact = container.template resolve<Class(*)[2][3]>();
    ASSERT_EQ(exact, &values);
    auto& ref = container.template resolve<Class(&)[2][3]>();
    ASSERT_EQ(std::addressof(ref), &values);

    ASSERT_THROW(container.template resolve<Class*>(),
                 type_not_convertible_exception);
}

TYPED_TEST(array_external_test, raw_array_nd_exact_extent_queries) {
    using container_type = TypeParam;
    Class values2[2][3];
    Class values4[4][3];

    container_type container;
    container.template register_type<scope<external>, storage<Class[2][3]>>(values2);
    container.template register_type<scope<external>, storage<Class[4][3]>>(values4);

    auto* array2 = container.template resolve<Class(*)[2][3]>();
    auto* array4 = container.template resolve<Class(*)[4][3]>();

    ASSERT_EQ(array2, &values2);
    ASSERT_EQ(array4, &values4);
    AssertClass((*array2)[0][0]);
    AssertClass((*array2)[1][2]);
    AssertClass((*array4)[0][0]);
    AssertClass((*array4)[3][2]);

    auto& ref2 = container.template resolve<Class(&)[2][3]>();
    auto& ref4 = container.template resolve<Class(&)[4][3]>();
    ASSERT_EQ(std::addressof(ref2), &values2);
    ASSERT_EQ(std::addressof(ref4), &values4);

    ASSERT_THROW(container.template resolve<Class(*)[3]>(),
                 type_ambiguous_exception);
}

template <typename T> struct array_construct_test : public test<T> {};
TYPED_TEST_SUITE(array_construct_test, container_types, );

TYPED_TEST(array_construct_test, raw_array_dependencies) {
    using container_type = TypeParam;

    struct ptr_consumer {
        Class* value;
        explicit ptr_consumer(Class* init) : value(init) {}
    };

    struct exact_array_consumer {
        Class (*value)[2];
        Class (*ref_value)[2];

        exact_array_consumer(Class (*init)[2], Class (&ref)[2])
            : value(init), ref_value(&ref) {}
    };

    container_type container;
    container.template register_type<scope<shared>, storage<Class[2]>>();

    auto ptr = container
                   .template construct<ptr_consumer,
                                       constructor<ptr_consumer(Class*)>>();
    ASSERT_EQ(ptr.value, container.template resolve<Class*>());
    AssertClass(ptr.value);
    AssertClass(ptr.value + 1);

    auto exact = container.template construct<
        exact_array_consumer,
        constructor<exact_array_consumer(Class(*)[2], Class(&)[2])>>();
    ASSERT_EQ(exact.value, container.template resolve<Class(*)[2]>());
    ASSERT_EQ(exact.ref_value,
              std::addressof(container.template resolve<Class(&)[2]>()));
    AssertClass((*exact.value)[0]);
    AssertClass((*exact.value)[1]);
}

TYPED_TEST(array_construct_test, raw_array_nd_dependencies) {
    using container_type = TypeParam;

    struct consumer {
        Class (*rows)[3];
        Class (*value)[2][3];
        Class (*ref_value)[2][3];

        consumer(Class (*init_rows)[3], Class (*init_value)[2][3],
                 Class (&ref)[2][3])
            : rows(init_rows), value(init_value), ref_value(&ref) {}
    };

    container_type container;
    container.template register_type<scope<shared>, storage<Class[2][3]>>();

    auto nd = container
                  .template construct<consumer,
                                      constructor<consumer(
                                          Class(*)[3], Class(*)[2][3],
                                          Class(&)[2][3])>>();
    ASSERT_EQ(nd.rows, container.template resolve<Class(*)[3]>());
    ASSERT_EQ(nd.value, container.template resolve<Class(*)[2][3]>());
    ASSERT_EQ(nd.ref_value,
              std::addressof(container.template resolve<Class(&)[2][3]>()));
    AssertClass(nd.rows[0][0]);
    AssertClass(nd.rows[1][2]);
}

TYPED_TEST(array_construct_test, unique_array_dependency) {
    using container_type = TypeParam;

    struct consumer {
        std::unique_ptr<Class[]> value;
        explicit consumer(std::unique_ptr<Class[]> init)
            : value(std::move(init)) {}
    };

    container_type container;
    container.template register_type<scope<unique>, storage<Class[2]>>();

    auto value = container.template construct<
        consumer, constructor<consumer(std::unique_ptr<Class[]>)>>();
    ASSERT_NE(value.value, nullptr);
    AssertClass(value.value[0]);
    AssertClass(value.value[1]);
}

TYPED_TEST(array_construct_test, shared_array_dependency) {
    using container_type = TypeParam;

    struct consumer {
        std::shared_ptr<Class[]> value;
        explicit consumer(std::shared_ptr<Class[]> init)
            : value(std::move(init)) {}
    };

    container_type container;
    container.template register_type<scope<shared>,
                                     storage<std::shared_ptr<Class[]>>>(
        callable([] { return std::shared_ptr<Class[]>(new Class[2]); }));

    auto value = container.template construct<
        consumer, constructor<consumer(std::shared_ptr<Class[]>)>>();
    ASSERT_NE(value.value, nullptr);
    AssertClass(value.value[0]);
    AssertClass(value.value[1]);
    ASSERT_EQ(value.value.get(), container.template resolve<Class*>());
}

TYPED_TEST(array_construct_test, nested_shared_array_dependency) {
    using container_type = TypeParam;

    struct consumer {
        std::shared_ptr<Class[][2][3]> value;
        Class (*blocks)[2][3];

        consumer(std::shared_ptr<Class[][2][3]> init, Class (*rows)[2][3])
            : value(std::move(init)), blocks(rows) {}
    };

    container_type container;
    container.template register_type<scope<shared>,
                                     storage<std::shared_ptr<Class[][2][3]>>>(
        callable([] {
            return std::shared_ptr<Class[][2][3]>(new Class[4][2][3]);
        }));

    auto value = container.template construct<
        consumer,
        constructor<consumer(std::shared_ptr<Class[][2][3]>,
                             Class(*)[2][3])>>();
    ASSERT_NE(value.value, nullptr);
    ASSERT_EQ(value.blocks, value.value.get());
    AssertClass(value.blocks[0][0][0]);
    AssertClass(value.blocks[3][1][2]);
}

TEST(array_test, registration_traits) {
    struct I {
        virtual ~I() = default;
    };
    struct A : I {};

    using shared_nested_array_registration =
        type_registration<scope<shared>, storage<std::shared_ptr<A[][2]>>>;
    using unique_nested_array_registration =
        type_registration<scope<unique>, storage<std::unique_ptr<A[][2]>>>;
    using shared_nd_nested_array_registration =
        type_registration<scope<shared>, storage<std::shared_ptr<A[][2][3]>>>;
    using shared_nd_raw_array_registration =
        type_registration<scope<shared>, storage<A[2][3]>>;

    static_assert(std::is_same_v<
                  typename shared_nested_array_registration::interface_type,
                  interfaces<A[2], A>>);
    static_assert(std::is_same_v<
                  typename unique_nested_array_registration::interface_type,
                  interfaces<A>>);
    static_assert(std::is_same_v<
                  typename shared_nd_nested_array_registration::interface_type,
                  interfaces<A[2][3], A>>);
    static_assert(std::is_same_v<
                  typename shared_nd_raw_array_registration::interface_type,
                  interfaces<A[3], A[2][3], A>>);
    static_assert(detail::is_array_like_type_v<A[2]>);
    static_assert(detail::is_array_like_type_v<A[2][3]>);
    static_assert(detail::is_array_like_type_v<std::unique_ptr<A[]>>);
    static_assert(detail::is_array_like_type_v<std::shared_ptr<A[]>>);
    static_assert(detail::is_array_like_type_v<std::shared_ptr<A[][2]>>);
    static_assert(detail::is_array_like_type_v<std::shared_ptr<A[][2][3]>>);
    static_assert(
        std::is_same_v<detail::array_like_exact_interface_type_t<A[2][3]>,
                       A[2][3]>);
    static_assert(
        std::is_same_v<detail::array_like_exact_interface_type_t<
                           std::shared_ptr<A[][2][3]>>,
                       A[2][3]>);
    static_assert(!detail::is_array_like_type_v<A>);
    static_assert(!detail::is_array_like_type_v<std::shared_ptr<A>>);
}

TEST(array_test, recursive_leaf_and_rebind_traits) {
    struct I {
        virtual ~I() = default;
    };
    struct A : I {};

    static_assert(std::is_same_v<leaf_type_t<A[2]>, A>);
    static_assert(std::is_same_v<leaf_type_t<A[2][3]>, A>);
    static_assert(std::is_same_v<leaf_type_t<std::unique_ptr<A[]>>, A>);
    static_assert(std::is_same_v<leaf_type_t<std::unique_ptr<A[][2]>>, A>);
    static_assert(std::is_same_v<leaf_type_t<std::unique_ptr<A[][2][3]>>, A>);
    static_assert(std::is_same_v<leaf_type_t<std::shared_ptr<A[]>>, A>);
    static_assert(std::is_same_v<leaf_type_t<std::shared_ptr<A[][2]>>, A>);
    static_assert(std::is_same_v<leaf_type_t<std::shared_ptr<A[][2][3]>>, A>);
    static_assert(std::is_same_v<rebind_leaf_t<A[2], I>, I[2]>);
    static_assert(std::is_same_v<rebind_leaf_t<A[2][3], I>, I[2][3]>);
    static_assert(
        std::is_same_v<rebind_leaf_t<std::unique_ptr<A[]>, I>,
                       std::unique_ptr<I[]>>);
    static_assert(
        std::is_same_v<rebind_leaf_t<std::unique_ptr<A[][2]>, I>,
                       std::unique_ptr<I[][2]>>);
    static_assert(
        std::is_same_v<rebind_leaf_t<std::shared_ptr<A[]>, I>,
                       std::shared_ptr<I[]>>);
    static_assert(
        std::is_same_v<rebind_leaf_t<std::shared_ptr<A[][2]>, I>,
                       std::shared_ptr<I[][2]>>);
    static_assert(
        std::is_same_v<rebind_leaf_t<std::shared_ptr<A[][2][3]>, I>,
                       std::shared_ptr<I[][2][3]>>);
}

TEST(array_test, normalized_type_trait) {
    struct A {};

    static_assert(std::is_same_v<normalized_type_t<A (*)[20]>, A[20]>);
    static_assert(std::is_same_v<normalized_type_t<A (&)[20]>, A[20]>);
    static_assert(std::is_same_v<normalized_type_t<A (*)[2][20]>, A[2][20]>);
    static_assert(std::is_same_v<normalized_type_t<A (&)[2][20]>, A[2][20]>);
}

} // namespace dingo
