#include <dingo/container.h>
#include <dingo/storage/external.h>

#include <gtest/gtest.h>

#include "class.h"
#include "assert.h"

namespace dingo
{
    TEST(external, value)
    {
        struct external_value {};
        typedef Class< external_value, __COUNTER__ > C;
        C c;

        {
            container<> container;
            container.register_binding< storage< dingo::container<>, external, C >, C, IClass >(std::move(c));

            AssertClass(*container.resolve< C* >());
            AssertClass(container.resolve< C& >());
            AssertClass(container.resolve< IClass& >());
        }
    }

    TEST(external, ref)
    {
        struct external_ref {};
        typedef Class< external_ref, __COUNTER__ > C;
        C c;

        {
            container<> container;
            container.register_binding< storage< dingo::container<>, external, C& >, C, IClass >(c);

            ASSERT_EQ(container.resolve< C* >(), &c);
            AssertClass(*container.resolve< C* >());
            AssertClass(container.resolve< C& >());
            AssertClass(container.resolve< IClass& >());
        }
    }

    TEST(external, ptr)
    {
        struct external_ptr {};
        typedef Class< external_ptr, __COUNTER__ > C;
        C c;

        {
            container<> container;
            container.register_binding< storage< dingo::container<>, external, C* >, C, IClass >(&c);

            ASSERT_EQ(container.resolve< C* >(), &c);
            AssertClass(*container.resolve< C* >());
            AssertClass(container.resolve< C& >());
            AssertClass(container.resolve< IClass& >());
        }
    }

    TEST(external, shared_ptr)
    {
        struct external_shared_ptr_lvalue {};
        typedef Class< external_shared_ptr_lvalue, __COUNTER__ > C;
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

    TEST(external, shared_ptr_ref)
    {
        struct external_shared_ptr_rvalue {};
        typedef Class< external_shared_ptr_rvalue, __COUNTER__ > C;
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

    TEST(external, unique_ptr_ref)
    {
        struct external_unique_ptr_lvalue {};
        typedef Class< external_unique_ptr_lvalue, __COUNTER__ > C;
        auto c = std::make_unique< C >();

        {
            container<> container;
            container.register_binding< storage< dingo::container<>, external, std::unique_ptr< C >& > >(c);
            container.resolve< C& >();
            container.resolve< C* >();
            container.resolve< std::unique_ptr< C >& >();
        }
    }

    TEST(external, unique_ptr_move)
    {
        struct external_unique_ptr_rvalue {};
        typedef Class< external_unique_ptr_rvalue, __COUNTER__ > C;
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
