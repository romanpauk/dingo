# Getting Started

This page covers the basic Dingo API: CMake integration, container types,
registration modes, resolution, and the `construct()` / `invoke()` APIs for
unmanaged code.

## CMake Integration

CMake projects can integrate Dingo with `FetchContent` and link against
`dingo::dingo`.

References:

- [README install section](../README.md#installation) with the short install
  snippet
- [test/fetchcontent/CMakeLists.txt](../test/fetchcontent/CMakeLists.txt) as a
  complete minimal project

## Container Types And Registration Modes

Dingo has two registration modes over the same scope, storage, interface, and
factory policy model.

- Runtime registration uses `container<>` and calls `register_type<...>()`. It
  fits registrations assembled from ordinary control flow, split across modules,
  or selected by runtime configuration.
- Compile-time registration uses `bindings<...>` and instantiates
  `container<bindings<...>>` or `static_container<bindings<...>>`. It fits
  graphs known in the type system and provides earlier diagnostics for missing
  dependencies or cycles.

Both modes use `resolve<T>()`, `construct<T>()`, and `invoke(...)` in the same
way after the container is configured.

Dingo exposes focused runtime/static containers plus a convenience `container`
facade:

- `runtime_container<>`: runtime-only registration with `register_type<...>()`
- `static_container<bindings<...>>`: compile-time registration only, with no
  runtime registration API
- `container<>`: the common runtime-only spelling, backed by the runtime
  container implementation
- `container<bindings<...>>`: the mixed form, with compile-time registrations
  plus optional runtime registrations in the same container

Dingo also applies supported ownership and interface
[conversions](architecture/conversion-model.md) automatically: a registration
can satisfy references, pointers, smart pointers, and interface views when the
selected storage and scope allow it.

## Runtime Registration

`register_type(...)` wires a type into the container. The registration is built
from policies such as:

- [scope](core-concepts.md#scopes): how long the stored instance lives
- [storage](core-concepts.md#storage-forms): what form of the instance is stored
- [interfaces](core-concepts.md#interfaces-and-service-locator-style-resolution):
  which types can be used to resolve it
- [factory](core-concepts.md#factories): how the instance is created

Resolution recursively builds the constructor arguments required by registered
types, then applies the requested conversion to the resolved result.

<!-- { include("../examples/container/quick_runtime.cpp", scope="////") -->

Example code included from
[../examples/container/quick_runtime.cpp](../examples/container/quick_runtime.cpp):

```c++
// User types do not need Dingo-specific base classes or macros.
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
// A and B are shared, so every resolution reuses the same instances.
container.register_type<scope<shared>, storage<std::shared_ptr<A>>>();
container.register_type<scope<shared>, storage<std::unique_ptr<B>>>();

// C is unique, so every resolve<C>() creates a fresh C.
container.register_type<scope<unique>, storage<C>>();

// Constructor arguments are resolved recursively, including ownership
// conversions from the registered storage forms.
C c = container.resolve<C>();

struct D {
    A& a;
    B* b;
};

// Construct an unmanaged object using dependencies from the container.
D d = container.construct<D>();

// Or invoke a callable with resolved arguments.
D e = container.invoke([&](A& a, B* b) { return D{a, b}; });
```

<!-- } -->

## Compile-Time Registration

`bindings<...>` wires the same policies into the container type. Each
`bind<...>` entry describes one registration, including explicit constructors or
`dependencies<...>` when dependency discovery should be part of the static
graph.

<!-- { include("../examples/container/quick_static.cpp", scope="////") -->

Example code included from
[../examples/container/quick_static.cpp](../examples/container/quick_static.cpp):

```c++
struct A {
    A() {}
};
struct B {
    B(A&, std::shared_ptr<A>) {}
};
struct C {
    C(B*, std::unique_ptr<B>&, A&) {}
};

// Compile-time bindings declare the same storage and scope as runtime
// registration.
using app_bindings =
    bindings<dingo::bind<scope<shared>, storage<std::shared_ptr<A>>>,
             dingo::bind<scope<shared>, storage<std::unique_ptr<B>>>,
             dingo::bind<scope<unique>, storage<C>>>;

container<app_bindings> container;
// Resolution still uses the same API.
C c = container.resolve<C>();

struct D {
    A& a;
    B* b;
};

// construct() and invoke() work with compile-time registration, too.
D d = container.construct<D>();
D e = container.invoke([&](A& a, B* b) { return D{a, b}; });
```

<!-- } -->

Examples:

- [examples/registration/non_intrusive.cpp](../examples/registration/non_intrusive.cpp)
- [examples/registration/runtime_registration.cpp](../examples/registration/runtime_registration.cpp)
- [examples/registration/compile_time_registration.cpp](../examples/registration/compile_time_registration.cpp)

## Scopes

The scope determines instance lifetime:

- `scope<external>`: use an existing instance
- `scope<unique>`: create a new instance for each resolution
- `scope<shared>`: cache one instance and reuse it
- `scope<shared_cyclical>`: allow cyclic graphs with two-phase construction

Most graphs use `unique` or `shared`. `external` represents ownership held
outside the container. `shared_cyclical` is reserved for cyclic construction and
adds the constraints required by that model.

References:

- [Core Concepts: Scopes and Storage](core-concepts.md#scopes-and-storage)
- [examples/storage/scope_external.cpp](../examples/storage/scope_external.cpp)
- [examples/storage/scope_unique.cpp](../examples/storage/scope_unique.cpp)
- [examples/storage/scope_shared.cpp](../examples/storage/scope_shared.cpp)
- [examples/storage/scope_shared_cyclical.cpp](../examples/storage/scope_shared_cyclical.cpp)

## Construct Unmanaged Types

Not every type needs to be registered.

- `construct<T>()` creates an unmanaged object and injects its dependencies from
  the container.
- `invoke(fn)` calls an unambiguous callable and resolves its arguments from the
  container.
- `invoke<Signature>(fn)` selects a specific overload when deduction is
  ambiguous.

These two APIs are useful at application boundaries where dependency injection
is needed without turning every transient object into container state.

<!-- { include("../examples/container/construct.cpp", scope="////") -->

Example code included from
[../examples/container/construct.cpp](../examples/container/construct.cpp):

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

<!-- { include("../examples/container/invoke.cpp", scope="////") -->

Example code included from
[../examples/container/invoke.cpp](../examples/container/invoke.cpp):

```c++
// struct B that will be constructed using container
struct B {
    A& a;
    static B factory(A& a) { return B{a}; }
};

struct overloaded_factory {
    B operator()(A& a) const { return B{a}; }
    int operator()(int value) const { return value * 2; }
};
// Construct instance of B, injecting shared instance of A
B b1 = container.invoke([&](A& a) { return B{a}; });
B b2 = container.invoke(std::function<B(A&)>([](auto& a) { return B{a}; }));
B b3 = container.invoke(B::factory);
// Use an explicit signature to disambiguate overloaded call operators.
B b4 = container.invoke<B(A&)>(overloaded_factory{});
```

<!-- } -->

## Further Reading

- [Core Concepts](core-concepts.md) for understanding how registration policies
  interact.
- [Advanced Topics](advanced-topics.md) for indexed lookup, nesting, or custom
  traits.
- [Examples](examples.md) for learning from runnable programs.
