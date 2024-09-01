# Dingo

Dependency Injection Container for C++, with DI, next-gen and an 'o' in a name.
Header only.

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
- [Static and Dynamic Containers](#static-and-dynamic-containers)
- [Multibindings](#multibindings)
- [Named Resolution](#named-resolution)
- [Annotated Types](#annotated-types)
- [Customizable Allocation](#customizable-allocation)

## Introduction

Dingo is a dependency injection library that preforms recursive instantiation of
registered types. It has no dependencies except for the C++ standard library.
History of the project is summarized in
[Motivation and History](docs/history.md).

Tests are using [google/googletest](https://github.com/google/googletest),
benchmarks are using [google/benchmark](https://github.com/google/benchmark).

### Quick Example

<!-- { include("examples/quick.cpp", scope="////") -->

Example code included from [examples/quick.cpp](examples/quick.cpp):

```c++
// Class types to be managed by the container. Note that there is no special
// code required for a type to be used by the container and that conversions
// are applied automatically based on an registered type scope.
struct A {
    A() {}
};
struct B {
    B(A&, std::shared_ptr<A>) {}
};
struct C {
    C(B*, std::unique_ptr<B>&, A&) {}
};

container<> container;
// Register struct A with a shared scope, stored as std::shared_ptr<A>
container.register_type<scope<shared>, storage<std::shared_ptr<A>>>();

// Register struct B with a shared scope, stored as std::unique_ptr<B>
container.register_type<scope<shared>, storage<std::unique_ptr<B>>>();

// Register struct C with an unique scope, stored as plain C
container.register_type<scope<unique>, storage<C>>();

// Resolving the struct C will recursively instantiate required dependencies
// (structs A and B) and inject the instances based on their scopes into C.
// As C is in unique scope, each resolve<C> will return new C instance.
// As A and B are in shared scope, each C will get the same instances
// injected.
C c = container.resolve<C>();

struct D {
    A& a;
    B* b;
};

// Construct an un-managed struct using dependencies from the container
D d = container.construct<D>();

// Invoke callable
D e = container.invoke([&](A& a, B* b) { return D{a, b}; });
```

<!-- } -->

To use the library from CMake-based projects, FetchContent or vcpkg can be used.
Alias dingo::dingo is added as an INTERFACE library without any other
dependencies.

<!-- { include("test/fetchcontent/CMakeLists.txt", scope="####") -->

Example code included from
[test/fetchcontent/CMakeLists.txt](test/fetchcontent/CMakeLists.txt):

```CMake
include(FetchContent)
FetchContent_Declare(dingo
                     GIT_REPOSITORY https://github.com/romanpauk/dingo.git
                     GIT_TAG        master)
FetchContent_MakeAvailable(dingo)
target_link_libraries(dingo_fetchcontent_test PRIVATE dingo::dingo)
```

<!-- } -->

### Existing DI Libraries

As creating a generic DI library is tricky, libraries end up different in many
aspects (API, supported C++ constructs, compile-time/run-time, intrusiveness).
Please take a look at few other libraries to find out which one suits the
intended use-case the best:

- [qlibs/di](https://github.com/qlibs/di)
- [boost-ex/di](https://github.com/boost-ext/di)
- [google/fruit](https://github.com/google/fruit)
- [gracicot/kangaru](https://github.com/gracicot/kangaru)
- [ybainier/Hypodermic](https://github.com/ybainier/Hypodermic)

### Features

#### Non-intrusive Class Registration

Managed types do not need to adhere to any special protocol. Constructor
auto-detection is attempted, but it is possible to provide a custom factory to
use for type construction. During the type registration, user can customize the
following: factory to create an instance, instance scope, type of the stored
instance, allowed conversions for resolution and interfaces that can be used to
resolve the instance. The customization is done through policies applied during
a register_type call. Some of the policies can be ommited if it is possible to
deduce them from other policies.

<!-- { include("examples/non_intrusive.cpp", scope="////") -->

Example code included from
[examples/non_intrusive.cpp](examples/non_intrusive.cpp):

```c++
container<> container;
// Registration of a struct A
container.register_type<scope<unique>,           // using unique scope
                        factory<constructor<A>>, // using constructor-detecting
                                                 // factory
                        storage<std::unique_ptr<A>>, // stored as unique_ptr<A>
                        interface<A>                 // resolvable as A
                        >();
// As some policies can be deduced from the others, the above
// registration simplified
container.register_type<scope<unique>, storage<std::unique_ptr<A>>>();
```

<!-- } -->

See [dingo/type_registration.h](include/dingo/type_registration.h) for details.

#### Instance Storage

Type instances are stored in a form that is specified during a registration.
Usual semantics of the stored type is respected (example: if a type is stored as
std::shared_ptr, it will be created using std::make_shared). Stored type,
together with type's scope, determines what conversions can be applied to an
instance during resolution of other instance's dependencies. Allowed conversions
are different for each scope, as not all conversions can be performed safely or
without unwanted side-effects. Conversions that could be applied are
dereferencing, taking an address, down-casting and referencing down-casted
types.

#### Factories

Factories parametrize how a type is instantiated. Factory type is deduced and
does not have to be specified during a type registration. Factories can
generally be stateless or stateful.

##### Automatically-deducing Constructor Factory

Constructor to create a type instance is determined automatically by attempting
a list initialization, going from a pre-configured number of arguments down to
zero. In a case the constructor is not determined, compile assert occurs. This
factory type is a default one so it does not have to be specified.

<!-- { include("examples/factory_constructor.cpp", scope="////") -->

Example code included from
[examples/factory_constructor.cpp](examples/factory_constructor.cpp):

```c++
struct A {
    A(int); // Definition is not required as constructor is not called
    A(double, double) {} // Definition is required as constructor is called
};

container<> container;
container.register_type<scope<external>, storage<double>>(1.1);

// Constructor with a highest arity will be used (factory<> is deduced
// automatically)
container
    .register_type<scope<unique>, storage<A> /*, factory<constructor<A>> */>();
```

<!-- } -->

See [dingo/factory/constructor.h](include/dingo/factory/constructor.h) for
details.

##### Concrete Constructor Factory

In a case an automatic deduction fails due to an ambiguity, it is possible to
override the constructor by specifying an constructor overload that will be
used.

<!-- { include("examples/factory_constructor_concrete.cpp", scope="////") -->

Example code included from
[examples/factory_constructor_concrete.cpp](examples/factory_constructor_concrete.cpp):

```c++
struct A {
    A(int);      // Definition is not required as constructor is not called
    A(double) {} // Definition is required as constructor is called
};

container<> container;
container.register_type<scope<external>, storage<double>>(1.1);

// Register class A that will be constructed using manually selected
// A(double) constructor. Manually disambiguation is required to avoid
// compile time assertion
container.register_type<scope<unique>, storage<A>,
                        factory<constructor<A(double)>>>();
```

<!-- } -->

See [dingo/factory/constructor.h](include/dingo/factory/constructor.h) for
details.

##### Static Function Factory

Static function factory allows a static function to be used for an instance
creation.

<!-- { include("examples/factory_static_function.cpp", scope="////") -->

Example code included from
[examples/factory_static_function.cpp](examples/factory_static_function.cpp):

```c++
// Declare struct A that has an inaccessible constructor
struct A {
    // Provide factory function to construct instance of A
    static A factory() { return A(); }

  private:
    A() = default;
};

container<> container;
// Register A that will be instantiated by calling A::factory()
container
    .register_type<scope<unique>, storage<A>, factory<function<&A::factory>>>();
```

<!-- } -->

See [dingo/factory/function.h](include/dingo/factory/function.h) for details.

##### Generic Stateful Factory

Using a stateful factory, any functor can be used to construct a type.

<!-- { include("examples/factory_callable.cpp", scope="////") -->

Example code included from
[examples/factory_callable.cpp](examples/factory_callable.cpp):

```c++
struct A {
    int value;
};

container<> container;
// Register double that will be passed to lambda function below
container.register_type<scope<external>, storage<int>>(2);
// Register A that will be instantiated by calling provided lambda function
// with arguments resolved using the container.
container.register_type<scope<unique>, storage<A>>(callable([](int value) {
    return A{value * 2};
}));
assert(container.resolve<A>().value == 4);
```

<!-- } -->

See [dingo/factory/callable.h](include/dingo/factory/callable.h) for details.

#### Scopes

Scopes parametrize stored instances' lifetime. Based on a stored type, each
scope defines a list of allowed conversions that are applied to the stored type
during resolution to satisfy dependencies of other types. Both cyclical and
a-cyclical dependency graphs are supported.

##### External Scope

An already existing instance is referred. The scope can eventually take the
ownership by moving the instance inside into the scope storage.

<!-- { include("examples/scope_external.cpp", scope="////") -->

Example code included from
[examples/scope_external.cpp](examples/scope_external.cpp):

```c++
struct A {
} instance;

container<> container;
// Register existing instance of A, stored as a pointer.
container.register_type<scope<external>, storage<A*>>(&instance);
// Resolution will return an existing instance of A casted to required type.
assert(&container.resolve<A&>() == container.resolve<A*>());
```

<!-- } -->

See [dingo/storage/external.h](include/dingo/storage/external.h) for allowed
conversions for accessing external instances.

##### Unique Scope

An unique instance is created for each resolution. In a case dependencies form a
cycle, an exception is thrown. See
[dingo/storage/unique.h](include/dingo/storage/unique.h) for allowed conversions
for accessing unique instances.

<!-- { include("examples/scope_unique.cpp", scope="////") -->

Example code included from
[examples/scope_unique.cpp](examples/scope_unique.cpp):

```c++
struct A {};
container<> container;
// Register struct A with unique scope
container.register_type<scope<unique>, storage<A>>();
// Resolution will get a unique instance of A
container.resolve<A>();
```

<!-- } -->

##### Shared Scope

The instance is cached for a subsequent resolutions. In a case dependencies form
a cycle, an exception is thrown. See
[dingo/storage/shared.h](include/dingo/storage/shared.h) for allowed conversions
for accessing shared instances.

<!-- { include("examples/scope_shared.cpp", scope="////") -->

Example code included from
[examples/scope_shared.cpp](examples/scope_shared.cpp):

```c++
struct A {};
container<> container;
// Register struct A with shared scope
container.register_type<scope<shared>, storage<A>>();
// Resolution will always return the same A instance
assert(container.resolve<A*>() == &container.resolve<A&>());
```

<!-- } -->

##### Shared-cyclical Scope

The instance is cached for a subsequent resolutions and allows to create object
graphs with cycles. This is implemented using two-phase construction and thus
has some limitations: injected types from a cyclical storage can't be used
safely in constructors, and as some conversions require a type to be fully
constructed, those are disallowed additionally (injecting an instance through
it's virtual base class). The functionality is quite a hack, but not all code
bases have a clean, a-cyclical dependencies, so it could be handy.

<!-- { include("examples/scope_shared_cyclical.cpp", scope="////") -->

Example code included from
[examples/scope_shared_cyclical.cpp](examples/scope_shared_cyclical.cpp):

```c++
// Forward-declare structs that have cyclical dependency
struct A;
struct B;

// Declare struct A, note that its constructor is taking arguments of struct B
struct A {
    A(B& b, std::shared_ptr<B> bptr) : b_(b), bptr_(bptr) {}
    B& b_;
    std::shared_ptr<B> bptr_;
};

// Declare struct B, note that its constructor is taking arguments of struct A
struct B {
    B(A& a, A* aptr) : a_(a), aptr_(aptr) {}
    A& a_;
    A* aptr_;
};

container<> container;

// Register struct A with cyclical scope
container.register_type<scope<shared_cyclical>, storage<A>>();
// Register struct B with cyclical scope
container.register_type<scope<shared_cyclical>, storage<std::shared_ptr<B>>>();

// Returns instance of A that has correctly set b_ member to an instance of
// B, and instance of B has correctly set a_ member to an instance of A.
// Conversions are supported with cycles, too.
A& a = container.resolve<A&>();
B& b = container.resolve<B&>();
// Check that the instances are constructed as promised
assert(&a.b_ == &b);
assert(&a.b_ == a.bptr_.get());
assert(&b.a_ == &a);
assert(b.aptr_ == &a);
```

<!-- } -->

See [dingo/storage/shared_cyclical.h](include/dingo/storage/shared_cyclical.h)
for allowed conversions for accessing shared cycle-aware instances.

#### Runtime-based Resolution

Container can be used from multiple modules, specifically it can be filled in
one module and used in other modules. This is a requirement for larger projects
with not so clean dependencies. Compared to the compile-time DI solutions,
errors are propagated in runtime as exceptions.

#### Exception Safety Guarantee

When an exception occurs in a user-provided code during instance resolution, the
state of a container is valid with all successfully resolved instances intact.
Note that as a resolution is a recursive process, the container state can be
modified due to successful resolutions being made before reaching a resolution
that will fail.

#### Extensible

Template class specializations are used to tweak the behavior and functionality
for different storage types, instance creation or passing instances through
type-erasure boundary. Standard RTTI is optional and an emulation can be used.
Single-module-single-container use-case is optimized with static type maps.

#### Service Locator

It is possible to register a binding to the type under a key that is a base
class of the type. As an upcast is compiled at the time of a registration so
multiple inheritance is correctly supported even without using dynamic_cast
operator. One instance can be resolved through multiple interfaces.

<!-- { include("examples/service_locator.cpp", scope="////") -->

Example code included from
[examples/service_locator.cpp](examples/service_locator.cpp):

```c++
// Interface that will be resolved
struct IA {
    virtual ~IA() {}
};
// Struct implementing the interface
struct A : IA {};

container<> container;

// Register struct A, resolvable as interface IA
container.register_type<scope<shared>, storage<A>, interface<IA>>();
// Resolve instance A through interface IA
IA& instance = container.resolve<IA&>();
assert(dynamic_cast<A*>(&instance));
```

<!-- } -->

#### Multibindings

Multibindings allow to resolve a collection of types that are resolvable using
the same interface. The collection can participate in a resolution of other
types. Lambda function can be used to transform the resolved types.

<!-- { include("examples/multibindings.cpp", scope="////") -->

Example code included from
[examples/multibindings.cpp](examples/multibindings.cpp):

```c++
struct IProcessor {
    virtual ~IProcessor() {}
};

template <size_t N> struct Processor : IProcessor {};

container<> container;
// Register multi-bindings collection
container.template register_type_collection<
    scope<shared>, storage<std::vector<IProcessor*>>>();

// Register types under the same interface
container.template register_type<scope<shared>, storage<Processor<0>>,
                                 interface<IProcessor>>();
container.template register_type<scope<shared>, storage<Processor<1>>,
                                 interface<IProcessor>>();

// Resolve the collection
container.template resolve<std::vector<IProcessor*>>();
```

<!-- } -->

#### Named Resolution

Traits used during container construction can specify one or more indexes
allowing each type to be indexed under additional user-specified key to the type
interface. The index data structure can be overridden using template
specialization, with the library providing specializations for std::map,
std::unordered_map and std::array.

<!-- { include("examples/index.cpp", scope="////") -->

Example code included from [examples/index.cpp](examples/index.cpp):

```c++
struct IAnimal {
    virtual ~IAnimal() {}
};

struct Dog : IAnimal {};
struct Cat : IAnimal {};

// Declare traits with std::string based index
struct container_traits : dynamic_container_traits {
    using index_definition_type =
        std::tuple<std::tuple<std::string, index_type::unordered_map>>;
};

container<container_traits> container;
container.template register_indexed_type<
    scope<shared>, storage<Dog>, interface<IAnimal>>(std::string("dog"));

container.template register_indexed_type<
    scope<shared>, storage<Cat>, interface<IAnimal>>(std::string("cat"));

// Resolve an instance of a dog
auto dog = container.template resolve<IAnimal>(std::string("dog"));
```

<!-- } -->

#### Annotated Types

It is possible to register different implementations with the same interface,
disambiguating the registration with an user-provided tag. See
[test/annotated.cpp](test/annotated.cpp).

#### Constructing Unmanaged Types

Unregistered types can be constructed using registered dependencies with
construct() member function.

<!-- { include("examples/construct.cpp", scope="////") -->

Example code included from [examples/construct.cpp](examples/construct.cpp):

```c++
// struct A that will be registered with the container
struct A {};

// struct B that will be constructed using container
struct B {
    A& a;
};

container<> container;
// Register struct A with shared scope
container.register_type<scope<shared>, storage<A>>();
// Construct instance of B, injecting shared instance of A
B b = container.construct<B>();
```

<!-- } -->

#### Invoking Callables

Callable objects can be called using invoke() member function with arguments
provided by the container. Supported callable types are lambdas, std::function
and function pointers.

<!-- { include("examples/invoke.cpp", scope="////") -->

Example code included from [examples/invoke.cpp](examples/invoke.cpp):

```c++
// struct B that will be constructed using container
struct B {
    A& a;
    static B factory(A& a) { return B{a}; }
};
// Construct instance of B, injecting shared instance of A
B b1 = container.invoke([&](A& a) { return B{a}; });
B b2 = container.invoke(std::function<B(A&)>([](auto& a) { return B{a}; }));
B b3 = container.invoke(B::factory);
```

<!-- } -->

#### Customizable RTTI

For non-RTTI enabled builds, it is possible to parametrize the container with
custom RTTI implementation. No library functionality depends on dynamic_cast
conversion. Two RTTI providers are available: dynamic one based on typeid
operator and static one, based on template specializations.

#### Static and Dynamic Containers

Static and dynamic container are just differently parametrized containers using
container traits.

Container type can be parametrized with a custom memory allocator, RTTI provider
and type map implementation. Dynamic containers use standard allocator, operator
typeid() and type map implemented using standard map. Static containers use
template specializations to implement required features, as all types that are
ever present are always different types, allowing static member variables to be
used to store and retrieve required data quickly.

While dynamic containers can be created completely freely, with static
containers it is a user's responsibility to use type tag to distinguish the
containers. The benefit of static containers is a performance gain. The drawback
is slightly harder and limited use.

Note that "static" in this context does not mean "compile-time", both container
parametrizations are fully runtime-based.

##### Caching of Resolved Types with Shared Scope

The container functions as a cache for already resolved types with shared scope,
returning stable references to those types. The initial resolution needs to
happen through some form of dynamic dispatch, as due to runtime nature of the
container, some informations are not present on the caller side. But as soon as
the type is resolved using factory through dynamic dispatch, the result is
cached on the caller side, so subsequent resolution will retrieve it directly
without having to use factory. For static containers, the cache is implemented
using template specializations, so the lookup is as instant as it can be, lets
just say it is much much faster than doing virtual function call.

As this slightly degrades performance for types with unique scope because on the
caller side, we do not know the scope type from T, the feature can be turned
on/off using traits.

#### Container Nesting

Containers can form a parent-child hierarchy and resolution will traverse the
container chain from child to last parent. Calling register_type() returns an
implicitly created child container for the type being registered, allowing a
per-type configuration to override a global configuration in the container.
Nesting is supported for both dynamic and static type maps based containers.

<!-- { include("examples/nesting.cpp", scope="////") -->

Example code included from [examples/nesting.cpp](examples/nesting.cpp):

```c++
struct A {
    int value;
};
struct B {
    int value;
};

container<> base_container;
base_container.register_type<scope<external>, storage<int>>(1);
base_container.register_type<scope<unique>, storage<A>>();
// Resolving A will use A{1} to construct A
assert(base_container.resolve<A>().value == 1);

base_container.register_type<scope<unique>, storage<B>>()
    .register_type<scope<external>, storage<int>>(
        2); // Override value of int for struct B
// Resolving B will use B{2} to construct B
assert(base_container.resolve<B>().value == 2);

// Create a nested container, overriding B (effectively removing override in
// base_container)
container<> nested_container(&base_container);
nested_container.register_type<scope<unique>, storage<B>>();
// Resolving B using nested container will use B{1} as provided by the
// parent container to construct B
assert(nested_container.resolve<B>().value == 1);
```

<!-- } -->

See [test/nesting.cpp](test/nesting.cpp) for details.

#### Customizable Allocation

To customize memory management, container constructor can take an optional user
supplied allocator that is used to allocate all memory required for container
operations. Note that resolved instances do not use this allocator as the
container does not have to own instances it constructs.

<!-- { include("examples/allocator.cpp", scope="////") -->

Example code included from [examples/allocator.cpp](examples/allocator.cpp):

```c++
// Define a container with user-provided allocator type
std::allocator<char> alloc;
container<dingo::dynamic_container_traits, std::allocator<char>>
    container(alloc);
```

<!-- } -->

#### Unit Tests

The functionality is covered with tests written using google test. See available
[test coverage](test).
