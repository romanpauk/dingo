#include <dingo/container.h>
#include <dingo/performance_counter.h>
#include <dingo/storage/unique.h>

#include <boost/test/unit_test.hpp>

#include "class.h"
#include "assert.h"

namespace dingo
{
    BOOST_AUTO_TEST_CASE(TestUniqueValue)
    {
        {
            typedef Class< __COUNTER__ > C;

            {
                container container;
                container.register_binding< storage< unique, C > >();

                {
                    AssertClass(container.resolve< C&& >());
                    BOOST_TEST(C::Constructor == 1);
                    BOOST_TEST(C::MoveConstructor == 2);
                    BOOST_TEST(C::CopyConstructor == 0);
                }

                BOOST_TEST(C::Destructor == 3);
            }

            BOOST_TEST(C::Destructor == 3);
        }

        {
            typedef Class< __COUNTER__ > C;

            {
                container container;
                container.register_binding< storage< unique, C > >();

                {
                    // TODO: This is quite stupid, it does allocation, move, than copy in TypeInstanceGetter
                    AssertClass(container.resolve< C >());
                    BOOST_TEST(C::Constructor == 1);
                    BOOST_TEST(C::MoveConstructor == 1);
                    BOOST_TEST(C::CopyConstructor == 1);
                }

                BOOST_TEST(C::Destructor == 3);
            }

            BOOST_TEST(C::Destructor == 3);
        }

        {
            typedef Class< __COUNTER__ > C;

            {
                container container;
                container.register_binding< storage< unique, std::unique_ptr< C > > >();

                {
                    AssertClass(*container.resolve< std::unique_ptr< C > >());
                    BOOST_TEST(C::Constructor == 1);
                    BOOST_TEST(C::MoveConstructor == 0);
                    BOOST_TEST(C::CopyConstructor == 0);
                }

                BOOST_TEST(C::Destructor == 1);
            }

            BOOST_TEST(C::Destructor == 1);
        }
    }

    BOOST_AUTO_TEST_CASE(TestUniquePointer)
    {
        {
            typedef Class< __COUNTER__ > C;

            {
                container container;
                container.register_binding< storage< unique, C* > >();
                AssertClass(container.resolve< C >());
                BOOST_TEST(C::Constructor == 1);
                BOOST_TEST(C::CopyConstructor == 1); // TODO: this is stupid. There should be no copy, just move.
                BOOST_TEST(C::MoveConstructor == 0);
            }

            BOOST_TEST(C::Destructor == 2);
        }

        {
            typedef Class< __COUNTER__ > C;

            {
                container container;
                container.register_binding< storage< unique, C* > >();
                auto c = container.resolve< C* >();
                AssertClass(*c);
                BOOST_TEST(C::Constructor == 1);
                BOOST_TEST(C::CopyConstructor == 0);
                BOOST_TEST(C::MoveConstructor == 0);
                BOOST_TEST(C::Destructor == 0);

                delete c;
                BOOST_TEST(C::Destructor == 1);
            }

            BOOST_TEST(C::Destructor == 1);
        }
    }
}
