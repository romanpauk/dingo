#include "pch.h"
#include "dingo/Container.h"
#include "dingo/Tuple.h"
#include "dingo/StorageShared.h"
#include "dingo/StorageSharedCyclical.h"
#include "dingo/StorageUnique.h"
#include "dingo/PerformanceCounter.h"

namespace dingo
{
    struct IClass
    {
        virtual ~IClass() {}
        virtual const std::string& GetName() = 0;
    };

    struct IClass1 : virtual IClass
    {
        virtual ~IClass1() {}
    };

    struct IClass2 : virtual IClass
    {
        virtual ~IClass2() {}
    };

    template < size_t Counter > struct Class : IClass1, IClass2
    {
        Class() : name_("Class") { ++Constructor; }
        ~Class() { ++Destructor; }
        Class(const Class& cls) : name_(cls.name_) { ++CopyConstructor; }
        Class(Class&& cls) : name_(std::move(cls.name_)) { ++MoveConstructor; }

        const std::string& GetName() { return name_; }

        static size_t Constructor;
        static size_t CopyConstructor;
        static size_t MoveConstructor;
        static size_t Destructor;

    private:
        std::string name_;
    };

    template < size_t Counter > size_t Class< Counter >::Constructor;
    template < size_t Counter > size_t Class< Counter >::CopyConstructor;
    template < size_t Counter > size_t Class< Counter >::MoveConstructor;
    template < size_t Counter > size_t Class< Counter >::Destructor;

#define AssertThrow(code, exception) try { (code); std::abort(); } catch(exception&) {}
#define Assert(code) do { if(!(code)) std::abort(); } while(false)

    template< typename Type, typename NonConvertibleTypes > void AssertTypeNotConvertible(Container& container)
    {
        Apply((NonConvertibleTypes*)0, [&](auto element)
        {
            AssertThrow(container.Resolve< decltype(element)::type >(), dingo::TypeNotConvertibleException);
        });
    }

    template < class T > void AssertClass(T&& cls)
    {
        Assert(cls.GetName() == "Class");
    }

    void TestType()
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

    void TestSharedValue()
    {
        typedef Class< __COUNTER__ > C;
        {
            Container container;
            container.RegisterBinding< Storage< Shared, C > >();

            AssertClass(*container.Resolve< C* >());
            AssertClass(container.Resolve< C& >());

            AssertTypeNotConvertible < C, TypeList< C, std::shared_ptr< C >, std::unique_ptr< C > > >(container);

            Assert(C::Constructor == 1);
            Assert(C::Destructor == 0);
            Assert(C::CopyConstructor == 0);
            Assert(C::MoveConstructor == 0);
        }

        {
            Assert(C::Destructor == 1);
        }
    }

    void TestSharedPointer()
    {
        typedef Class< __COUNTER__ > C;

        {
            Container container;
            container.RegisterBinding< Storage< Shared, C* > >();

            AssertClass(*container.Resolve< C* >());
            AssertClass(container.Resolve< C& >());

            AssertTypeNotConvertible< C, TypeList< C, std::shared_ptr< C >, std::unique_ptr< C > > >(container);

            Assert(C::Constructor == 1);
            Assert(C::Destructor == 0);
            Assert(C::CopyConstructor == 0);
            Assert(C::MoveConstructor == 0);
        }

        {
            Assert(C::Destructor == 1);
        }
    }

    void TestSharedSharedPtr()
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

            Assert(C::Constructor == 1);
            Assert(C::Destructor == 0);
            Assert(C::CopyConstructor == 0);
            Assert(C::MoveConstructor == 0);
        }

        {
            Assert(C::Destructor == 1);
        }
    }

    void TestSharedUniquePtr()
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

            Assert(C::Constructor == 1);
            Assert(C::Destructor == 0);
            Assert(C::CopyConstructor == 0);
            Assert(C::MoveConstructor == 0);
        }

        {
            Assert(C::Destructor == 1);
        }
    }

    void TestUniqueValue()
    {
        {
            typedef Class< __COUNTER__ > C;

            {
                Container container;
                container.RegisterBinding< Storage< Unique, C > >();

                {
                    AssertClass(container.Resolve< C&& >());
                    Assert(C::Constructor == 1);
                    Assert(C::MoveConstructor == 2);
                    Assert(C::CopyConstructor == 0);
                }

                Assert(C::Destructor == 3);
            }

            Assert(C::Destructor == 3);
        }

        {
            typedef Class< __COUNTER__ > C;

            {
                Container container;
                container.RegisterBinding< Storage< Unique, C > >();

                {
                    // TODO: This is quite stupid, it does allocation, move, than copy in TypeInstanceGetter
                    AssertClass(container.Resolve< C >());
                    Assert(C::Constructor == 1);
                    Assert(C::MoveConstructor == 1);
                    Assert(C::CopyConstructor == 1);
                }

                Assert(C::Destructor == 3);
            }

            Assert(C::Destructor == 3);
        }

        {
            typedef Class< __COUNTER__ > C;

            {
                Container container;
                container.RegisterBinding< Storage< Unique, std::unique_ptr< C > > >();

                {
                    AssertClass(*container.Resolve< std::unique_ptr< C > >());
                    Assert(C::Constructor == 1);
                    Assert(C::MoveConstructor == 0);
                    Assert(C::CopyConstructor == 0);
                }

                Assert(C::Destructor == 1);
            }

            Assert(C::Destructor == 1);
        }
    }

    void TestUniquePointer()
    {
        {
            typedef Class< __COUNTER__ > C;

            {
                Container container;
                container.RegisterBinding< Storage< Unique, C* > >();
                AssertClass(container.Resolve< C >());
                Assert(C::Constructor == 1);
                Assert(C::CopyConstructor == 1); // TODO: this is stupid. There should be no copy, just move.
                Assert(C::MoveConstructor == 0);
            }

            Assert(C::Destructor == 2);
        }

        {
            typedef Class< __COUNTER__ > C;

            {
                Container container;
                container.RegisterBinding< Storage< Unique, C* > >();
                auto c = container.Resolve< C* >();
                AssertClass(*c);
                Assert(C::Constructor == 1);
                Assert(C::CopyConstructor == 0);
                Assert(C::MoveConstructor == 0);
                Assert(C::Destructor == 0);

                delete c;
                Assert(C::Destructor == 1);
            }

            Assert(C::Destructor == 1);
        }
    }

    void TestMultipleInterfaces()
    {
        typedef Class< __COUNTER__ > C;

        dingo::Container container;
        container.RegisterBinding< Storage< Shared, C >, IClass, IClass1, IClass2 >();

        {
            auto c = container.Resolve< IClass* >();
            Assert(dynamic_cast<C*>(c));
        }

        {
            auto c = container.Resolve< IClass1* >();
            Assert(dynamic_cast<C*>(c));
        }

        {
            auto c = container.Resolve< IClass2* >();
            Assert(dynamic_cast<C*>(c));
        }
    }

    void TestSharedHierarchy()
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

    void TestUniqueHierarchy()
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

    void TestRecursion()
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

        AssertThrow(container.Resolve< A >(), TypeRecursionException);
        AssertThrow(container.Resolve< B >(), TypeRecursionException);
    }

    void TestRecursionCyclical()
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
                AssertThrow(aptr_->GetName(), dingo::TypeNotConstructedException);
                AssertThrow(a_.GetName(), dingo::TypeNotConstructedException);
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

    void TestResolvePerformance()
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

    void TestResolveRollback()
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

        AssertThrow(container.Resolve< C& >(), Ex);
        Assert(A::Constructor == 1);
        Assert(A::Destructor == 1);
        Assert(B::Constructor == 1);
        Assert(B::Destructor == 1);

        container.Resolve< A& >();
        Assert(A::Constructor == 2);
        Assert(A::Destructor == 1);
        AssertThrow(container.Resolve< C >(), Ex);
        Assert(A::Constructor == 2);
        Assert(A::Destructor == 1);
        Assert(B::Constructor == 2);
        Assert(B::Destructor == 2);
    }

    void TestResolveMultiple()
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
            Assert(vector.size() == 2);

            auto list = container.Resolve< std::list< I* > >();
            Assert(list.size() == 2);

            auto set = container.Resolve< std::set< I* > >();
            Assert(set.size() == 2);
        }

        {
            AssertThrow(container.Resolve< std::vector< std::shared_ptr< I > > >(), TypeNotConvertibleException);
        }

        {
            auto& c = container.Resolve< C& >();
            Assert(c.v_.size() == 2);
            Assert(c.l_.size() == 2);
            Assert(c.s_.size() == 2);
        }
    }
}

int main()
{
    using namespace dingo;

    TestType();
    TestSharedValue();
    TestSharedPointer();
    TestSharedSharedPtr();
    TestSharedUniquePtr();

    TestUniqueValue();
    TestUniquePointer();

    TestMultipleInterfaces();

    TestSharedHierarchy();
    TestUniqueHierarchy();

    TestRecursion();
    TestRecursionCyclical();

    // TestResolvePerformance();

    TestResolveRollback();
    TestResolveMultiple();
}

