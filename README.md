# Dingo

Dependency Injection Container for C++, with DI, next-gen and an 'o' in a name. Header only.

Tested with:

- C++17, C++20, C++23
- GCC 9-12
- Clang 11-15
- Visual Studio 2019, 2022

[![Build](https://github.com/romanpauk/dingo/actions/workflows/build.yaml/badge.svg?branch=master)](https://github.com/romanpauk/dingo/actions?query=branch%3Amaster++)

Features Overview:

- [Non-intrusive Class Registration](#non-intrusive-class-registration)
- [Customizable Scopes](#scopes)
- [Customizable Factories](#factories)
- [Service Locator Pattern](#service-locator)
- [Runtime-based Resolution](#runtime-based-resolution)
- [Customizable RTTI](#customizable-rtti)
- [Container Nesting](#container-nesting)
- [Static and Dynamic Type Maps](#static-and-dynamic-type-maps)
- [Annotated Types](#annotated-types)

## Introduction

Dingo is a dependency injection library that preforms recursive instantiation of registered types. It has no dependencies except for the C++ standard library.  

Tests are using [google/googletest](https://github.com/google/googletest), benchmarks are using [google/benchmark](https://github.com/google/benchmark).

### Quick Example

```c++
// Class types to be managed by the container. Note that there is no special code required 
// for a type to be used by the container and that conversions are applied automatically based
// on an registered type scope.
struct A { A() {} };
struct B { B(A&, std::shared_ptr<A>) {} };
struct C { C(B*, std::unique_ptr<B>&, A&) {} };

container<> container;

// Register type A with a shared scope, stored as std::shared_ptr<A>
container.register_type<scope<shared>, storage<std::shared_ptr<A>>>();

// Register type B with a shared scope, stored as std::unique_ptr<B>
container.register_type<scope<shared>, storage<std::unique_ptr<B>>>();

// Register type C with an unique scope, stored as plain C
container.register_type<scope<unique>, storage<C>>();

// Resolving the type C will instantiate required dependencies
// (types A and B) and inject the instances based on their scopes into C.
// As C is in unique scope, each resolve<C> will return new C instance.
// As A and B are in shared scope, each C will get the same A's and B's.
C c = container.resolve<C>();

struct D {
    A& a;
    B* b;
};

// Create an un-managed type using dependencies from the container
D d = container.construct<D>();

```

### Features

#### Non-intrusive Class Registration

Managed types do not need to adhere to any special protocol. Constructor auto-detection is attempted, but it is possible to provide a custom factory to use for type construction.
During the type registration, user can customize the following: factory to create an instance, instance scope, type of the stored instance, allowed conversions for resolution and interfaces that can be used to resolve the instance. The customization is done through policies applied during a register_type call. Some of the policies can be ommited if it is possible to deduce them from other policies.

```c++
container<> container;
struct A {};

// Example of registering class A:
container.register_type<
    scope<unqiue>,                  // using unique scope
    factory<constructor<A>>,        // using constructor-detecting factory 
    storage<std::unqiue_ptr<A>>,    // stored as unique_ptr<A>
    interface<A>                    // resolvable as type A
>();

// As some policies can be deduced from the others, the above can be simplified to:
container.register_type<
    scope<unqiue>, 
    storage<std::unqiue_ptr<A>>
>();
```

See [dingo/type_registration.h](include/dingo/type_registration.h) for details.

#### Instance Storage

Type instances are stored in a form that is specified during a registration. Usual semantics of the stored type is respected (example: if a type is stored as std::shared_ptr, it will be created using std::make_shared).
Stored type, together with type's scope, determines what conversions can be applied to an instance during resolution of other instance's dependencies. Allowed conversions are different for each scope,
as not all conversions can be performed safely or without unwanted side-effects. Conversions that could be applied are dereferencing, taking an address, down-casting and referencing down-casted types.

#### Factories

Factories parametrize how a type is instantiated. Factory type is deduced and does not have to be specified during a type registration. Factories can generally be stateless or stateful.

##### Automatically-deducing Constructor Factory

Constructor to create a type instance is determined automatically by attempting a list initialization, going from a pre-configured number of arguments down to zero. In a case the constructor is not determined, compile assert occurs.
This factory type is a default one so it does not have to be specified.

```c++
container<> container;
container.register_type<scope<external>, storage<double>>(1.1);

struct A {
    A(double, double);
};

// Detect constructor with highest arity
container.register_type<scope<unique>, storage<A> /*, factory<constructor<A>> */>();
```

See [dingo/factory/constructor.h](include/dingo/factory/constructor.h) for details.

##### Concrete Constructor Factory

In a case an automatic deduction fails due to an ambiguity, it is possible to override the constructor by specifying an constructor overload that will be used.

```c++
container<> container;
container.register_type<scope<external>, storage<double>>(1.1);

struct A {
    A(int);
    A(double);
};

// Register A that will be instantiated by calling A(double). 
// This case needs to be manually disambiguated otherwise compile assert happens.
container.register_type<scope<unique>, storage<A>, factory<constructor<A(double)>>>();
```

See [dingo/factory/constructor.h](include/dingo/factory/constructor.h) for details.

##### Static Function Factory

Static function factory allows a static function to be used for an instance creation.

```c++
container<> container;

struct A {
    static A factory();
private:
    A();
};

// Register A that will be instantiated by calling A::factory()
container.register_type<scope<unique>, storage<A>, factory<function<&A::factory>>>();
```

See [dingo/factory/function.h](include/dingo/factory/function.h) for details.

##### Generic Stateful Factory

Using a stateful factory, any functor can be used to construct a type.

```c++
container<> container;
container.register_type<scope<external>, storage<double>>(1.1);

struct A {
    A(double);
};

// Register A that will be instantiated by calling provided lambda function
container.register_type<scope<unique>, storage<A>>(callable([](double val){ return A(val * 2); }));
```

See [dingo/factory/callable.h](include/dingo/factory/callable.h) for details.

#### Scopes

Scopes parametrize stored instances' lifetime. Based on a stored type, each scope defines a list of allowed conversions that are applied to the stored type during resolution to satisfy dependencies of other types. Both cyclical and a-cyclical dependency graphs are supported.

##### External Scope

An already existing instance is referred. The scope can eventually take the ownership by moving the instance inside into the scope storage.

```c++
struct A {} a;
container<> container;
container.register_type<scope<external>, storage<A*>>(&a);
A& a = container.resolve<A&>();
```

See [dingo/storage/external.h](include/dingo/storage/external.h) for allowed conversions for accessing external instances.

##### Unique Scope

An unique instance is created for each resolution. In a case dependencies form a cycle, an exception is thrown.
See [dingo/storage/unique.h](include/dingo/storage/unique.h) for allowed conversions for accessing unique instances.

```c++
struct A {};
container<> container;
container.register_type<scope<unique>, storage<A>>();
auto a = container.resolve<A>();
```

##### Shared Scope

The instance is cached for a subsequent resolutions. In a case dependencies form a cycle, an exception is thrown.
See [dingo/storage/shared.h](include/dingo/storage/shared.h) for allowed conversions for accessing shared instances.

```c++
struct A {};
container<> container;
container.register_type<scope<shared>, storage<A>>();
assert(container.resolve<A*>() == &container.resolve<A&>());
```

##### Shared-cyclical Scope

The instance is cached for a subsequent resolutions and allows to create object graphs with cycles. This is implemented using two-phase construction and thus has some limitations: injected types from a cyclical storage can't be used safely in constructors, and as some conversions require a type to be fully constructed, those are disallowed additionally (injecting an instance through it's virtual base class). The functionality is quite a hack, but not all code bases have a clean, a-cyclical dependencies, so it could be handy.

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
container.register_type<scope<shared_cyclical>, storage<std::shared_ptr<A>>>();
container.register_type<scope<shared_cyclical>, storage<std::shared_ptr<B>>>();

// Returns instance of A that has correctly set b_ member to an instance of B,
// and instance of B has correctly set a_ member to an instance of A. Note that
// allowed conversions are supported with cycles, too.
A& a = container.resolve<A&>();
```

See [dingo/storage/shared_cyclical.h](include/dingo/storage/shared_cyclical.h) for allowed conversions for accessing shared cycle-aware instances.

#### Runtime-based Resolution

Container can be used from multiple modules, specifically it can be filled in one module and used in other modules. This is a requirement for larger projects with not so clean dependencies. Compared to the compile-time DI solutions, errors are propagated in runtime as exceptions.

#### Exception-safe

When exception occurs in user-provided code, the state of the container is not changed. Implemented using roll-back.

#### Extensible

Template class specializations are used to tweak the behavior and functionality for different storage types, instance creation or passing instances through type-erasure boundary. Standard RTTI is optional and an emulation can be used. Single-module-single-container use-case is optimized with static type maps.

#### Service Locator

It is possible to register a binding to the type under a key that is a base class of the type. As an upcast is compiled at the time of a registration, multiple inheritance is correctly supported. One instance can be resolved through multiple interfaces.

```c++
struct IA {};
struct A: IA {};

container<> container;
container.register_type<scope<shared>, storage<A>, interface<A, IA>>();
IA& instance = container.resolve<IA&>();
```

#### Annotated Types

It is possible to register different implementations with the same interface, disambiguating the registration with an user-provided tag. See [test/annotated.cpp](test/annotated.cpp).

#### Constructing Unmanaged Types

It is possible to let a container construct an unknown type using registered dependencies to construct it.

```c++
struct A {};
struct B { A& a; };

container<> container;
container.register_type<scope<shared>, storage<A>>();
B b = container.construct<B>();
```

#### Customizable RTTI
For non-RTTI enabled builds, it is possible to parametrize the container with custom RTTI implementation.

#### Static and Dynamic Type Maps
Dynamic type maps are implementing mapping from a type (key) to an instance (value), using std::type_index to represent the key. Static type maps are using template specializations to represent the key without relying on other data structure. This can be used in the case when a limited amount of container instances, denoted by a typed tags, exist, and when the final application is linked in such way that there is just a single counter in the whole application that is used to assign the custom type id.

### Container Nesting
Containers can form a parent-child hierarchy and resolution will traverse the container chain from child to last parent. Calling register_type() returns an implicitly created child container for the type being registered, allowing a per-type configuration to override a global configuration in the container. Nesting is supported for both dynamic and static type maps based containers.

```c++
struct A { int value; };
struct B { int value; };

container<> container;
container.register_type<scope<external>, storage<int>>(1);
container.register_type<scope<unique>, storage<A>>();
// Resolving A will use int(1) as provided by the container.

container.register_type<scope<unique>, storage<B>>() // Override int() for B
    .register_type<scope<external>, storage<int>>(2);
// Resolving B will use int(2) as provided by the B's container.

container<> nested(&container);
nested.register_type<scope<unique>, storage<B>>();
// Resolving B using nested container will use int(1) as provided by the parent container.
```

See [test/nesting.cpp](test/nesting.cpp) for details.

#### Unit Tests

The functionality is covered with tests written using google test. See available [test coverage](test).
