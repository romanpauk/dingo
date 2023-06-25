#include <dingo/container.h>
#include <dingo/storage/unique.h>

#include <gtest/gtest.h>

#include "class.h"
#include "assert.h"

namespace dingo
{
    TEST(unique, value)
    {
        struct unique_value {};
        {
            typedef Class< unique_value, __COUNTER__ > C;

            {
                container<> container;
                container.register_binding< storage< dingo::container<>, unique, C > >();

                {
                    AssertClass(container.resolve< C&& >());
                    ASSERT_EQ(C::Constructor, 1);
                    ASSERT_EQ(C::MoveConstructor, 2);
                    ASSERT_EQ(C::CopyConstructor, 0);
                }

                ASSERT_EQ(C::Destructor, 3);
            }

            ASSERT_EQ(C::Destructor, 3);
        }

        {
            typedef Class< unique_value, __COUNTER__ > C;

            {
                container<> container;
                container.register_binding< storage< dingo::container<>, unique, C > >();

                {
                    // TODO: This is quite stupid, it does allocation, move, than copy in TypeInstanceGetter
                    AssertClass(container.resolve< C >());
                    ASSERT_EQ(C::Constructor, 1);
                    ASSERT_EQ(C::MoveConstructor, 1);
                    ASSERT_EQ(C::CopyConstructor, 1);
                }

                ASSERT_EQ(C::Destructor, 3);
            }

            ASSERT_EQ(C::Destructor, 3);
        }

        {
            typedef Class< unique_value, __COUNTER__ > C;

            {
                container<> container;
                container.register_binding< storage< dingo::container<>, unique, std::unique_ptr< C > >, C, IClass >();

                {
                    AssertClass(*container.resolve< std::unique_ptr< C > >());
                    ASSERT_EQ(C::Constructor, 1);
                    ASSERT_EQ(C::MoveConstructor, 0);
                    ASSERT_EQ(C::CopyConstructor, 0);
                }

                ASSERT_EQ(C::Destructor, 1);
            }

            ASSERT_EQ(C::Destructor, 1);
        }
    }

    TEST(unique, pointer)
    {
        struct unique_pointer {};
        {
            typedef Class< unique_pointer, __COUNTER__ > C;

            {
                container<> container;
                container.register_binding< storage< dingo::container<>, unique, C* >, C, IClass >();
                AssertClass(container.resolve< C >());
                ASSERT_EQ(C::Constructor, 1);
                ASSERT_EQ(C::CopyConstructor, 1); // TODO: this is stupid. There should be no copy, just move.
                ASSERT_EQ(C::MoveConstructor, 0);
            }

            ASSERT_EQ(C::Destructor, 2);
        }

        {
            typedef Class< unique_pointer, __COUNTER__ > C;

            {
                container<> container;
                container.register_binding< storage< dingo::container<>, unique, C* > >();
                auto c = container.resolve< C* >();
                AssertClass(*c);
                ASSERT_EQ(C::Constructor, 1);
                ASSERT_EQ(C::CopyConstructor, 0);
                ASSERT_EQ(C::MoveConstructor, 0);
                ASSERT_EQ(C::Destructor, 0);

                delete c;
                ASSERT_EQ(C::Destructor, 1);
            }

            ASSERT_EQ(C::Destructor, 1);
        }
    }
}
