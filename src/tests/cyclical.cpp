#include <dingo/container.h>
#include <dingo/type_list.h>
#include <dingo/storage/shared.h>
#include <dingo/storage/shared_cyclical.h>
#include <dingo/storage/shared_cyclical_protected.h>
#include <dingo/storage/unique.h>

#include <boost/test/unit_test.hpp>

#include "class.h"
#include "assert.h"

namespace dingo
{
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
        struct A : Class< TestRecursionCyclical, __COUNTER__ >
        {
            A(B& b) : b_(b) {}
            B& b_;
        };

        struct B : Class< TestRecursionCyclical, __COUNTER__ >
        {
            B(A& a) : a_(a) {}
            A& a_;
        };

        container container;
        container.register_binding< storage< shared_cyclical, A > >();
        container.register_binding< storage< shared_cyclical, B > >();

        auto& a = container.resolve< A& >();
        AssertClass(a);
        AssertClass(a.b_);

        auto& b = container.resolve< B& >();
        AssertClass(b);
        AssertClass(b.a_);
    }

#if 0
#if defined(_WIN32)
    BOOST_AUTO_TEST_CASE(TestRecursionCyclicalProtected)
    {
        struct B;
        struct A : Class< TestRecursionCyclicalProtected, __COUNTER__ >
        {
            A(B& b) : b_(b) {}

            B& b_;
        };

        struct B : Class< TestRecursionCyclicalProtected, __COUNTER__ >
        {
            B(std::shared_ptr< A > aptr, A& a) : aptr_(aptr), a_(a)
            {
                BOOST_CHECK_THROW(aptr_->GetName(), type_not_constructed_exception);
                BOOST_CHECK_THROW(a_.GetName(), type_not_constructed_exception);
            }

            std::shared_ptr< A > aptr_;
            A& a_;
        };

        container container;
        container.register_binding< storage< shared_cyclical_protected, std::shared_ptr< A > > >();
        container.register_binding< storage< shared_cyclical_protected, B > >();

        auto& a = container.resolve< A& >();
        AssertClass(a);
        AssertClass(a.b_);

        auto& b = container.resolve< B& >();
        AssertClass(b);
        AssertClass(b.a_);
        AssertClass(*b.aptr_);
    }
#endif
#endif
}