#include <dingo/container.h>
#include <dingo/storage/unique.h>

#include <gtest/gtest.h>

#include "assert.h"
#include "class.h"
#include "containers.h"

namespace dingo {
template <typename T> struct unique_test : public testing::Test {};
TYPED_TEST_SUITE(unique_test, container_types);

TYPED_TEST(unique_test, value) {
    using container_type = TypeParam;
    struct unique_value {};
    {
        typedef Class<unique_value, __COUNTER__> C;

        {
            container_type container;
            container.template register_binding<storage<unique, C>>();
            AssertTypeNotConvertible<C, type_list<C&, C*>>(container);
            {
                AssertClass(container.template resolve<C&&>());
                ASSERT_EQ(C::Constructor, 1);
                ASSERT_EQ(C::MoveConstructor, 2); // TODO
                ASSERT_EQ(C::CopyConstructor, 0);
            }

            ASSERT_EQ(C::Destructor, C::GetTotalInstances());
        }

        ASSERT_EQ(C::Destructor, C::GetTotalInstances());
    }

    {
        typedef Class<unique_value, __COUNTER__> C;

        {
            container_type container;
            container.template register_binding<storage<unique, C>>();

            {
                AssertClass(container.template resolve<C>());
                ASSERT_EQ(C::Constructor, 1);
                ASSERT_EQ(C::MoveConstructor, 1); // TODO
                ASSERT_EQ(C::CopyConstructor, 1);
            }

            ASSERT_EQ(C::Destructor, C::GetTotalInstances());
        }

        ASSERT_EQ(C::Destructor, C::GetTotalInstances());
    }

    {
        typedef Class<unique_value, __COUNTER__> C;

        {
            container_type container;
            container.template register_binding<storage<unique, std::unique_ptr<C>>, C, IClass>();
            AssertTypeNotConvertible<C, type_list<C, C&, C&&, C*>>(container);
            {
                AssertClass(*container.template resolve<std::unique_ptr<C>>());
                ASSERT_EQ(C::Constructor, 1);
                ASSERT_EQ(C::MoveConstructor, 0);
                ASSERT_EQ(C::CopyConstructor, 0);
            }

            ASSERT_EQ(C::Destructor, C::GetTotalInstances());
        }

        ASSERT_EQ(C::Destructor, C::GetTotalInstances());
    }
}

TYPED_TEST(unique_test, pointer) {
    using container_type = TypeParam;

    struct unique_pointer {};
    /*
        // TODO: if something is stored as a pointer and is unique, we have to
       force pointer ownership transfer
        // by disallowing anything else than pointer resolution

        {
            typedef Class< unique_pointer, __COUNTER__ > C;

            {
                container_type container;
                container.template register_binding< storage< container_type,
       unique, C* >, C, IClass >(); AssertClass(container.template resolve< C
       >()); ASSERT_EQ(C::Constructor, 1); ASSERT_EQ(C::CopyConstructor, 1); //
       TODO: this is stupid. There should be no copy, just move.
                ASSERT_EQ(C::MoveConstructor, 0);
            }

            ASSERT_EQ(C::Destructor, C::GetTotalInstances());
        }
    */
    {
        typedef Class<unique_pointer, __COUNTER__> C;

        {
            container_type container;
            container.template register_binding<storage<unique, C*>>();
            AssertTypeNotConvertible<C, type_list<C, C&, C&&>>(container);

            auto c = container.template resolve<C*>();
            AssertClass(*c);
            ASSERT_EQ(C::Constructor, 1);
            ASSERT_EQ(C::CopyConstructor, 0);
            ASSERT_EQ(C::MoveConstructor, 0);
            ASSERT_EQ(C::Destructor, 0);

            delete c;
            ASSERT_EQ(C::Destructor, C::GetTotalInstances());
        }

        ASSERT_EQ(C::Destructor, C::GetTotalInstances());
    }
}
} // namespace dingo
