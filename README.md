# Dingo
Dependency Injection Container for C++, with DI, next-gen and an 'o' in a name.

## Introduction
Dingo is a header-only, dependency injection library that preforms recursive instantiation of registered types. 
It has no dependencies except for the C++ standard library.

### Quick Example

```c++
// Classes to be managed by the container. Note that there is not special code required for the type to become managed.
struct A { A() {} };
struct B { B(A&, std::shared_ptr< A >) {} };
struct C { C(B*, std::unique_ptr< B >&, A&) {} };

// Registration into container with different lifetime policies
container<> container;
container.register_binding< storage< shared, std::shared_ptr< A > > >();
container.register_binding< storage< shared, std::unique_ptr< B > > >();
container.register_binding< storage< unique, C > >();

// Resolving the type C through container will instantiate A, B, and call C's constructor
// with required types.
C c = container.resolve< C >();
``` 

### Features

#### Non-intrusive Class Registration
Required dependencies of registered types are detected automatically from constructor signatures. See [include/dingo/class_factory.h](include/dingo/class_factory.h).

The container tries to preserve usual type semantics of types it manages, without imposing requirements on managed types. It tries to be as transparent as possible to the managed type. Registered type can be automatically converted to types that are 'reachable' from the base type using usual conversions. See [include/dingo/storage/shared.h](include/dingo/storage/shared.h) for example of how object with shared lifetime can be used while resolving it.

#### Runtime-based Usage
Container can be used from multiple modules, specifically it can be filled in one module and used in other modules. This is a requirement for larger projects with not so clean dependencies. Compared to the compile-time DI solutions, errors are propagated in runtime as exceptions.

#### Exception-safe
When exception occurs in user-provided code, the state of the container is not changed. Implemented using roll-back.

#### Flexible
Template class specializations are used to tweak the behavior and functionality for different storage types, instance creation or passing instances through type-erasure boundary. Standard RTTI is optional and emulation can be used. Single-module-single-container use-case is optimized with static type maps.

#### Usable as Service Locator
It is possible to register a binding to the type under a key that is a base class of the type. As an upcast is compiled at the time of a registration, multiple inheritance is correctly supported. One instance can be resolved through multiple interfaces.

```c++
struct IA {};
struct A: IA {};

container<> container;
container.register_binding< storage< shared, A >, IA >();
IA& instance = container.resolve< IA& >();
``` 

#### Support for Annotated Types
It is possible to register different implementations with the same interface, disambiguating the registration with an user-provided tag. See [annotated.cpp](test/annotated.cpp).

#### Support for Cycles
With shared_cyclical storage, cycles between types are supported. This is implemented using two-phase construction. The limitation is that the types can't have
virtual bases and are unusable during constructors. It is quite a hack, but not all code bases have clean, acyclical dependencies.

```c++
struct A;
struct B;
struct A { 
    A(B& b) {} 
    B& b_; 
};
struct B { 
    B(A&) {} 
    A& a_; 
};

container<> container;
container.register_binding< storage< shared, A > >();
container.register_binding< storage< shared, B > >();

// Returns instance of A that has correctly set b_ member to an instance of B,
// and instance of B has correctly set a_ member to an instance of A.
A& a = container.resolve< A& >();
``` 

#### Unit Tests
The functionality is covered with tests written using google test. See available [test coverage](test).


