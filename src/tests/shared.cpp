#include <dingo/container.h>
#include <dingo/performance_counter.h>
#include <dingo/storage/shared.h>

#include <boost/test/unit_test.hpp>

#include "class.h"
#include "assert.h"

namespace dingo
{
    BOOST_AUTO_TEST_CASE(TestSharedValue)
    {
        typedef Class< __COUNTER__ > C;
        {
            container container;
            container.register_binding< storage< shared, C > >();

            AssertClass(*container.resolve< C* >());
            AssertClass(container.resolve< C& >());

            AssertTypeNotConvertible < C, type_list< C, std::shared_ptr< C >, std::unique_ptr< C > > >(container);

            BOOST_TEST(C::Constructor == 1);
            BOOST_TEST(C::Destructor == 0);
            BOOST_TEST(C::CopyConstructor == 0);
            BOOST_TEST(C::MoveConstructor == 0);
        }

        {
            BOOST_TEST(C::Destructor == 1);
        }
    }

    BOOST_AUTO_TEST_CASE(TestSharedPointer)
    {
        typedef Class< __COUNTER__ > C;

        {
            container container;
            container.register_binding< storage< shared, C* > >();

            AssertClass(*container.resolve< C* >());
            AssertClass(container.resolve< C& >());

            AssertTypeNotConvertible< C, type_list< C, std::shared_ptr< C >, std::unique_ptr< C > > >(container);

            BOOST_TEST(C::Constructor == 1);
            BOOST_TEST(C::Destructor == 0);
            BOOST_TEST(C::CopyConstructor == 0);
            BOOST_TEST(C::MoveConstructor == 0);
        }

        {
            BOOST_TEST(C::Destructor == 1);
        }
    }

    BOOST_AUTO_TEST_CASE(TestSharedSharedPtr)
    {
        typedef Class< __COUNTER__ > C;

        {
            container container;
            container.register_binding< storage< shared, std::shared_ptr< C > > >();

            AssertClass(*container.resolve< C* >());
            AssertClass(container.resolve< C& >());
            AssertClass(*container.resolve< std::shared_ptr< C > >());
            AssertClass(*container.resolve< std::shared_ptr< C >& >());
            AssertClass(**container.resolve< std::shared_ptr< C >* >());

            AssertTypeNotConvertible< C, type_list< C, std::unique_ptr< C > > >(container);

            BOOST_TEST(C::Constructor == 1);
            BOOST_TEST(C::Destructor == 0);
            BOOST_TEST(C::CopyConstructor == 0);
            BOOST_TEST(C::MoveConstructor == 0);
        }

        {
            BOOST_TEST(C::Destructor == 1);
        }
    }

    BOOST_AUTO_TEST_CASE(TestSharedUniquePtr)
    {
        typedef Class< __COUNTER__ > C;

        {
            container container;
            container.register_binding< storage< shared, std::unique_ptr< C > > >();

            AssertClass(*container.resolve< C* >());
            AssertClass(container.resolve< C& >());
            AssertClass(*container.resolve< std::unique_ptr< C >& >());
            AssertClass(**container.resolve< std::unique_ptr< C >* >());

            AssertTypeNotConvertible< C, type_list< C, std::unique_ptr< C > > >(container);

            BOOST_TEST(C::Constructor == 1);
            BOOST_TEST(C::Destructor == 0);
            BOOST_TEST(C::CopyConstructor == 0);
            BOOST_TEST(C::MoveConstructor == 0);
        }

        {
            BOOST_TEST(C::Destructor == 1);
        }
    }

    BOOST_AUTO_TEST_CASE(TestSharedHierarchy)
    {
        struct S : Class< __COUNTER__ >
        {};

        struct U : Class< __COUNTER__ >
        {
            U(S& s1)
            {
                AssertClass(s1);
            }
        };

        struct B : Class< __COUNTER__ >
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
        container.register_binding< storage< shared, std::shared_ptr< S > > >();
        container.register_binding< storage< shared, std::unique_ptr< U > > >();
        container.register_binding< storage< shared, B > >();

        container.resolve< B& >();
    }

}
