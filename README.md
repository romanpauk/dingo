# Dingo
Dependency Injection Container for C++, with DI, next-gen and an 'o' in a name.

## Introduction
This is a prototype of header-only, dependency injection library. 
It has no dependencies except for the C++ standard library.

```c++
// Classes to be managed by the container
struct A { A() {} };
struct B { B(A&, std::shared_ptr< A >) {} };
struct C { C(B*, std::unique_ptr< B >&, A&) {} };

// Registration into container with different storages (lifetime policies)
container<> container;
container.register_binding< storage< shared, std::shared_ptr< A > > >();
container.register_binding< storage< shared, std::unique_ptr< B > > >();
container.register_binding< storage< unique, C > >();

// Resolving the type C through container will instantiate A, B, and C.
auto c = container.resolve< C >();

// Multibinding support
struct ICommand { virtual ~ICommand() {} };
struct Command1: I {};
struct Command2: I {};
struct CommandProcessor { CommandProcessor(std::vector< ICommand* >) {} };

container.register_binding< storage< shared, Command1 >, Command1, ICommand >();
container.register_binding< storage< shared, Command2 >, Command2, ICommand >();
container.register_binding< storage< shared, CommandProcessor > >();
container.resolve< std::list< ICommand* > >();
container.resolve< CommandProcessor >();
``` 

### Features
#### Non-intrusive
Constructor signature of registered types is detected automatically.

#### Run-time Based
Container can be used from multiple modules. This is a requirement for larger projects with not so clean dependencies. The drawback is that errors are propagated in runtime, but in practice this can be mitigated by having unit test that is checking if types in the container will correctly resolve.

#### Behaving Naturally
Natural means that the container tries to preserve usual type semantics of types it manages, without imposing requirements on managed types. It tries to be as opaque as possible. 
1) Internal instance storage is customisable, the container does not require some concrete storage.
2) Internal instance creation is further customized based on type being created.
Type held as std::shared_ptr is constructed using std::make_shared.
3) Internal instance can be converted to based on conversion rules specified during binding registration. Default conversions are quite permissive, allowing all conversions that can be done manually.

Thread-safety guarantees are the same as for STL containers - safe for reading, unsafe for writing. The catch is that one cannot tell if resolve will be reading or writing.

#### Flexible
It should be possible to implement various enhancements without touching core code too much. Template class specializations are used to tweak the behavior and functionality for different storage types, instance creation or passing instances through type-erasure boundary. 

#### Optimal
Unnecessary copyies of managed types are avoided if possible.

#### Usable as Service Locator
It is possible to register a binding to the type under a key that is a base class of the type. As the correct cast is calculated at the time of registration, multiple inheritance is correctly supported. One instance can be resolved through multiple interfaces.

#### Multibindings Support
It is possible to resolve multiple implementations of an interface as a collection.

#### Annotations Support
It is possible to register different implementations with the same interface, disambiguating them with a user-provided tag.

#### Support for Cycles
With shared_cyclical storage, cycles between types are supported. This is implemented using two-phase construction.
More an experiment than something practical, virtual memory protection can be used to throw upon access of not-yet-constructed class.

#### Unit Tests
The functionality is covered with tests written using Boost Test.


