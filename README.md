# Dingo
Dependency Injection Container for C++, with DI, next-gen and an 'o' in a name.

## Introduction
This is a prototype of header-only, dependency injection library I am working on. Developed with VS2019.

```c++
// Classes to be managed by the container
struct A { A() {} };
struct B { B(A&, std::shared_ptr< A >) {} }
struct C { C(B*, std::unique_ptr< B >&, A&) {} }

// Registration into container with different storages / lifetime policies
Container container;
container.RegisterBinding< Storage< Shared, std::shared_ptr< A > > >();
container.RegisterBinding< Storage< Shared, std::unique_ptr< B > > >();
container.RegisterBinding< Storage< Unique, C > >();

// Resolving the type C through container will instantiate A, B, and C.
auto c = container.Resolve< C >();
```

### Goals
The following goals are roughly based on what seemed either important or interesting in my job. It is perfectly possible someone else will find something else more important, or more interesting.

#### Non-intrusive
Constructor signature of registered types is detected automatically.

#### Run-time Based
Container can be used from multiple modules. This is usually a requirement for larger projects with not so clean dependencies. The drawback is that errors are propagated in runtime, but in practice this can be mitigated by having unit test that is checking if types in the container will correctly resolve.

#### Natural
Natural means that the container tries to preserve usual type semantics of types it manages, without imposing requirements on managed types. It tries to be as opaque as possible.
1) Internal instance storage is customisable, the container does not require some concrete storage.
2) Internal instance creation is further customized based on type being created.
Type held as std::shared_ptr is constructed using std::make_shared.
3) Internal instance can be converted to based on conversion rules specified during binding registration. Default conversions are quite permissive, allowing all conversions that can be done manually.
 
#### Flexible
Flexible means that it should be possible to implement various enhancements without touching core code too much. Template class specializations are used to tweak the behavior and functionality for different storage types, instance creation or passing instances through type-erasure boundary. I've a lot of ideas, so I am going to test the flexibility soon.

#### Optimal
This comes from goal to be Natural, one would not expect types that can be moved to be copied, or unnecessary copies created. The container should have low footprint and behave correctly in case of exceptions thrown from client code. Heap allocations are done only during RegisterBinding calls.

#### Usable as Key/Value Container
It is possible to register a binding to the type under a key that is a base class of the type. As the correct cast is calculated at the time of registration, multiple inheritance is correctly supported. One instance can be resolved through multiple interfaces.

#### Unit Tested
Right now there is something that is more of a development help than a proper unit test, but at least it works. See src/dingo.cpp.

