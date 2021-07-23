#include <dingo/container.h>
#include <dingo/type_list.h>
#include <dingo/annotated.h>
#include <dingo/storage/shared.h>

#include <boost/test/unit_test.hpp>

#include "class.h"
#include "assert.h"

namespace dingo
{
    template < size_t N > struct tag {};

    /*
    BOOST_AUTO_TEST_CASE(test_annotated_value)
    {        
        struct A
        {
            A(
                annotated< int, tag< 1 > > value1, 
                annotated< int, tag< 2 > > value10
            ) {}
        };

        // TODO: support constant / lambda factory
        container<> container;
        container.register_binding< storage< dingo::container<>, unique, annotated< int, tag< 1 > > > >(1);
        container.register_binding< storage< dingo::container<>, unique, annotated< int, tag< 2 > > > >([]{ return 10; });
        container.register_binding< storage< dingo::container<>, unique, A > >();

        container.resolve< annotated< int, tag< 1 > > >();
        container.resolve< annotated< int, tag< 2 > > >();
        container.resolve< A >();
    }
    */

    BOOST_AUTO_TEST_CASE(test_annotated_interface)
    {
        struct I { virtual ~I() {} };
        struct A : I {};
        struct B : I {};
        struct C : I {};

        container<> container;

        container.register_binding< storage< dingo::container<>, shared, A >, annotated< I, tag< 1 > > >();
        container.register_binding< storage< dingo::container<>, shared, B >, annotated< I, tag< 2 > > >();
        container.register_binding< storage< dingo::container<>, shared, std::shared_ptr< C > >, annotated< I, tag< 3 > > >();
        
        BOOST_TEST(dynamic_cast<A*>(&container.resolve< annotated< I, tag< 1 > >& >()));
        BOOST_TEST(dynamic_cast<B*>(&container.resolve< annotated< I, tag< 2 > >& >()));
        BOOST_TEST(dynamic_cast<C*>(&container.resolve< annotated< I, tag< 3 > >& >()));
    }
}