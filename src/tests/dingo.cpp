#include <dingo/container.h>
#include <dingo/type_list.h>
#include <dingo/storage/shared.h>
#include <dingo/storage/shared_cyclical_protected.h>
#include <dingo/storage/unique.h>

#include <boost/test/unit_test.hpp>

#include "class.h"
#include "assert.h"

namespace dingo
{
    BOOST_AUTO_TEST_CASE(TestUniqueHierarchy)
    {
        struct S
        {};

        struct U
        {};

        struct B
        {
            B(std::shared_ptr< S >&&) {}
        };

        container container;
        container.register_binding< storage< unique, std::shared_ptr< S > > >();
        container.register_binding< storage< unique, std::unique_ptr< U > > >();
        container.register_binding< storage< shared, B > >();

        container.resolve< B& >();
    }
    BOOST_AUTO_TEST_CASE(TestResolveRollback)
    {
        typedef Class< TestResolveRollback, __COUNTER__ > A;
        typedef Class< TestResolveRollback, __COUNTER__ > B;
        struct Ex {};
        struct C
        {
            C(A&, B&) { throw Ex(); }
        };

        container container;
        container.register_binding< storage< shared, A > >();
        container.register_binding< storage< shared, B > >();
        container.register_binding< storage< shared, C > >();

        BOOST_CHECK_THROW(container.resolve< C& >(), Ex);
        BOOST_TEST(A::Constructor == 1);
        BOOST_TEST(A::Destructor == 1);
        BOOST_TEST(B::Constructor == 1);
        BOOST_TEST(B::Destructor == 1);

        container.resolve< A& >();
        BOOST_TEST(A::Constructor == 2);
        BOOST_TEST(A::Destructor == 1);
        BOOST_CHECK_THROW(container.resolve< C >(), Ex);
        BOOST_TEST(A::Constructor == 2);
        BOOST_TEST(A::Destructor == 1);
        BOOST_TEST(B::Constructor == 2);
        BOOST_TEST(B::Destructor == 2);
    }
}