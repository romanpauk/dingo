# Dingo
Dependency Injection Container for C++, with DI, next-gen and an 'o' in a name. Header only.

Tested with:
- C++17, C++20, C++23
- GCC 9-12
- Clang 11-15
- Visual Studio 2019, 2022

[![Build](https://github.com/romanpauk/dingo/actions/workflows/build.yaml/badge.svg?branch=master)](https://github.com/romanpauk/dingo/actions?query=branch%3Amaster++)

## Introduction
Dingo is a dependency injection library that preforms recursive instantiation of registered types. It has no dependencies except for the C++ standard library. 

Tests are using [google/googletest](https://github.com/google/googletest), benchmarks are using [google/benchmark](https://github.com/google/benchmark).

### Quick Example

```c++
// Classes to be managed by the container. Note that there is not special code required 
// for the type to be used by the container and conversions are applied automatically based
// on instance scope.
struct A { A() {} };
struct B { B(A&, std::shared_ptr<A>) {} };
struct C { C(B*, std::unique_ptr<B>&, A&) {} };

// Registration into container with different instance scopes
container<> container;
container.register_binding<storage<shared, std::shared_ptr<A>>>();
container.register_binding<storage<shared, std::unique_ptr<B>>>();
container.register_binding<storage<unique, C>>();

// Resolving the type C through container will instantiate A, B, and call C's constructor
// with required arguments.
C c = container.resolve<C>();
``` 

### Features

#### Non-intrusive Class Registration
Required dependencies of registered types can be detected automatically from a list initialization with a highest number of arguments. It is also possbile to manually specify a constructor, static method or lambda function that should be used to construct the type during resolution. See [dingo/class_factory.h](include/dingo/class_factory.h) for details.

```c++
container<> container;
container.register_binding<storage<external, double>>(1.1);

struct A { 
    A(int);
    A(double);
};

// Register A that will be instantiated by calling A(double)
container.register_binding<storage<unique, A, constructor<A(double)>>>();

struct B {
    static B factory();
};

// Register B that will be instantiated by calling B::factory()
container.register_binding<storage<unique, B, function<&B::factory>>>();

struct C {
    C(double);
};

// Register C that will be instantiated by calling provided lambda function
container.register_binding<storage<unique, C>(callable([](double val){ return C(val * 2); }));
``` 

The container tries to preserve usual type semantics of types it manages, without imposing any requirements on managed types. It tries to be as transparent as possible to the managed type, meaning moves are used where they would be normally used, std::make_shared is used to create std::shared_ptr instances and so on. Registered type can be automatically converted to types that are 'reachable' from the base type using usual conversions - dereferencing, taking an address, downcasting and referencing downcasted types is allowed based on managed type lifetime.

#### Scopes

Scopes are policies stating how the container mangages an instance creation and lifetime.

##### External
The container refers to an already existing instance. It can eventually take the ownership by moving the instance inside external storage. See [dingo/storage/external.h](include/dingo/storage/external.h) for allowed conversions for accessing external instances.

```c++
struct A {} a;
container<> container;
container.register_binding<storage<external, A*>>(&a);
A& a = container.resolve<A&>();
```

##### Unique
The container creates unique instance for each resolution. See [dingo/storage/unique.h](include/dingo/storage/unique.h) for allowed conversions for accessing unique instances.

```c++
struct A {};
container<> container;
container.register_binding<storage<unique, A>>();
auto a = container.resolve<A>();
```

##### Shared
The container caches the instance for subsequent resolutions. See [dingo/storage/shared.h](include/dingo/storage/shared.h) for allowed conversions for accessing shared instances.

```c++
struct A {};
container<> container;
container.register_binding<storage<shared, A>>();
assert(container.resolve<A*>() == container.resolve<A*>());
```

##### Shared-cyclical
The container caches the instance and allows to inject instances into object graphs with cycles. This is implemented using two-phase construction. The limitation is that the types can't have virtual bases and are unusable during constructors. It is quite a hack but not all code bases have clean, acyclical dependencies. See [dingo/storage/shared_cyclical.h](include/dingo/storage/shared_cyclical.h) for allowed conversions for accessing shared cycle-aware instances.

```c++
struct A;
struct B;
struct A { 
    A(B& b): b_(b) {} 
    B& b_; 
};
struct B { 
    B(A& a): a_(a) {} 
    A& a_; 
};

container<> container;
container.register_binding<storage<shared_cyclical, std::shared_ptr<A>>>();
container.register_binding<storage<shared_cyclical, B>>();

// Returns instance of A that has correctly set b_ member to an instance of B,
// and instance of B has correctly set a_ member to an instance of A. Note that
// allowed conversions are supported with cycles, too.
A& a = container.resolve<A&>();
```

This storage type disallows registering virtual base classes as it is not always possible to calculate the cast from Derived to Base without constructing the type first, and with cyclical storage, it is not always possible to construct it first.

#### Runtime-based Usage
Container can be used from multiple modules, specifically it can be filled in one module and used in other modules. This is a requirement for larger projects with not so clean dependencies. Compared to the compile-time DI solutions, errors are propagated in runtime as exceptions.

#### Exception-safe
When exception occurs in user-provided code, the state of the container is not changed. Implemented using roll-back.

#### Extensible
Template class specializations are used to tweak the behavior and functionality for different storage types, instance creation or passing instances through type-erasure boundary. Standard RTTI is optional and emulation can be used. Single-module-single-container use-case is optimized with static type maps.

#### Usable as Service Locator
It is possible to register a binding to the type under a key that is a base class of the type. As an upcast is compiled at the time of a registration, multiple inheritance is correctly supported. One instance can be resolved through multiple interfaces.

```c++
struct IA {};
struct A: IA {};

container<> container;
container.register_binding<storage<shared, A>, IA>();
IA& instance = container.resolve<IA&>();
``` 

#### Support for Annotated Types
It is possible to register different implementations with the same interface, disambiguating the registration with an user-provided tag. See [test/annotated.cpp](test/annotated.cpp).

#### Support for Constructing Unmanaged Types
It is possible to let a container construct an unknown type using registered dependencies to construct it.

```c++
struct A {};
struct B { A& a; };

container<> container;
container.register_binding<storage<shared, A>>();
B b = container.construct<B>();
```

#### Unit Tests
The functionality is covered with tests written using google test. See available [test coverage](test).


