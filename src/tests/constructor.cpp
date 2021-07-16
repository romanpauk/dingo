#include <dingo/constructor.h>

#include <boost/test/unit_test.hpp>

namespace dingo
{
    BOOST_AUTO_TEST_CASE(constructor_test)
    {
        struct A {};
        struct B
        {
            B(std::shared_ptr< A >) {}
        };

        constructor< A >::invoke();
        constructor< B >::invoke(nullptr);
        constructor< B* >::invoke(nullptr);
        constructor< std::unique_ptr< B > >::invoke(nullptr);
        constructor< std::shared_ptr< B > >::invoke(nullptr);
        constructor< std::optional< B > >::invoke(nullptr);
    }

}