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

        struct C: I
        {
            C(
                annotated< I&, tag< 1 > > ref,
                annotated< I*, tag< 1 > > ptr,
                annotated< std::shared_ptr< I >, tag< 2 > > sharedptr
            ) 
                : ref_(ref)
                , ptr_(ptr)
                , sharedptr_(sharedptr)
            {
                BOOST_TEST(dynamic_cast<A*>(&ref_));
                BOOST_TEST(dynamic_cast<A*>(ptr_));
                BOOST_TEST(dynamic_cast<B*>(sharedptr_.get()));
            }

            I& ref_;
            I* ptr_;
            std::shared_ptr< I > sharedptr_;
        };

        container<> container;

        container.register_binding< storage< dingo::container<>, shared, A >, annotated< I, tag< 1 > > >();
        container.register_binding< storage< dingo::container<>, shared, std::shared_ptr< B > >, annotated< I, tag< 2 > > >();
        container.register_binding< storage< dingo::container<>, shared, std::shared_ptr< C > >, C, annotated< I, tag< 3 > > >();

        I& aref = container.resolve< annotated< I&, tag< 1 > > >();
        BOOST_TEST(dynamic_cast<A*>(&aref));
        
        I* bptr = container.resolve< annotated< I*, tag< 2 > > >();
        BOOST_TEST(dynamic_cast<B*>(bptr));

        std::shared_ptr< I > bsharedptr = container.resolve< annotated< std::shared_ptr< I >, tag< 2 > > >();
        BOOST_TEST(dynamic_cast<B*>(bsharedptr.get()));

        BOOST_TEST(dynamic_cast<C*>(&container.resolve< C& >()));
        I& cref = container.resolve< annotated< I&, tag< 3 > > >();
        BOOST_TEST(dynamic_cast<C*>(&cref));
    }
}