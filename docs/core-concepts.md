# Core Concepts

This document groups the concepts that define how Dingo behaves at runtime:
registration, scopes, storage, factories, and interface-oriented resolution.

## Registration Model

Dingo does not require managed types to inherit from a framework base class or
to expose special metadata. You describe how a type should be handled at the
registration site.

Common registration policies:

- `scope<...>` selects lifetime and caching behavior
- `storage<...>` selects the stored representation
- `interface<...>` exposes the registration through another type
- `factory<...>` overrides construction when constructor deduction is not enough

Some policies are deduced from others, so many registrations stay short in
practice.

<!-- { include("../examples/non_intrusive.cpp", scope="////", summary="Registration policies example") -->

<details>
<summary>Registration policies example</summary>

Example code included from
[../examples/non_intrusive.cpp](../examples/non_intrusive.cpp):

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

</details>
<!-- } -->

See:

- [examples/non_intrusive.cpp](../examples/non_intrusive.cpp)
- [include/dingo/type_registration.h](../include/dingo/type_registration.h)

## Scopes And Storage

Scope and storage work together. Scope controls lifetime; storage controls the
representation that Dingo keeps and converts during resolution.

### Scopes

- `external`: refer to an already existing object
- `unique`: build a fresh instance per resolution
- `shared`: cache and reuse the same instance
- `shared_cyclical`: cache instances while permitting cyclic graphs

### Storage Forms

The stored type can be a value, raw pointer, `std::unique_ptr`,
`std::shared_ptr`, or another supported form. Dingo respects the usual semantics
of that form when creating or passing instances around.

This matters because conversions during resolution depend on both:

- the chosen scope
- the chosen stored type

Examples:

- a `shared` registration stored as `std::shared_ptr<T>` can naturally satisfy
  `T&`, `T*`, and `std::shared_ptr<T>` style dependencies
- an `external` registration can expose an already-owned object without moving
  ownership into the container

<!-- { include("../examples/scope_external.cpp", scope="////", summary="External scope example") -->

<details>
<summary>External scope example</summary>

Example code included from
[../examples/scope_external.cpp](../examples/scope_external.cpp):

```c++
struct A {
} instance;

container<> container;
// Register existing instance of A, stored as a pointer.
container.register_type<scope<external>, storage<A*>>(&instance);
// Resolution will return an existing instance of A casted to required type.
assert(&container.resolve<A&>() == container.resolve<A*>());
```

</details>
<!-- } -->

<!-- { include("../examples/scope_unique.cpp", scope="////", summary="Unique scope example") -->

<details>
<summary>Unique scope example</summary>

Example code included from
[../examples/scope_unique.cpp](../examples/scope_unique.cpp):

```c++
struct A {};
container<> container;
// Register struct A with unique scope
container.register_type<scope<unique>, storage<A>>();
// Resolution will get a unique instance of A
container.resolve<A>();
```

</details>
<!-- } -->

<!-- { include("../examples/scope_shared.cpp", scope="////", summary="Shared scope example") -->

<details>
<summary>Shared scope example</summary>

Example code included from
[../examples/scope_shared.cpp](../examples/scope_shared.cpp):

```c++
struct A {};
container<> container;
// Register struct A with shared scope
container.register_type<scope<shared>, storage<A>>();
// Resolution will always return the same A instance
assert(container.resolve<A*>() == &container.resolve<A&>());
```

</details>
<!-- } -->

<!-- { include("../examples/scope_shared_cyclical.cpp", scope="////", summary="Shared cyclical scope example") -->

<details>
<summary>Shared cyclical scope example</summary>

Example code included from
[../examples/scope_shared_cyclical.cpp](../examples/scope_shared_cyclical.cpp):

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

</details>
<!-- } -->

See:

- [examples/scope_external.cpp](../examples/scope_external.cpp)
- [examples/scope_unique.cpp](../examples/scope_unique.cpp)
- [examples/scope_shared.cpp](../examples/scope_shared.cpp)
- [examples/scope_shared_cyclical.cpp](../examples/scope_shared_cyclical.cpp)
- [include/dingo/storage/external.h](../include/dingo/storage/external.h)
- [include/dingo/storage/unique.h](../include/dingo/storage/unique.h)
- [include/dingo/storage/shared.h](../include/dingo/storage/shared.h)
- [include/dingo/storage/shared_cyclical.h](../include/dingo/storage/shared_cyclical.h)

## Factories

By default, Dingo tries to select a usable constructor automatically. That keeps
the common case concise, but you can override construction when needed.

Factory styles in the repo:

- automatic constructor deduction
- explicit constructor selection
- static function factory
- stateful callable factory

Use an explicit factory when:

- constructor deduction is ambiguous
- the type should be built through a named factory function
- construction depends on extra callable state

<!-- { include("../examples/factory_constructor.cpp", scope="////", summary="Automatically deduced constructor factory") -->

<details>
<summary>Automatically deduced constructor factory</summary>

Example code included from
[../examples/factory_constructor.cpp](../examples/factory_constructor.cpp):

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

</details>
<!-- } -->

<!-- { include("../examples/factory_constructor_concrete.cpp", scope="////", summary="Explicit constructor selection") -->

<details>
<summary>Explicit constructor selection</summary>

Example code included from
[../examples/factory_constructor_concrete.cpp](../examples/factory_constructor_concrete.cpp):

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

</details>
<!-- } -->

<!-- { include("../examples/factory_static_function.cpp", scope="////", summary="Static factory-function example") -->

<details>
<summary>Static factory-function example</summary>

Example code included from
[../examples/factory_static_function.cpp](../examples/factory_static_function.cpp):

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

</details>
<!-- } -->

<!-- { include("../examples/factory_callable.cpp", scope="////", summary="Stateful callable factory example") -->

<details>
<summary>Stateful callable factory example</summary>

Example code included from
[../examples/factory_callable.cpp](../examples/factory_callable.cpp):

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

</details>
<!-- } -->

See:

- [examples/factory_constructor.cpp](../examples/factory_constructor.cpp)
- [examples/factory_constructor_concrete.cpp](../examples/factory_constructor_concrete.cpp)
- [examples/factory_static_function.cpp](../examples/factory_static_function.cpp)
- [examples/factory_callable.cpp](../examples/factory_callable.cpp)
- [include/dingo/factory/constructor.h](../include/dingo/factory/constructor.h)
- [include/dingo/factory/function.h](../include/dingo/factory/function.h)
- [include/dingo/factory/callable.h](../include/dingo/factory/callable.h)

## Interfaces And Service-Locator Style Resolution

Registrations can be exposed through base classes or other interface types. The
upcast is fixed at registration time, which keeps multiple-inheritance cases
predictable without relying on `dynamic_cast` for ordinary resolution.

Use this when:

- concrete implementations should stay hidden behind an interface
- one concrete type should be resolvable through several interfaces

<!-- { include("../examples/service_locator.cpp", scope="////", summary="Resolve through an interface") -->

<details>
<summary>Resolve through an interface</summary>

Example code included from
[../examples/service_locator.cpp](../examples/service_locator.cpp):

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

</details>
<!-- } -->

See:

- [examples/service_locator.cpp](../examples/service_locator.cpp)

## Multibindings And Collections

Dingo can register multiple implementations under one interface and then resolve
them as a collection.

This is useful for plugin-style extension points such as:

- processors
- handlers
- strategy lists

The collection can use a standard aggregation rule or a custom one.

<!-- { include("../examples/multibindings.cpp", scope="////", summary="Multibindings example") -->

<details>
<summary>Multibindings example</summary>

Example code included from
[../examples/multibindings.cpp](../examples/multibindings.cpp):

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

</details>
<!-- } -->

<!-- { include("../examples/collection.cpp", scope="////", summary="Custom collection aggregation example") -->

<details>
<summary>Custom collection aggregation example</summary>

Example code included from
[../examples/collection.cpp](../examples/collection.cpp):

```c++
struct ProcessorBase {
    virtual ~ProcessorBase() {}
    virtual void process(const void*) = 0;
    virtual std::type_index type() = 0;
};

template <typename T, typename ProcessorT> struct Processor : ProcessorBase {
    virtual std::type_index type() override { return typeid(T); }

    void process(const void* transaction) override {
        static_cast<ProcessorT*>(this)->process(
            *reinterpret_cast<const T*>(transaction));
    }
};

struct StringProcessor : Processor<std::string, StringProcessor> {
    void process(const std::string& value) {};
};

struct VectorIntProcessor : Processor<std::vector<int>, VectorIntProcessor> {
    void process(const std::vector<int>& value) {};
};

struct Dispatcher {
    Dispatcher(
        std::map<std::type_index, std::unique_ptr<ProcessorBase>>&& processors)
        : processors_(std::move(processors)) {}

    template <typename T> void process(const T& value) {
        processors_.at(typeid(T))->process(&value);
    }

  private:
    std::map<std::type_index, std::unique_ptr<ProcessorBase>> processors_;
};

container<> container;
container
    .register_type<scope<unique>, storage<std::unique_ptr<StringProcessor>>,
                   interface<ProcessorBase>>();
container
    .register_type<scope<unique>, storage<std::unique_ptr<VectorIntProcessor>>,
                   interface<ProcessorBase>>();
container.register_type_collection<
    scope<unique>,
    storage<std::map<std::type_index, std::unique_ptr<ProcessorBase>>>>(
    [](auto& collection, auto&& value) {
        collection.emplace(value->type(), std::move(value));
    });

auto dispatcher = container.construct<Dispatcher>();
dispatcher.process(std::string(""));
```

</details>
<!-- } -->

See:

- [examples/multibindings.cpp](../examples/multibindings.cpp)
- [examples/collection.cpp](../examples/collection.cpp)
- [include/dingo/collection_traits.h](../include/dingo/collection_traits.h)

## Runtime Model And Error Handling

Dingo is runtime-based. That makes cross-module usage practical, but it also
means some wiring errors surface as exceptions during resolution rather than as
pure compile-time failures.

When user code throws during resolution, the container remains in a valid state.
Already completed shared resolutions may stay cached, because resolution is a
recursive process and some work may already have finished successfully.

See:

- [docs/advanced-topics.md](advanced-topics.md)
