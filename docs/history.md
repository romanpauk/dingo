# Motivation and History

This document explains where Dingo came from and why the current design looks
the way it does.

It is not an argument that dependency injection is always the right answer, or
that this library is the right fit for every codebase. It is background for
readers who want to understand the constraints that shaped the project.

I first ran into the DI-container problem on a large legacy project in 2012,
still using a pre-C++11 compiler. Constructor injection was already in use, but
the wiring code that invoked those constructors had become unmaintainable:
classes with 80 or so constructor parameters, hundreds of classes overall, and a
single wiring `.cpp` compiled into multiple binaries until the `#ifdef` maze
became unreadable.

Large-scale refactoring was not on the table because it did not map directly to
business value and carried real delivery risk. The DI work grew out of that
constraint.

## First Iteration

To get away from one massive `.cpp` constructing massive classes, I built a
first DI container based on a form of automated setter injection. It was
intrusive: every class with injected dependencies had to derive from a common
base, and the injected dependencies lived in smart-pointer-like members.

That version at least removed the need to spell out gigantic constructor calls.
It also supported cycles and, as a bonus, let us use the smart pointer's
`operator->` to assert that the correct locks were taken when crossing from a
multi-threaded domain into a single-threaded one. We could also compile just the
wiring in a unit test, which helped catch missing dependencies before runtime.

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

As part of an internal innovation project, I extended the design to detect
constructor dependencies automatically and invoke the constructor directly. That
meant the class no longer needed smart-pointer-like members for injection.

This was still the pre-C++11 era, so variadic templates were emulated with
generated code for 1..N arguments. The previous example turned into something
closer to this:

```c++
    class Service: public IService {
        INJECT(Service(IOtherService& other_service)):
            other_service_(other_service)
        {}

        void call() { other_service_.do_work(); }

        IOtherService& other_service_;
    };
```

The `INJECT` macro declared a typedef that could then be matched to a template
specialization, which made constructor-argument types retrievable.

It was still a limited design: all instance lifetimes were shared, the container
had to manage all instances, and everything had to be injected through
interfaces.

## Third Iteration

Developers liked the second iteration because it looked generic. The problem was
that it only looked generic. Under the hood it still had strong structural
limits, and teams kept trying to push through those limits instead of removing
them at the core.

That is what led to the current direction: a design that tries to stay general
without imposing ownership rules on user types and without forcing everything
through framework-specific protocols.

Instead of requiring every type to be fully managed by the container and always
resolved through interfaces, the current iteration tries to work with what is
already there, in the form it is already in, with semantics that a C++ developer
could also write manually.

That third iteration was a large step forward, but it still had hard limits.
There was no variant support, no array support, and heavily nested handle types
such as `std::shared_ptr<std::unique_ptr<...>>` were out of reach. The codebase
also became increasingly complex. Even when the behavior was correct, compiler
errors during refactoring were often hard to decipher, which made structural
changes slower and less enjoyable than they should have been.

Those limitations mattered for two reasons. The missing features meant the
"generic" story still had obvious holes, and the implementation complexity made
it too expensive to keep pushing the design forward in small safe steps.

## Fourth Iteration

The fourth iteration became practical with the arrival of LLMs. The problem was
no longer only about what the language could express, but about how much
mechanical template work, refactoring, and compiler-error archaeology one person
could realistically carry at a time.

LLMs did not invent the design goals, but they changed the economics of working
on them. That made it realistic to simplify old machinery, explore broader type
support, and keep iterating on the architecture without getting stuck in the
cost of deciphering every metaprogramming failure by hand.
