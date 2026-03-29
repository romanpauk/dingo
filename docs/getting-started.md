# Getting Started

This guide covers the shortest path to a working container: add the library,
register a few types, resolve them, and then use `construct()` or `invoke()` for
unmanaged code.

## Add Dingo To Your Build

For CMake projects, use `FetchContent` and link against `dingo::dingo`.

See:

- [README install section](../README.md#installation) for the short install
  snippet
- [test/fetchcontent/CMakeLists.txt](../test/fetchcontent/CMakeLists.txt) for a
  complete minimal project

## Register And Resolve Types

`register_type(...)` wires a type into the container. The registration is built
from policies such as:

- scope: how long the stored instance lives
- storage: what form of the instance is stored
- interfaces: which types can be used to resolve it
- factory: how the instance is created

The default path is straightforward:

1. Register a type with a scope and storage policy.
2. Call `resolve<T>()`.
3. Let Dingo recursively build the required constructor arguments.

<!-- { include("../examples/quick.cpp", scope="////", summary="Quick end-to-end example") -->

<details>
<summary>Quick end-to-end example</summary>

Example code included from [../examples/quick.cpp](../examples/quick.cpp):

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
/*C c =*/container.resolve<C>();

struct D {
    A& a;
    B* b;
};

// Construct an un-managed struct using dependencies from the container
/*D d =*/container.construct<D>();

// Invoke callable
/*D e =*/container.invoke([&](A& a, B* b) { return D{a, b}; });
```

</details>
<!-- } -->

See:

- [examples/quick.cpp](../examples/quick.cpp)
- [examples/non_intrusive.cpp](../examples/non_intrusive.cpp)

## Choose A Scope Deliberately

The scope determines instance lifetime:

- `scope<external>`: use an existing instance
- `scope<unique>`: create a new instance for each resolution
- `scope<shared>`: cache one instance and reuse it
- `scope<shared_cyclical>`: allow cyclic graphs with two-phase construction

Start with `unique` or `shared`. Use `external` when ownership lives elsewhere.
Use `shared_cyclical` only when you actually need cyclic construction and can
accept its extra constraints.

See:

- [Core Concepts: Scopes and Storage](core-concepts.md#scopes-and-storage)
- [examples/scope_external.cpp](../examples/scope_external.cpp)
- [examples/scope_unique.cpp](../examples/scope_unique.cpp)
- [examples/scope_shared.cpp](../examples/scope_shared.cpp)
- [examples/scope_shared_cyclical.cpp](../examples/scope_shared_cyclical.cpp)

## Construct Unmanaged Types

Not every type needs to be registered.

- `construct<T>()` creates an unmanaged object and injects its dependencies from
  the container.
- `invoke(fn)` calls a callable and resolves its arguments from the container.

These two APIs are useful at application boundaries where you want dependency
injection without turning every transient object into container state.

<!-- { include("../examples/construct.cpp", scope="////", summary="Construct an unmanaged type") -->

<details>
<summary>Construct an unmanaged type</summary>

Example code included from
[../examples/construct.cpp](../examples/construct.cpp):

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
/*B b =*/container.construct<B>();
```

</details>
<!-- } -->

<!-- { include("../examples/invoke.cpp", scope="////", summary="Invoke a callable with injected arguments") -->

<details>
<summary>Invoke a callable with injected arguments</summary>

Example code included from [../examples/invoke.cpp](../examples/invoke.cpp):

```c++
// struct B that will be constructed using container
struct B {
    A& a;
    static B factory(A& a) { return B{a}; }
};
// Construct instance of B, injecting shared instance of A
/*B b1 =*/container.invoke([&](A& a) { return B{a}; });
/*B b2 =*/container.invoke(std::function<B(A&)>([](auto& a) { return B{a}; }));
/*B b3 =*/container.invoke(B::factory);
```

</details>
<!-- } -->

See:

- [examples/construct.cpp](../examples/construct.cpp)
- [examples/invoke.cpp](../examples/invoke.cpp)

## What To Read Next

- [Core Concepts](core-concepts.md) if you want to understand how registration
  policies interact.
- [Advanced Topics](advanced-topics.md) if you need indexed lookup, nesting, or
  custom traits.
- [Examples](examples.md) if you prefer learning from runnable programs.
