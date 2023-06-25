# Dingo
Dependency Injection Container for C++, with DI, next-gen and an 'o' in a name.

## Introduction
Dingo is a header-only, dependency injection library that preforms recursive instantiation of registered types. 
It has no dependencies except for the C++ standard library.

```c++
// Classes to be managed by the container. Note that there is not special code required for the type to become managed.
struct A { A() {} };
struct B { B(A&, std::shared_ptr< A >) {} };
struct C { C(B*, std::unique_ptr< B >&, A&) {} };

// Registration into container with different lifetime policies
container<> container;
container.register_binding< storage< container<>, shared, std::shared_ptr< A > > >();
container.register_binding< storage< container<>, shared, std::unique_ptr< B > > >();
container.register_binding< storage< container<>, unique, C > >();

// Resolving the type C through container will instantiate A, B, and C.
auto c = container.resolve< C >();

// Multibinding support
struct ICommand { virtual ~ICommand() {} };
struct Command1: I {};
struct Command2: I {};
struct CommandProcessor { CommandProcessor(std::vector< ICommand* >) {} };

container.register_binding< storage< container<>, shared, Command1 >, Command1, ICommand >();
container.register_binding< storage< container<>, shared, Command2 >, Command2, ICommand >();
container.register_binding< storage< container<>, shared, CommandProcessor > >();
container.resolve< std::list< ICommand* > >();
container.resolve< CommandProcessor >();
``` 

### Features

#### Non-intrusive class registration
Constructor signatures of registered types are detected automatically from constructor signature. 

#### Runtime-based usage
Container can be used from multiple modules, specifically it can be filled in one module and used in other modules. This is a requirement for larger projects with not so clean dependencies. Compared to the compile-time DI solutions, errors are propagated in runtime as exceptions.

#### Naturally-behaving
The container tries to preserve usual type semantics of types it manages, without imposing requirements on managed types. It tries to be as transparent as possible to the managed type. Registered type can be automatically converted to types that are 'reachable' from the base type using usual conversions.

#### Exception-safe
When exception occurs in user-provided code, the state of the container is not changed.

#### Thread-safe
The thread-safety guarante is the same as for STL containers: thread-safe for reading and not thread-safe for writing. The catch is that one cannot tell if resolving operation will be reading or writing.

#### Flexible
The code is decomposed so it is possible to implement various enhancements without touching core code too much. Template class specializations are used to tweak the behavior and functionality for different storage types, instance creation or passing instances through type-erasure boundary. 

#### Optimal
Unnecessary copies of managed types are avoided if possible, the type is treated as it is natural for its storage type.

#### Usable as Service Locator
It is possible to register a binding to the type under a key that is a base class of the type. As the upcast is compiled at the time of a registration, multiple inheritance is correctly supported. One instance can be resolved through multiple interfaces.

#### Multibindings Support
It is possible to resolve multiple implementations of an interface as a collection. See [multibindings.cpp](src/tests/multibindings.cpp).

#### Annotated Type Support
It is possible to register different implementations with the same interface, disambiguating the registration with an user-provided tag. See [annotated.cpp](src/tests/annotated.cpp).

#### Support for Cycles
With shared_cyclical storage, cycles between types are supported. This is implemented using two-phase construction.
More an experiment than something practical, virtual memory protection can be used to throw upon access of not-yet-constructed class. See [cyclical.cpp](src/tests/cyclical.cpp).

#### Unit Tests
The functionality is covered with tests written using google test. See available [test coverage](src/tests).


