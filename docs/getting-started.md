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

Dingo exposes three container shapes:

- `container<>`: runtime-only registration with `register_type<...>()`
- `static_container<bindings<...>>`: compile-time registration only, with no
  runtime registration API
- `container<bindings<...>>`: compile-time registrations plus optional runtime
  registrations in the same container

Dingo also applies supported ownership and interface conversions automatically:
a registration can satisfy references, pointers, smart pointers, and interface
views when the selected storage and scope allow it.

## Runtime Registration

`register_type(...)` wires a type into the container. The registration is built
from policies such as:

- scope: how long the stored instance lives
- storage: what form of the instance is stored
- interfaces: which types can be used to resolve it
- factory: how the instance is created

Resolution recursively builds the constructor arguments required by registered
types, then applies the requested conversion to the resolved result.

<!-- { include("../examples/registration/runtime_registration.cpp", scope="////", summary="Runtime registration example") -->

<details>
<summary>Runtime registration example</summary>

Example code included from
[../examples/registration/runtime_registration.cpp](../examples/registration/runtime_registration.cpp):

```c++
class config {
  public:
    int retries() const { return retries_; }

  private:
    int retries_ = 3;
};

struct service_interface {
    virtual ~service_interface() {}
    virtual int retries() const = 0;
};

struct service : service_interface {
    explicit service(config& cfg) : cfg_(cfg) {}

    int retries() const override { return cfg_.retries(); }

  private:
    config& cfg_;
};

void runtime_registration_example() {
    using namespace dingo;
    container<> container;

    container.register_type<scope<shared>, storage<config>>();
    container.register_type<scope<shared>, storage<std::shared_ptr<service>>,
                            interfaces<service_interface>>();

    assert(container.resolve<service_interface&>().retries() == 3);
    assert(container.resolve<service_interface*>()->retries() == 3);
    assert(
        container.resolve<std::shared_ptr<service_interface>&>()->retries() ==
        3);
}
```

</details>
<!-- } -->

## Compile-Time Registration

`bindings<...>` wires the same policies into the container type. Each
`bind<...>` entry describes one registration, including explicit constructors or
`dependencies<...>` when dependency discovery should be part of the static
graph.

<!-- { include("../examples/registration/compile_time_registration.cpp", scope="////", summary="Compile-time registration example") -->

<details>
<summary>Compile-time registration example</summary>

Example code included from
[../examples/registration/compile_time_registration.cpp](../examples/registration/compile_time_registration.cpp):

```c++
class config {
  public:
    int retries() const { return retries_; }

  private:
    int retries_ = 3;
};

struct service_interface {
    virtual ~service_interface() {}
    virtual int retries() const = 0;
};

struct service : service_interface {
    explicit service(config& cfg) : cfg_(cfg) {}

    int retries() const override { return cfg_.retries(); }

  private:
    config& cfg_;
};

void compile_time_registration_example() {
    using namespace dingo;
    using app_bindings = dingo::bindings<
        dingo::bind<scope<shared>, storage<config>>,
        dingo::bind<scope<shared>, storage<std::shared_ptr<service>>,
                    interfaces<service_interface>>>;

    container<app_bindings> container;

    assert(container.resolve<service_interface&>().retries() == 3);
    assert(container.resolve<service_interface*>()->retries() == 3);
    assert(
        container.resolve<std::shared_ptr<service_interface>&>()->retries() ==
        3);

    static_container<app_bindings> static_only;
    assert(static_only.resolve<service_interface&>().retries() == 3);
}
```

</details>
<!-- } -->

Examples:

- [examples/container/quick.cpp](../examples/container/quick.cpp)
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

<!-- { include("../examples/container/construct.cpp", scope="////", summary="Construct an unmanaged type") -->

<details>
<summary>Construct an unmanaged type</summary>

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
/*B b =*/container.construct<B>();
```

</details>
<!-- } -->

<!-- { include("../examples/container/invoke.cpp", scope="////", summary="Invoke a callable with injected arguments") -->

<details>
<summary>Invoke a callable with injected arguments</summary>

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
/*B b1 =*/container.invoke([&](A& a) { return B{a}; });
/*B b2 =*/container.invoke(std::function<B(A&)>([](auto& a) { return B{a}; }));
/*B b3 =*/container.invoke(B::factory);
// Use an explicit signature to disambiguate overloaded call operators.
/*B b4 =*/container.invoke<B(A&)>(overloaded_factory{});
```

</details>
<!-- } -->

Examples:

- [examples/container/construct.cpp](../examples/container/construct.cpp)
- [examples/container/invoke.cpp](../examples/container/invoke.cpp)

## Further Reading

- [Core Concepts](core-concepts.md) for understanding how registration policies
  interact.
- [Advanced Topics](advanced-topics.md) for indexed lookup, nesting, or custom
  traits.
- [Examples](examples.md) for learning from runnable programs.
