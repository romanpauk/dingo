#include "dingo/Container.h"
#include "dingo/Tuple.h"
#include "dingo/StorageShared.h"
#include "dingo/StorageSharedCyclical.h"
#include "dingo/StorageUnique.h"
#include "dingo/PerformanceCounter.h"

#include <boost/test/unit_test.hpp>

#include "class.h"
#include "assert.h"

namespace dingo
{
    BOOST_AUTO_TEST_CASE(TestType)
    {
        struct A {};
        struct B
        {
            B(std::shared_ptr< A >) {}
        };

        TypeConstructor< B >::Construct(nullptr);
        TypeConstructor< B* >::Construct(nullptr);
        TypeConstructor< std::unique_ptr< B > >::Construct(nullptr);
        TypeConstructor< std::shared_ptr< B > >::Construct(nullptr);
        TypeConstructor< std::optional< B > >::Construct(nullptr);

    }

    BOOST_AUTO_TEST_CASE(TestSharedValue)
    {
        typedef Class< __COUNTER__ > C;
        {
            Container container;
            container.RegisterBinding< Storage< Shared, C > >();

            AssertClass(*container.Resolve< C* >());
            AssertClass(container.Resolve< C& >());

            AssertTypeNotConvertible < C, TypeList< C, std::shared_ptr< C >, std::unique_ptr< C > > >(container);

            BOOST_TEST(C::Constructor == 1);
            BOOST_TEST(C::Destructor == 0);
            BOOST_TEST(C::CopyConstructor == 0);
            BOOST_TEST(C::MoveConstructor == 0);
        }

        {
            BOOST_TEST(C::Destructor == 1);
        }
    }

    BOOST_AUTO_TEST_CASE(TestSharedPointer)
    {
        typedef Class< __COUNTER__ > C;

        {
            Container container;
            container.RegisterBinding< Storage< Shared, C* > >();

            AssertClass(*container.Resolve< C* >());
            AssertClass(container.Resolve< C& >());

            AssertTypeNotConvertible< C, TypeList< C, std::shared_ptr< C >, std::unique_ptr< C > > >(container);

            BOOST_TEST(C::Constructor == 1);
            BOOST_TEST(C::Destructor == 0);
            BOOST_TEST(C::CopyConstructor == 0);
            BOOST_TEST(C::MoveConstructor == 0);
        }

        {
            BOOST_TEST(C::Destructor == 1);
        }
    }

    BOOST_AUTO_TEST_CASE(TestSharedSharedPtr)
    {
        typedef Class< __COUNTER__ > C;

        {
            Container container;
            container.RegisterBinding< Storage< Shared, std::shared_ptr< C > > >();

            AssertClass(*container.Resolve< C* >());
            AssertClass(container.Resolve< C& >());
            AssertClass(*container.Resolve< std::shared_ptr< C > >());
            AssertClass(*container.Resolve< std::shared_ptr< C >& >());
            AssertClass(**container.Resolve< std::shared_ptr< C >* >());

            AssertTypeNotConvertible< C, TypeList< C, std::unique_ptr< C > > >(container);

            BOOST_TEST(C::Constructor == 1);
            BOOST_TEST(C::Destructor == 0);
            BOOST_TEST(C::CopyConstructor == 0);
            BOOST_TEST(C::MoveConstructor == 0);
        }

        {
            BOOST_TEST(C::Destructor == 1);
        }
    }

    BOOST_AUTO_TEST_CASE(TestSharedUniquePtr)
    {
        typedef Class< __COUNTER__ > C;

        {
            Container container;
            container.RegisterBinding< Storage< Shared, std::unique_ptr< C > > >();

            AssertClass(*container.Resolve< C* >());
            AssertClass(container.Resolve< C& >());
            AssertClass(*container.Resolve< std::unique_ptr< C >& >());
            AssertClass(**container.Resolve< std::unique_ptr< C >* >());

            AssertTypeNotConvertible< C, TypeList< C, std::unique_ptr< C > > >(container);

            BOOST_TEST(C::Constructor == 1);
            BOOST_TEST(C::Destructor == 0);
            BOOST_TEST(C::CopyConstructor == 0);
            BOOST_TEST(C::MoveConstructor == 0);
        }

        {
            BOOST_TEST(C::Destructor == 1);
        }
    }

    BOOST_AUTO_TEST_CASE(TestUniqueValue)
    {
        {
            typedef Class< __COUNTER__ > C;

            {
                Container container;
                container.RegisterBinding< Storage< Unique, C > >();

                {
                    AssertClass(container.Resolve< C&& >());
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
                Container container;
                container.RegisterBinding< Storage< Unique, C > >();

                {
                    // TODO: This is quite stupid, it does allocation, move, than copy in TypeInstanceGetter
                    AssertClass(container.Resolve< C >());
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
                Container container;
                container.RegisterBinding< Storage< Unique, std::unique_ptr< C > > >();

                {
                    AssertClass(*container.Resolve< std::unique_ptr< C > >());
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
                Container container;
                container.RegisterBinding< Storage< Unique, C* > >();
                AssertClass(container.Resolve< C >());
                BOOST_TEST(C::Constructor == 1);
                BOOST_TEST(C::CopyConstructor == 1); // TODO: this is stupid. There should be no copy, just move.
                BOOST_TEST(C::MoveConstructor == 0);
            }

            BOOST_TEST(C::Destructor == 2);
        }

        {
            typedef Class< __COUNTER__ > C;

            {
                Container container;
                container.RegisterBinding< Storage< Unique, C* > >();
                auto c = container.Resolve< C* >();
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

    BOOST_AUTO_TEST_CASE(TestMultipleInterfaces)
    {
        typedef Class< __COUNTER__ > C;

        dingo::Container container;
        container.RegisterBinding< Storage< Shared, C >, IClass, IClass1, IClass2 >();

        {
            auto c = container.Resolve< IClass* >();
            BOOST_TEST(dynamic_cast<C*>(c));
        }

        {
            auto c = container.Resolve< IClass1* >();
            BOOST_TEST(dynamic_cast<C*>(c));
        }

        {
            auto c = container.Resolve< IClass2* >();
            BOOST_TEST(dynamic_cast<C*>(c));
        }
    }

    BOOST_AUTO_TEST_CASE(TestSharedHierarchy)
    {
        struct S : Class< __COUNTER__ >
        {};

        struct U : Class< __COUNTER__ >
        {
            U(S& s1)
            {
                AssertClass(s1);
            }
        };

        struct B : Class< __COUNTER__ >
        {
            B(S s1, S& s2, S* s3, std::shared_ptr< S >* s4, std::shared_ptr< S >& s5, std::shared_ptr< S > s6,
                U u1, U& u2, U* u3, std::unique_ptr< U >* u4, std::unique_ptr< U >& u5
            )
            {
                AssertClass(s1);
                AssertClass(s2);
                AssertClass(*s3);
                AssertClass(**s4);
                AssertClass(*s5);
                AssertClass(*s6);

                AssertClass(u1);
                AssertClass(u2);
                AssertClass(*u3);
                AssertClass(**u4);
                AssertClass(*u5);
            }
        };

        Container container;
        container.RegisterBinding< Storage< Shared, std::shared_ptr< S > > >();
        container.RegisterBinding< Storage< Shared, std::unique_ptr< U > > >();
        container.RegisterBinding< Storage< Shared, B > >();

        container.Resolve< B& >();
    }

    BOOST_AUTO_TEST_CASE(TestUniqueHierarchy)
    {
        struct S
        {};

        struct U
        {};

        struct B
        {
            B(std::shared_ptr< S >&&) {}
        };

        Container container;
        container.RegisterBinding< Storage< dingo::Unique, std::shared_ptr< S > > >();
        container.RegisterBinding< Storage< dingo::Unique, std::unique_ptr< U > > >();
        container.RegisterBinding< Storage< dingo::Shared, B > >();

        container.Resolve< B& >();
    }

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

        Container container;
        container.RegisterBinding< Storage< dingo::Shared, std::shared_ptr< A > > >();
        container.RegisterBinding< Storage< dingo::Shared, B > >();

        BOOST_CHECK_THROW(container.Resolve< A >(), TypeRecursionException);
        BOOST_CHECK_THROW(container.Resolve< B >(), TypeRecursionException);
    }

#if 0
    BOOST_AUTO_TEST_CASE(TestRecursionCyclical)
    {
        struct B;
        struct A: Class< __COUNTER__ >
        {
            A(B& b): b_(b) {}

            B& b_;
        };

        struct B: Class< __COUNTER__ >
        {
            B(std::shared_ptr< A > aptr, A& a): aptr_(aptr), a_(a)
            {
                BOOST_CHECK_THROW(aptr_->GetName(), dingo::TypeNotConstructedException);
                BOOST_CHECK_THROW(a_.GetName(), dingo::TypeNotConstructedException);
            }

            std::shared_ptr< A > aptr_;
            A& a_;
        };

        Container container;
        container.RegisterBinding< Storage< dingo::SharedCyclical, std::shared_ptr< A > > >();
        container.RegisterBinding< Storage< dingo::SharedCyclical, B > >();

        auto& a = container.Resolve< A& >();
        AssertClass(a);
        AssertClass(a.b_);

        auto& b = container.Resolve< B& >();
        AssertClass(b);
        AssertClass(b.a_);
        AssertClass(*b.aptr_);       
    }
#endif 

#if 0
    BOOST_AUTO_TEST_CASE(TestResolvePerformance)
    {
        struct A {};
        struct B { B(A&&) {} };
        struct C { C(A&&, B&&) {} };

        PerformanceCounter counter;
        PerformanceCounter counter2;

        Container container;
        container.RegisterBinding< Storage< dingo::Unique, A > >();
        container.RegisterBinding< Storage< dingo::Unique, B > >();
        container.RegisterBinding< Storage< dingo::Unique, C > >();

        const size_t N = 1000000;
        for (size_t i = 0; i < N; ++i)
        {
            counter.Start();
            container.Resolve< C >();
            counter.Stop();
        }

        std::cout << N << " Resolve() took " << counter.GetElapsed() << std::endl;

        /*
                // const size_t N = 10000000;
                for (size_t i = 0; i < N; ++i)
                {
                    counter2.Start();
                    A a;
                    B b(A());
                    C c(A(), B());
                    counter2.Stop();
                }

                std::cout << N << " Fake() took " << counter2.GetElapsed() << std::endl;
        */
    }
#endif

    BOOST_AUTO_TEST_CASE(TestResolveRollback)
    {
        typedef Class< __COUNTER__ > A;
        typedef Class< __COUNTER__ > B;
        struct Ex {};
        struct C
        {
            C(A&, B&) { throw Ex(); }
        };

        Container container;
        container.RegisterBinding< Storage< Shared, A > >();
        container.RegisterBinding< Storage< Shared, B > >();
        container.RegisterBinding< Storage< Shared, C > >();

        BOOST_CHECK_THROW(container.Resolve< C& >(), Ex);
        BOOST_TEST(A::Constructor == 1);
        BOOST_TEST(A::Destructor == 1);
        BOOST_TEST(B::Constructor == 1);
        BOOST_TEST(B::Destructor == 1);

        container.Resolve< A& >();
        BOOST_TEST(A::Constructor == 2);
        BOOST_TEST(A::Destructor == 1);
        BOOST_CHECK_THROW(container.Resolve< C >(), Ex);
        BOOST_TEST(A::Constructor == 2);
        BOOST_TEST(A::Destructor == 1);
        BOOST_TEST(B::Constructor == 2);
        BOOST_TEST(B::Destructor == 2);
    }

    BOOST_AUTO_TEST_CASE(TestResolveMultiple)
    {
        struct I {};
        struct A: I {};
        struct B: I {};
        struct C 
        {
            C(std::vector< I* > v, std::list< I* > l, std::set< I* > s) : v_(v), l_(l), s_(s) {}

            std::vector< I* > v_;
            std::list< I* > l_;
            std::set< I* > s_;
        };

        Container container;
        container.RegisterBinding< Storage< Shared, A >, A, I >();
        container.RegisterBinding< Storage< Shared, B >, B, I >();
        container.RegisterBinding< Storage< Shared, C > >();

        {
            auto vector = container.Resolve< std::vector< I* > >();
            BOOST_TEST(vector.size() == 2);

            auto list = container.Resolve< std::list< I* > >();
            BOOST_TEST(list.size() == 2);

            auto set = container.Resolve< std::set< I* > >();
            BOOST_TEST(set.size() == 2);
        }

        {
            BOOST_CHECK_THROW(container.Resolve< std::vector< std::shared_ptr< I > > >(), TypeNotConvertibleException);
        }

        {
            auto& c = container.Resolve< C& >();
            BOOST_TEST(c.v_.size() == 2);
            BOOST_TEST(c.l_.size() == 2);
            BOOST_TEST(c.s_.size() == 2);
        }
    }
}