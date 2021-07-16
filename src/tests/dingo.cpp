#include <dingo/container.h>
#include <dingo/type_list.h>
#include <dingo/storage/shared.h>
#include <dingo/storage/shared_cyclical.h>
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

    BOOST_AUTO_TEST_CASE(TestRecursion)
    {
        struct B;
        struct A
        {
            A(B&) {}
        };

        struct B
        {
            B(std::shared_ptr< A >) {}
        };

        container container;
        container.register_binding< storage< dingo::shared, std::shared_ptr< A > > >();
        container.register_binding< storage< dingo::shared, B > >();

        BOOST_CHECK_THROW(container.resolve< A >(), type_recursion_exception);
        BOOST_CHECK_THROW(container.resolve< B >(), type_recursion_exception);
    }

    BOOST_AUTO_TEST_CASE(TestRecursionCyclical)
    {
        struct B;
        struct A: Class< __COUNTER__ >
        {
            A(B& b): b_(b) {}

            B& b_;
        };

        struct B: Class< __COUNTER__ >
        {
            B(std::shared_ptr< A > aptr, A& a): aptr_(aptr), a_(a)
            {
                BOOST_CHECK_THROW(aptr_->GetName(), type_not_constructed_exception);
                BOOST_CHECK_THROW(a_.GetName(), type_not_constructed_exception);
            }

            std::shared_ptr< A > aptr_;
            A& a_;
        };

        container container;
        container.register_binding< storage< shared_cyclical, std::shared_ptr< A > > >();
        container.register_binding< storage< shared_cyclical, B > >();

        auto& a = container.resolve< A& >();
        AssertClass(a);
        AssertClass(a.b_);

        auto& b = container.resolve< B& >();
        AssertClass(b);
        AssertClass(b.a_);
        AssertClass(*b.aptr_);       
    }

    BOOST_AUTO_TEST_CASE(TestResolveRollback)
    {
        typedef Class< __COUNTER__ > A;
        typedef Class< __COUNTER__ > B;
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