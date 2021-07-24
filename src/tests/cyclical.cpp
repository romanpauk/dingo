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
    BOOST_AUTO_TEST_CASE(test_recursion_exception)
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

        container<> container;
        container.register_binding< storage< dingo::container<>, shared, std::shared_ptr< A > > >();
        container.register_binding< storage< dingo::container<>, shared, B > >();

        BOOST_CHECK_THROW(container.resolve< A >(), type_recursion_exception);
        BOOST_CHECK_THROW(container.resolve< B >(), type_recursion_exception);
    }

    BOOST_AUTO_TEST_CASE(test_shared_cyclical_value)
    {
        struct B;
        struct A : Class< test_shared_cyclical_value, __COUNTER__ >
        {
            A(B& b) : b_(b) {}
            B& b_;
        };

        struct B : Class< test_shared_cyclical_value, __COUNTER__ >
        {
            B(A& a, IClass& ia) : a_(a), ia_(ia) {}
            A& a_;
            IClass& ia_;
        };

        container<> container;
        container.register_binding< storage< dingo::container<>, shared_cyclical, A >, A, IClass >();
        container.register_binding< storage< dingo::container<>, shared_cyclical, B > >();

        auto& a = container.resolve< A& >();
        AssertClass(a);
        AssertClass(a.b_);
        

        auto& b = container.resolve< B& >();
        AssertClass(b);
        AssertClass(b.a_);
        AssertClass(b.ia_);

        auto& c = container.resolve< IClass& >();
        AssertClass(c);
    }

    BOOST_AUTO_TEST_CASE(test_shared_cyclical_shared_ptr)
    {
        struct B;
        struct A : Class< test_shared_cyclical_shared_ptr, __COUNTER__ >
        {
            A(B& b, IClass1& ib, std::shared_ptr< IClass1 >& ibptr) : b_(b), ib_(ib), ibptr_(ibptr) {}
            B& b_;
            IClass1& ib_;
            std::shared_ptr< IClass1 >& ibptr_;

            // Conversion to IClass& ib_; crashes during constructor as the object is not healthy during construction.
        };

        struct B : Class< test_shared_cyclical_shared_ptr, __COUNTER__ >
        {
            B(A& a, IClass& ia, std::shared_ptr< IClass >& iaptr) : a_(a), ia_(ia), iaptr_(iaptr) {}
            A& a_;
            IClass& ia_;
            std::shared_ptr< IClass >& iaptr_;
        };

        container<> container;
        container.register_binding< storage< dingo::container<>, shared_cyclical, std::shared_ptr< A > >, A, IClass >();
        container.register_binding< storage< dingo::container<>, shared_cyclical, std::shared_ptr< B > >, B, IClass1 >();

        // TODO: container.resolve< IClass& >();
        // Virtual base has some issues.

        auto& a = container.resolve< A& >();
        AssertClass(a);
        AssertClass(a.b_);
        AssertClass(a.ib_);
        AssertClass(*a.ibptr_);

        auto& b = container.resolve< B& >();
        AssertClass(b);
        AssertClass(b.a_);
        AssertClass(b.ia_);
        AssertClass(*b.iaptr_);
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