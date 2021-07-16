#include <dingo/container.h>
#include <dingo/performance_counter.h>
#include <dingo/storage/unique.h>

#include <boost/test/unit_test.hpp>

namespace dingo
{
    BOOST_AUTO_TEST_CASE(TestResolvePerformance)
    {
        struct A {};
        struct B { B(A&&) {} };
        struct C { C(A&&, B&&) {} };

        performance_counter counter;
        performance_counter counter2;

        container container;
        container.register_binding< storage< unique, A > >();
        container.register_binding< storage< unique, B > >();
        container.register_binding< storage< unique, C > >();

        const size_t N = 1000000;
        for (size_t i = 0; i < N; ++i)
        {
            counter.start();
            container.resolve< C >();
            counter.stop();
        }

        /*
        std::cout << N << " Resolve() took " << counter.get_elapsed_time() << std::endl;

        // const size_t N = 10000000;
        for (size_t i = 0; i < N; ++i)
        {
            counter2.start();
            A a;
            B b(A());
            volatile C c(A(), B());
            counter2.stop();
        }

        std::cout << N << " Fake() took " << counter2.GetElapsed() << std::endl;
        */
    }
}