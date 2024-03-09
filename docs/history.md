# Motivation and History

Dear reader, the following will hopefully explain the over-engineered mess that
the library currently is and why is that so. Keeping aside general wisdom that
"there are better solutions than DI", "DI container is an anti-pattern", "you
should refactor your code" that often is right, yet often also does not help to
tackle the problem, this just shows how the library has evolved in time. It does
not advocate for DI to be the best pattern ever, or this to be the best library,
in fact, if you will find a need to use something like this, in C++, the project
is on the crossing.

The first time I've met DI container concept was on a large legacy project in
2012, using pre-C++11 compiler. Constructor injection was used there, yet the
wiring code invoking the constructors has quickly become unmaintainable due to
having 80 or so parameters and there were hundreds of classes. Also, the wiring
code was single .cpp file compiled in multiple binaries, so soon the #ifdefs
become undecipherable. Did I mention legacy? No one wanted to hear about large
refactoring that does not bring direct business value and can destabilize the
product.

## First Iteration

To solve the issue with one massive .cpp file constructing massive classes, I've
developed first version of DI container, using sort of automated setter
injection. It was intrusive, as each class that had dependencies injected had to
derive from common base class and the injected dependencies were in smart-ptr
like classes. This at least solved the problem of having to call gigantic
constructors. Cycles were supported too, and as a bonus, using smart_ptr's
operator ->, we were able to assert that proper locks are taken when calling
from multi-threaded to single-threaded domain. It was also possible to compile
just the wiring in the unit-test, so there would not be any runtime issues later
with missing dependencies.

It looked something like this:

```c++
    class Service: DI::BaseService, public IService {
        Service():
            other_service_(this)
        {}

        void call() { other_service_->do_work(); }

        DI::ServiceDependency< IOtherService > other_service_;
        // and 80 more members like this...
    };

    // Wiring code:
    container.Register<IService, Service>(); // No need to pass ctor arguments
    IService& service = container.Get<IService>(); // Build/return shared instance
```

## Second Iteration

As part of innovation project, extension was implemented to detect the
dependencies to be injected from the constructor and to invoke the constructor
automatically. So the class no longer had to have smart_ptr like members. This
was still pre-C++11 era, so variadic templates were emulated using generated
code for 1..N arguments. The above example changed to something like this:

```c++
    class Service: public IService {
        INJECT(Service(IOtherService& other_service)):
            other_service_(other_service)
        {}

        void call() { other_service_.do_work(); }

        IOtherService& other_service_;
    };
```

INJECT macro was used to declare a typedef that can than be matched to template
specialization and thus, types of constructor arguments can be retrieved. This
was still very limited as all instances lifetimes were shared, the container had
to manage all instances, everything has to be injected using interfaces etc.

## Third Iteration

Interestingly, developers liked to use the second iteration, as it looked
somehow generic. Problem was that it only looked generic, it was not really
generic under the hood and it had a very strong limits to what can be done and
this caused developers to try to fight it, but it was always a lost time as the
problem was in the core of the library. At that time I've started thinking about
how to approach this problem with generality it requires, without imposing
requirements on user-types or their ownership and with some sort of natural
semantic that developers could expect from C++ library. Instead of requiring of
all types to be managed by the container and to be retrieved through interfaces,
the last iteration tries to work with "what is there, in a form that is there,
in fastest way achievable, in a way developers would be able to write manually".
