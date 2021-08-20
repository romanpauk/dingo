#include <dingo/container.h>
#include <dingo/storage/external.h>

#include <boost/test/unit_test.hpp>

#include "class.h"
#include "assert.h"

namespace dingo
{
    BOOST_AUTO_TEST_CASE(test_external_value)
    {
        typedef Class< test_external_value, __COUNTER__ > C;
        C c;

        {
            container<> container;
            container.register_binding< storage< dingo::container<>, external, C >, C, IClass >(std::move(c));

            AssertClass(*container.resolve< C* >());
            AssertClass(container.resolve< C& >());
            AssertClass(container.resolve< IClass& >());
        }
    }

    BOOST_AUTO_TEST_CASE(test_external_ref)
    {
        typedef Class< test_external_ref, __COUNTER__ > C;
        C c;

        {
            container<> container;
            container.register_binding< storage< dingo::container<>, external, C& >, C, IClass >(c);

            BOOST_TEST(container.resolve< C* >() == &c);
            AssertClass(*container.resolve< C* >());
            AssertClass(container.resolve< C& >());
            AssertClass(container.resolve< IClass& >());
        }
    }

    BOOST_AUTO_TEST_CASE(test_external_ptr)
    {
        typedef Class< test_external_ptr, __COUNTER__ > C;
        C c;

        {
            container<> container;
            container.register_binding< storage< dingo::container<>, external, C* >, C, IClass >(&c);

            BOOST_TEST(container.resolve< C* >() == &c);
            AssertClass(*container.resolve< C* >());
            AssertClass(container.resolve< C& >());
            AssertClass(container.resolve< IClass& >());
        }
    }

    BOOST_AUTO_TEST_CASE(test_external_shared_ptr_lvalue)
    {
        typedef Class< test_external_shared_ptr_lvalue, __COUNTER__ > C;
        auto c = std::make_shared< C >();

        {
            container<> container;
            container.register_binding< storage< dingo::container<>, external, std::shared_ptr< C > >, C, IClass >(c);
            AssertClass(container.resolve< C& >());
            AssertClass(*container.resolve< C* >());
            AssertClass(*container.resolve< std::shared_ptr< C > >());
            AssertClass(*container.resolve< std::shared_ptr< C >& >());
            AssertClass(container.resolve< IClass& >());
            AssertClass(*container.resolve< std::shared_ptr< IClass > >());
        }
    }

    BOOST_AUTO_TEST_CASE(test_external_shared_ptr_rvalue)
    {
        typedef Class< test_external_shared_ptr_rvalue, __COUNTER__ > C;
        auto c = std::make_shared< C >();

        {
            container<> container;
            container.register_binding < storage< dingo::container<>, external, std::shared_ptr< C >& >, C, IClass >(c); //, C, IClass >(c);
            container.resolve< C& >();
            container.resolve< C* >();
            container.resolve< std::shared_ptr< C > >();
            container.resolve< std::shared_ptr< C >& >();

            // container.resolve< IClass& >();
        }
    }

    BOOST_AUTO_TEST_CASE(test_external_unique_ptr_lvalue)
    {
        typedef Class< test_external_unique_ptr_lvalue, __COUNTER__ > C;
        auto c = std::make_unique< C >();

        {
            container<> container;
            container.register_binding< storage< dingo::container<>, external, std::unique_ptr< C >& > >(c);
            container.resolve< C& >();
            container.resolve< C* >();
            container.resolve< std::unique_ptr< C >& >();
        }
    }

    BOOST_AUTO_TEST_CASE(test_external_unique_ptr_rvalue)
    {
        typedef Class< test_external_unique_ptr_rvalue, __COUNTER__ > C;
        auto c = std::make_unique< C >();

        {
            container<> container;
            container.register_binding< storage< dingo::container<>, external, std::unique_ptr< C > > >(std::move(c));
            container.resolve< C& >();
            container.resolve< C* >();
            container.resolve< std::unique_ptr< C >& >();
        }
    }
}
