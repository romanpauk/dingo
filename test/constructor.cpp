#include <dingo/class_constructor.h>

#include <gtest/gtest.h>

namespace dingo
{
    TEST(constructor, basic)
    {
        struct A {};
        struct B
        {
            B(std::shared_ptr< A >) {}
        };

        class_constructor< A >::invoke();
        class_constructor< B >::invoke(nullptr);
        delete class_constructor< B* >::invoke(nullptr);
        class_constructor< std::unique_ptr< B > >::invoke(nullptr);
        class_constructor< std::shared_ptr< B > >::invoke(nullptr);
        class_constructor< std::optional< B > >::invoke(nullptr);
    }
}