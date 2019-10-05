# Dingo
Dependency Injection Container for C++, with DI, next-gen and an 'o' in a name.

## Introduction
This is a prototype of header-only, dependency injection library. Developed with VS2019.

```c++
// Classes to be managed by the container
struct A { A() {} };
struct B { B(A&, std::shared_ptr< A >) {} };
struct C { C(B*, std::unique_ptr< B >&, A&) {} };

// Registration into container with different storages / lifetime policies
Container container;
container.RegisterBinding< Storage< Shared, std::shared_ptr< A > > >();
container.RegisterBinding< Storage< Shared, std::unique_ptr< B > > >();
container.RegisterBinding< Storage< Unique, C > >();

// Resolving the type C through container will instantiate A, B, and C.
auto c = container.Resolve< C >();

// Preliminary multibinding support
struct ICommand { virtual ~ICommand() {} };
struct Command1: I {};
struct Command2: I {};
struct CommandProcessor { CommandProcessor(std::vector< ICommand* >) {} };

container.RegisterBinding< Storage< Shared, Command1 >, Command1, ICommand >();
container.RegisterBinding< Storage< Shared, Command2 >, Command2, ICommand >();
container.RegisterBinding< Storage< Shared, CommandProcessor > >();
container.Resolve< std::list< ICommand* > >();
container.Resolve< CommandProcessor >();
```

### Goals
The following goals are based on what seemed either important or interesting in my day job. 
It is perfectly possible someone else will find something else more important, or more interesting.

#### Non-intrusive
Constructor signature of registered types is detected automatically.

#### Run-time Based
Container can be used from multiple modules. This is usually a requirement for larger projects with not so clean dependencies. The drawback is that errors are propagated in runtime, but in practice this can be mitigated by having unit test that is checking if types in the container will correctly resolve.

#### Natural Behavior
Natural means that the container tries to preserve usual type semantics of types it manages, without imposing requirements on managed types. It tries to be as opaque as possible. 
1) Internal instance storage is customisable, the container does not require some concrete storage.
2) Internal instance creation is further customized based on type being created.
Type held as std::shared_ptr is constructed using std::make_shared.
3) Internal instance can be converted to based on conversion rules specified during binding registration. Default conversions are quite permissive, allowing all conversions that can be done manually.

Commit / rollback exception guarantee for Resolve is implemented, meaning that after failed Resolve, container data is unchanged.
Thread-safety guarantees are the same as for STL containers - safe readers, unsafe writers. The catch is that one cannot tell if Resolve will be reading or writing.

#### Flexible
It should be possible to implement various enhancements without touching core code too much. Template class specializations are used to tweak the behavior and functionality for different storage types, instance creation or passing instances through type-erasure boundary. I've a lot of ideas, so I am going to test the flexibility soon.

#### Optimal
Unnecessary copyies of managed types are avoided. 
Memory allocations are used in RegisterBinding calls only, Resolve run without allocating memory.

#### Usable as Key/Value Container
It is possible to register a binding to the type under a key that is a base class of the type. As the correct cast is calculated at the time of registration, multiple inheritance is correctly supported. One instance can be resolved through multiple interfaces.

#### Multibindings Support
It is possible to resolve multiple implementations of an interface.

#### Cycle Support
With SharedCyclical storage, cycles between types are supported. This is implemented using two-phase construction.
Virtual Memory protection is used to guard access to not yet fully constructed dependencies and
exception is thrown if such access is detected.

#### Unit Tests
Right now there is something that is more of a development help than a proper unit test, but at least it works. See src/dingo.cpp.

