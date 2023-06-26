#include <dingo/container.h>
#include <dingo/type_list.h>
#include <dingo/storage/shared.h>
#include <dingo/storage/unique.h>

#include <gtest/gtest.h>

#include "class.h"
#include "assert.h"

namespace dingo
{
    TEST(dingo, unique_hierarchy)
    {
        struct S
        {};

        struct U
        {};

        struct B
        {
            B(std::shared_ptr< S >&&) {}
        };

        container<> container;
        container.register_binding< storage< dingo::container<>, unique, std::shared_ptr< S > > >();
        container.register_binding< storage< dingo::container<>, unique, std::unique_ptr< U > > >();
        container.register_binding< storage< dingo::container<>, shared, B > >();

        container.resolve< B& >();
    }
    
    TEST(dingo, resolve_rollback)
    {
        struct resolve_rollback {};
        typedef Class< resolve_rollback, __COUNTER__ > A;
        typedef Class< resolve_rollback, __COUNTER__ > B;
        struct Ex {};
        struct C
        {
            C(A&, B&) { throw Ex(); }
        };

        container<> container;
        container.register_binding< storage< dingo::container<>, shared, A > >();
        container.register_binding< storage< dingo::container<>, shared, B > >();
        container.register_binding< storage< dingo::container<>, shared, C > >();

        ASSERT_THROW(container.resolve< C& >(), Ex);
        ASSERT_EQ(A::Constructor, 1);
        ASSERT_EQ(A::Destructor, 1);
        ASSERT_EQ(B::Constructor, 1);
        ASSERT_EQ(B::Destructor, 1);

        container.resolve< A& >();
        ASSERT_EQ(A::Constructor, 2);
        ASSERT_EQ(A::Destructor, 1);
        ASSERT_THROW(container.resolve< C >(), Ex);
        ASSERT_EQ(A::Constructor, 2);
        ASSERT_EQ(A::Destructor, 1);
        ASSERT_EQ(B::Constructor, 2);
        ASSERT_EQ(B::Destructor, 2);
    }
}