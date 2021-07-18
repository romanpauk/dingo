#include <dingo/container.h>
#include <dingo/storage/shared.h>

#include <boost/test/unit_test.hpp>

#include "class.h"
#include "assert.h"

namespace dingo
{
    BOOST_AUTO_TEST_CASE(TestMultipleInterfaces)
    {
        typedef Class< TestMultipleInterfaces, __COUNTER__ > C;

        dingo::container container;
        container.register_binding< storage< shared, C >, IClass, IClass1, IClass2 >();

        {
            auto c = container.resolve< IClass* >();
            BOOST_TEST(dynamic_cast<C*>(c));
        }

        {
            auto c = container.resolve< IClass1* >();
            BOOST_TEST(dynamic_cast<C*>(c));
        }

        {
            auto c = container.resolve< IClass2* >();
            BOOST_TEST(dynamic_cast<C*>(c));
        }
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
