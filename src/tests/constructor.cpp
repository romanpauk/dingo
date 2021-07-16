#include <dingo/class_constructor.h>

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

        class_constructor< A >::invoke();
        class_constructor< B >::invoke(nullptr);
        class_constructor< B* >::invoke(nullptr);
        class_constructor< std::unique_ptr< B > >::invoke(nullptr);
        class_constructor< std::shared_ptr< B > >::invoke(nullptr);
        class_constructor< std::optional< B > >::invoke(nullptr);
    }

}