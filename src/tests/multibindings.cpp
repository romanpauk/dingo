#include <dingo/container.h>
#include <dingo/storage/shared.h>
#include <dingo/storage/shared_cyclical.h>
#include <dingo/storage/unique.h>

#include <boost/test/unit_test.hpp>
#include <boost/mpl/list.hpp>

#include "class.h"
#include "assert.h"

namespace dingo
{
    // TODO: shared_cyclical throws
    typedef boost::mpl::list< shared /*, shared_cyclical */ > test_types_value;
    BOOST_AUTO_TEST_CASE_TEMPLATE(test_multiple_interfaces_shared_value, Policy, test_types_value)
    {
        typedef Class< test_multiple_interfaces_shared_value, __COUNTER__ > C;

        container<> container;
        container.register_binding< storage< dingo::container<>, Policy, C >, IClass, IClass1, IClass2 >();

        BOOST_TEST(dynamic_cast<C*>(&container.resolve< IClass& >()));
        BOOST_TEST(dynamic_cast<C*>(container.resolve< IClass1* >()));
        BOOST_TEST(dynamic_cast<C*>(container.resolve< IClass2* >()));
    }

    // TODO: shared_cyclical crashes
    typedef boost::mpl::list< shared /*, shared_cyclical*/ > test_types_shared_ptr;
    BOOST_AUTO_TEST_CASE_TEMPLATE(test_multiple_interfaces_shared_shared_ptr, Policy, test_types_shared_ptr)
    {
        typedef Class< test_multiple_interfaces_shared_shared_ptr, __COUNTER__ > C;

        container<> container;
        container.register_binding< storage< dingo::container<>, Policy, std::shared_ptr< C > >, IClass, IClass1, IClass2 >();

        BOOST_TEST(dynamic_cast<C*>(&container.resolve< IClass& >()));
        BOOST_TEST(dynamic_cast<C*>(container.resolve< std::shared_ptr< IClass >& >().get()));
        BOOST_TEST(dynamic_cast<C*>(&container.resolve< IClass1& >()));
        BOOST_TEST(dynamic_cast<C*>(container.resolve< std::shared_ptr< IClass1 > >().get()));
        BOOST_TEST(dynamic_cast<C*>(&container.resolve< IClass2& >()));
        BOOST_TEST(dynamic_cast<C*>(container.resolve< std::shared_ptr< IClass2 >* >()->get()));
    }

    /*
    // TODO: does not compile
    BOOST_AUTO_TEST_CASE(test_multiple_interfaces_shared_unique_ptr)
    {
        typedef Class< test_multiple_interfaces_shared_unique_ptr, __COUNTER__ > C;

        container<> container;
        container.register_binding< storage< dingo::container<>, shared, std::unique_ptr< C > >, IClass, IClass1, IClass2 >();
    }
    */

    BOOST_AUTO_TEST_CASE(test_multiple_interfaces_unique_shared_ptr)
    {
        typedef Class< test_multiple_interfaces_unique_shared_ptr, __COUNTER__ > C;

        container<> container;
        container.register_binding< storage< dingo::container<>, unique, std::shared_ptr< C > >, IClass, IClass1, IClass2 >();

        BOOST_TEST(dynamic_cast<C*>(container.resolve< std::shared_ptr< IClass > >().get()));
        BOOST_TEST(dynamic_cast<C*>(container.resolve< std::shared_ptr< IClass1 > >().get()));
        BOOST_TEST(dynamic_cast<C*>(container.resolve< std::shared_ptr< IClass2 > >().get()));
    }

    BOOST_AUTO_TEST_CASE(test_multiple_interfaces_unique_unique_ptr)
    {
        typedef Class< test_multiple_interfaces_unique_unique_ptr, __COUNTER__ > C;

        container<> container;
        container.register_binding< storage< dingo::container<>, unique, std::unique_ptr< C > >, IClass, IClass1, IClass2 >();

        BOOST_TEST(dynamic_cast<C*>(container.resolve< std::unique_ptr< IClass > >().get()));
        BOOST_TEST(dynamic_cast<C*>(container.resolve< std::unique_ptr< IClass1 > >().get()));
        BOOST_TEST(dynamic_cast<C*>(container.resolve< std::unique_ptr< IClass2 > >().get()));
    }

// TODO: this is not correctly implemented, it leaks memory
#if 0
    BOOST_AUTO_TEST_CASE(TestResolveMultiple)
    {
        struct I {};
        struct A : I {};
        struct B : I {};
        struct C
        {
            C(std::vector< I* > v, std::list< I* > l, std::set< I* > s) : v_(v), l_(l), s_(s) {}

            std::vector< I* > v_;
            std::list< I* > l_;
            std::set< I* > s_;
        };

        container container;
        container.register_binding< storage< shared, A >, A, I >();
        container.register_binding< storage< shared, B >, B, I >();
        container.register_binding< storage< shared, C > >();

        {
            auto vector = container.resolve< std::vector< I* > >();
            BOOST_TEST(vector.size() == 2);

            auto list = container.resolve< std::list< I* > >();
            BOOST_TEST(list.size() == 2);

            auto set = container.resolve< std::set< I* > >();
            BOOST_TEST(set.size() == 2);
        }

        {
            BOOST_CHECK_THROW(container.resolve< std::vector< std::shared_ptr< I > > >(), type_not_convertible_exception);
        }

        {
            auto& c = container.resolve< C& >();
            BOOST_TEST(c.v_.size() == 2);
            BOOST_TEST(c.l_.size() == 2);
            BOOST_TEST(c.s_.size() == 2);
        }
    }
#endif
}
