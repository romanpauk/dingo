#include <dingo/container.h>
#include <dingo/storage/shared.h>

#include <gtest/gtest.h>

#include "class.h"
#include "assert.h"

namespace dingo
{
    TEST(shared, value)
    {
        struct shared_value {};
        typedef Class< shared_value, __COUNTER__ > C;
        {
            container<> container;
            container.register_binding< storage< dingo::container<>, shared, C >, C, IClass >();

            AssertClass(*container.resolve< C* >());
            AssertClass(container.resolve< C& >());

            AssertTypeNotConvertible < C, type_list< C, std::shared_ptr< C >, std::unique_ptr< C > > >(container);

            ASSERT_EQ(C::Constructor, 1);
            ASSERT_EQ(C::Destructor, 0);
            ASSERT_EQ(C::CopyConstructor, 0);
            ASSERT_EQ(C::MoveConstructor, 0);
        }

        {
            ASSERT_EQ(C::Destructor, 1);
        }
    }

    TEST(shared, ptr)
    {
        struct shared_ptr {};
        typedef Class< shared_ptr, __COUNTER__ > C;

        {
            container<> container;
            container.register_binding< storage< dingo::container<>, shared, C* >, C, IClass >();

            AssertClass(*container.resolve< C* >());
            AssertClass(container.resolve< C& >());

            AssertTypeNotConvertible< C, type_list< C, std::shared_ptr< C >, std::unique_ptr< C > > >(container);

            ASSERT_EQ(C::Constructor, 1);
            ASSERT_EQ(C::Destructor, 0);
            ASSERT_EQ(C::CopyConstructor, 0);
            ASSERT_EQ(C::MoveConstructor, 0);
        }

        {
            ASSERT_EQ(C::Destructor, 1);
        }
    }

    TEST(shared, shared_ptr)
    {
        struct shared_shared_ptr {};
        typedef Class< shared_shared_ptr, __COUNTER__ > C;

        {
            container container;
            container.register_binding< storage< dingo::container<>, shared, std::shared_ptr< C > >, C, IClass >();

            AssertClass(*container.resolve< C* >());
            AssertClass(container.resolve< C& >());
            AssertClass(*container.resolve< std::shared_ptr< C > >());
            AssertClass(*container.resolve< std::shared_ptr< C >& >());
            AssertClass(**container.resolve< std::shared_ptr< C >* >());

            AssertTypeNotConvertible< C, type_list< C, std::unique_ptr< C > > >(container);

            ASSERT_EQ(C::Constructor, 1);
            ASSERT_EQ(C::Destructor, 0);
            ASSERT_EQ(C::CopyConstructor, 0);
            ASSERT_EQ(C::MoveConstructor, 0);
        }

        {
            ASSERT_EQ(C::Destructor, 1);
        }
    }

    TEST(shared, unique_ptr)
    {
        struct shared_unique_ptr {};
        typedef Class< shared_unique_ptr, __COUNTER__ > C;

        {
            container container;
            container.register_binding< storage< dingo::container<>, shared, std::unique_ptr< C > > >();

            AssertClass(*container.resolve< C* >());
            AssertClass(container.resolve< C& >());
            AssertClass(*container.resolve< std::unique_ptr< C >& >());
            AssertClass(**container.resolve< std::unique_ptr< C >* >());

            AssertTypeNotConvertible< C, type_list< C, std::unique_ptr< C > > >(container);

            ASSERT_EQ(C::Constructor, 1);
            ASSERT_EQ(C::Destructor, 0);
            ASSERT_EQ(C::CopyConstructor, 0);
            ASSERT_EQ(C::MoveConstructor, 0);
        }

        {
            ASSERT_EQ(C::Destructor, 1);
        }
    }

    TEST(shared, hierarchy)
    {
        struct shared_hierarchy {};
        struct S : Class< shared_hierarchy, __COUNTER__ >
        {};

        struct U : Class< shared_hierarchy, __COUNTER__ >
        {
            U(S& s1)
            {
                AssertClass(s1);
            }
        };

        struct B : Class< shared_hierarchy, __COUNTER__ >
        {
            B(S s1, S& s2, S* s3, std::shared_ptr< S >* s4, std::shared_ptr< S >& s5, std::shared_ptr< S > s6,
                U u1, U& u2, U* u3, std::unique_ptr< U >* u4, std::unique_ptr< U >& u5
            )
            {
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

        container container;
        container.register_binding< storage< dingo::container<>, shared, std::shared_ptr< S > > >();
        container.register_binding< storage< dingo::container<>, shared, std::unique_ptr< U > > >();
        container.register_binding< storage< dingo::container<>, shared, B > >();

        container.resolve< B& >();
    }

}
