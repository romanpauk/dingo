#include <dingo/container.h>
#include <dingo/performance_counter.h>
#include <dingo/storage/shared.h>

#include <boost/test/unit_test.hpp>

#include "class.h"
#include "assert.h"

namespace dingo
{
    BOOST_AUTO_TEST_CASE(test_shared_value)
    {
        typedef Class< test_shared_value, __COUNTER__ > C;
        {
            container<> container;
            container.register_binding< storage< dingo::container<>, shared, C > >();

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

    BOOST_AUTO_TEST_CASE(test_shared_raw_ptr)
    {
        typedef Class< test_shared_raw_ptr, __COUNTER__ > C;

        {
            container<> container;
            container.register_binding< storage< dingo::container<>, shared, C* > >();

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

    BOOST_AUTO_TEST_CASE(test_shared_shared_ptr)
    {
        typedef Class< test_shared_shared_ptr, __COUNTER__ > C;

        {
            container container;
            container.register_binding< storage< dingo::container<>, shared, std::shared_ptr< C > > >();

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

    BOOST_AUTO_TEST_CASE(test_shared_unique_ptr)
    {
        typedef Class< test_shared_unique_ptr, __COUNTER__ > C;

        {
            container container;
            container.register_binding< storage< dingo::container<>, shared, std::unique_ptr< C > > >();

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

    BOOST_AUTO_TEST_CASE(test_shared_hierarchy)
    {
        struct S : Class< test_shared_hierarchy, __COUNTER__ >
        {};

        struct U : Class< test_shared_hierarchy, __COUNTER__ >
        {
            U(S& s1)
            {
                AssertClass(s1);
            }
        };

        struct B : Class< test_shared_hierarchy, __COUNTER__ >
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
