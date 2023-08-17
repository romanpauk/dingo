#include <dingo/container.h>
#include <dingo/storage/shared.h>

#include <gtest/gtest.h>

#include "class.h"
#include "assert.h"
#include "containers.h"

namespace dingo {
    template <typename T> struct shared_test : public testing::Test {};
    TYPED_TEST_SUITE(shared_test, container_types);

    TYPED_TEST(shared_test, value) {
        using container_type = TypeParam;

        struct shared_value {};
        typedef Class< shared_value, __COUNTER__ > C;
        {
            container_type container;
            container.template register_binding< storage< container_type, shared, C >, C, IClass >();

            AssertClass(*container.template resolve< C* >());
            AssertClass(container.template resolve< C& >());

            AssertTypeNotConvertible < C, type_list< std::shared_ptr< C >, std::unique_ptr< C > > >(container);

            ASSERT_EQ(C::Constructor, 1);
            ASSERT_EQ(C::Destructor, 0);
            ASSERT_EQ(C::CopyConstructor, 0);
            ASSERT_EQ(C::MoveConstructor, 0);
        }

        {
            ASSERT_EQ(C::Destructor, C::GetTotalInstances());
        }
    }

    TYPED_TEST(shared_test, ptr) {
        using container_type = TypeParam;

        struct shared_ptr {};
        typedef Class< shared_ptr, __COUNTER__ > C;

        {
            container_type container;
            container.template register_binding< storage< container_type, shared, C* >, C, IClass >();

            AssertClass(*container.template resolve< C* >());
            AssertClass(container.template resolve< C& >());

            AssertTypeNotConvertible< C, type_list< std::shared_ptr< C >, std::unique_ptr< C > > >(container);

            ASSERT_EQ(C::Constructor, 1);
            ASSERT_EQ(C::Destructor, 0);
            ASSERT_EQ(C::CopyConstructor, 0);
            ASSERT_EQ(C::MoveConstructor, 0);
        }

        {
            ASSERT_EQ(C::Destructor, C::GetTotalInstances());
        }
    }

    TYPED_TEST(shared_test, shared_ptr) {
        using container_type = TypeParam;

        struct shared_shared_ptr {};
        typedef Class< shared_shared_ptr, __COUNTER__ > C;

        {
            container_type container;
            container.template register_binding< storage< container_type, shared, std::shared_ptr< C > >, C, IClass >();

            AssertClass(*container.template resolve< C* >());
            AssertClass(container.template resolve< C& >());
            AssertClass(*container.template resolve< std::shared_ptr< C > >());
            AssertClass(*container.template resolve< std::shared_ptr< C >& >());
            AssertClass(**container.template resolve< std::shared_ptr< C >* >());

            AssertTypeNotConvertible< C, type_list< std::unique_ptr< C > > >(container);

            ASSERT_EQ(C::Constructor, 1);
            ASSERT_EQ(C::Destructor, 0);
            ASSERT_EQ(C::CopyConstructor, 0);
            ASSERT_EQ(C::MoveConstructor, 0);
        }

        {
            ASSERT_EQ(C::Destructor, C::GetTotalInstances());
        }
    }

    TYPED_TEST(shared_test, unique_ptr) {
        using container_type = TypeParam;

        struct shared_unique_ptr {};
        typedef Class< shared_unique_ptr, __COUNTER__ > C;

        {
            container_type container;
            container.template register_binding< storage< container_type, shared, std::unique_ptr< C > > >();

            AssertClass(*container.template resolve< C* >());
            AssertClass(container.template resolve< C& >());
            AssertClass(*container.template resolve< std::unique_ptr< C >& >());
            AssertClass(**container.template resolve< std::unique_ptr< C >* >());

            AssertTypeNotConvertible< C, type_list< std::unique_ptr< C > > >(container);

            ASSERT_EQ(C::Constructor, 1);
            ASSERT_EQ(C::Destructor, 0);
            ASSERT_EQ(C::CopyConstructor, 0);
            ASSERT_EQ(C::MoveConstructor, 0);
        }

        {
            ASSERT_EQ(C::Destructor, C::GetTotalInstances());
        }
    }

    TYPED_TEST(shared_test, hierarchy) {
        using container_type = TypeParam;

        struct shared_hierarchy {};
        struct S : Class< shared_hierarchy, __COUNTER__ > {};
        struct U : Class< shared_hierarchy, __COUNTER__ > {
            U(S& s1) {
                AssertClass(s1);
            }
        };

        struct B : Class< shared_hierarchy, __COUNTER__ > {
            B(S s1, S& s2, S* s3, std::shared_ptr< S >* s4, std::shared_ptr< S >& s5, std::shared_ptr< S > s6,
                U u1, U& u2, U* u3, std::unique_ptr< U >* u4, std::unique_ptr< U >& u5
            ) {
                AssertClass(s1);
                AssertClass(s2);
                AssertClass(*s3);
                AssertClass(**s4);
                AssertClass(*s5);
                AssertClass(*s6);

                AssertClass(u1);
                AssertClass(u2);
                AssertClass(*u3);
                AssertClass(**u4);
                AssertClass(*u5);
            }
        };

        container_type container;
        container.template register_binding< storage< container_type, shared, std::shared_ptr< S > > >();
        container.template register_binding< storage< container_type, shared, std::unique_ptr< U > > >();
        container.template register_binding< storage< container_type, shared, B > >();

        container.template resolve< B& >();
    }
}
