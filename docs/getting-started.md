# Getting Started

This page describes the basic Dingo API: CMake integration, container types,
registration modes, resolution, and `construct()` and `invoke()` for unmanaged
code.

## CMake Integration

CMake projects integrate Dingo with `FetchContent` and link to `dingo::dingo`.

References:

- [README install section](../README.md#installation) with the short install
  snippet
- [test/fetchcontent/CMakeLists.txt](../test/fetchcontent/CMakeLists.txt) as a
  complete minimal project

## Container Types And Registration Modes

Dingo has two registration modes. Both modes use the same scope, storage,
interface and factory policies.

| Container                         | Registration                                                                  | Use it when                                                                                              |
| --------------------------------- | ----------------------------------------------------------------------------- | -------------------------------------------------------------------------------------------------------- |
| `container<>`                     | Runtime, with `register_type<...>()`. `runtime_container<>` is the same type. | Ordinary control flow, several modules or runtime configuration select the registrations.                |
| `static_container<bindings<...>>` | Compile time only. It has no runtime registration API.                        | The graph is known in the type system. The compiler reports missing dependencies and unsupported cycles. |

Both modes use `resolve<T>()`, `construct<T>()` and `invoke(...)` in the same
way. The two modes do not mix. A container holds either runtime registrations or
compile-time bindings.

Dingo applies the supported ownership and interface
[conversions](architecture/conversion-model.md). One registration can satisfy
references, pointers, smart pointers and interface lookups, if the selected
storage and scope allow it.

## Runtime Registration

`register_type(...)` adds a type to the container. These policies make up the
registration:

- [scope](core-concepts.md#scopes): how long the stored instance lives
- [storage](core-concepts.md#storage-forms): the form in which the container
  stores the instance
- [interfaces](core-concepts.md#interfaces-and-service-locator-style-resolution):
  the types that resolve the instance
- [factory](core-concepts.md#factories): how the container creates the instance

Resolution builds the constructor arguments of registered types recursively.
Then it applies the requested conversion to the result.

<!-- { include("../examples/container/quick_runtime.cpp", scope="////") -->

Example code included from
[../examples/container/quick_runtime.cpp](../examples/container/quick_runtime.cpp):

```c++
// User types do not need Dingo-specific base classes or macros.
struct A {
  A() = default;
};
struct B {
  B(A &, std::shared_ptr<A>) {}
};
struct C {
  C(B *, std::unique_ptr<B> &, A &) {}
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
  A &a;
  B *b;
};

// Construct an unmanaged object using dependencies from the container.
D d = container.construct<D>();

// Or invoke a callable with resolved arguments.
D e = container.invoke([&](A &a, B *b) { return D{a, b}; });
```

<!-- } -->

## Compile-Time Registration

`bindings<...>` puts the same policies into the container type. Each `bind<...>`
entry describes one registration. An entry can name an explicit constructor. It
can also list `dependencies<...>` to make them part of the static graph.

<!-- { include("../examples/container/quick_static.cpp", scope="////") -->

Example code included from
[../examples/container/quick_static.cpp](../examples/container/quick_static.cpp):

```c++
struct A {
  A() = default;
};
struct B {
  B(A &, std::shared_ptr<A>) {}
};
struct C {
  C(B *, std::unique_ptr<B> &, A &) {}
};

// Compile-time bindings declare the same storage and scope as runtime
// registration.
using app_bindings =
    bindings<dingo::bind<scope<shared>, storage<std::shared_ptr<A>>>,
             dingo::bind<scope<shared>, storage<std::unique_ptr<B>>>,
             dingo::bind<scope<unique>, storage<C>>>;

static_container<app_bindings> container;
// Resolution still uses the same API.
C c = container.resolve<C>();

struct D {
  A &a;
  B *b;
};

// construct() and invoke() work with compile-time registration, too.
D d = container.construct<D>();
D e = container.invoke([&](A &a, B *b) { return D{a, b}; });
```

<!-- } -->

Examples:

- [examples/registration/non_intrusive.cpp](../examples/registration/non_intrusive.cpp)
- [examples/registration/runtime_registration.cpp](../examples/registration/runtime_registration.cpp)
- [examples/registration/compile_time_registration.cpp](../examples/registration/compile_time_registration.cpp)

## Scopes

The scope sets the lifetime of the instance:

- `scope<external>`: use an existing instance
- `scope<unique>`: create a new instance for each resolution
- `scope<shared>`: cache one instance and reuse it
- `scope<shared_cyclical>`: allow cyclic graphs with two-phase construction

Most graphs use `unique` or `shared`. With `external`, code outside the
container owns the instance. Use `shared_cyclical` only for cyclic construction.
It adds the constraints of that model.

References:

- [Core Concepts: Scopes and Storage](core-concepts.md#scopes-and-storage)
- [examples/storage/scope_external.cpp](../examples/storage/scope_external.cpp)
- [examples/storage/scope_unique.cpp](../examples/storage/scope_unique.cpp)
- [examples/storage/scope_shared.cpp](../examples/storage/scope_shared.cpp)
- [examples/storage/scope_shared_cyclical.cpp](../examples/storage/scope_shared_cyclical.cpp)

## Construct Unmanaged Types

Not every type needs to be registered.

- `construct<T>()` creates an unmanaged object. The container supplies its
  dependencies.
- `construct<T &>()`, `construct<T &&>()` and a request with a key or an
  annotation name an instance or a binding, not a value to make. These requests
  resolve as `resolve<T>()` does.
- `invoke(fn)` calls an unambiguous callable. The container resolves its
  arguments.
- `invoke<Signature>(fn)` selects one overload when deduction is ambiguous.

Use these APIs at application boundaries. They inject dependencies, but the
container does not keep the objects that they make.

<!-- { include("../examples/container/construct.cpp", scope="////") -->

Example code included from
[../examples/container/construct.cpp](../examples/container/construct.cpp):

```c++
// struct A that will be registered with the container
struct A {};

// struct B that will be constructed using container
struct B {
  A &a;
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
  A &a;
  static B factory(A &a) { return B{a}; }
};

struct overloaded_factory {
  B operator()(A &a) const { return B{a}; }
  int operator()(int value) const { return value * 2; }
};
// Construct instance of B, injecting shared instance of A
B b1 = container.invoke([&](A &a) { return B{a}; });
B b2 = container.invoke(std::function<B(A &)>([](auto &a) { return B{a}; }));
B b3 = container.invoke(B::factory);
// Use an explicit signature to disambiguate overloaded call operators.
B b4 = container.invoke<B(A &)>(overloaded_factory{});
```

<!-- } -->

## Further Reading

- [Core Concepts](core-concepts.md): how the registration policies interact.
- [Advanced Topics](advanced-topics.md): lookups, nesting and custom traits.
- [Examples](examples.md): runnable programs.
