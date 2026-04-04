# Core Concepts

This page covers the parts of Dingo that shape runtime behavior: registration,
lifetimes, stored forms, arrays, variants, factories, and interface-oriented
resolution.

If you want the internal model behind these features, start with the
[architecture docs](architecture/README.md).

## Registration Model

Dingo does not require managed types to inherit from a framework base class or
to expose special metadata. You describe how a type should be handled at the
registration site.

Common registration policies:

- `scope<...>` selects lifetime and caching behavior
- `storage<...>` selects the stored representation
- `interfaces<...>` exposes the registration through one or more interface types
- `factory<...>` overrides construction when constructor deduction is not enough

Many registrations stay short because Dingo can infer the missing policy from
the rest.

<!-- { include("../examples/registration/non_intrusive.cpp", scope="////", summary="Registration policies example") -->

<details>
<summary>Registration policies example</summary>

Example code included from
[../examples/registration/non_intrusive.cpp](../examples/registration/non_intrusive.cpp):

```c++
container<> container;
// Registration of a struct A
container.register_type<scope<unique>,           // using unique scope
                        factory<constructor<A>>, // using constructor-detecting
                                                 // factory
                        storage<std::unique_ptr<A>>, // stored as unique_ptr<A>
                        interfaces<A>                // resolvable as A
                        >();
// As some policies can be deduced from the others, the above
// registration simplified
container.register_type<scope<unique>, storage<std::unique_ptr<A>>>();
```

</details>
<!-- } -->

See:

- [examples/registration/non_intrusive.cpp](../examples/registration/non_intrusive.cpp)
- [include/dingo/registration/type_registration.h](../include/dingo/registration/type_registration.h)

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
`std::shared_ptr`, `std::optional`, raw arrays, or another supported form. Dingo
respects the usual semantics of that form when creating or passing instances
around.

What you can inject depends on both:

- the chosen scope

- the chosen stored type

- a `shared` registration stored as `std::shared_ptr<T>` can naturally satisfy
  `T&`, `T*`, and `std::shared_ptr<T>` style dependencies

- an `external` registration can expose an already-owned object without moving
  ownership into the container

- some exact wrapper forms intentionally resolve only as the wrapper itself,
  rather than as every possible contained alternative

<!-- { include("../examples/storage/scope_external.cpp", scope="////", summary="External scope example") -->

<details>
<summary>External scope example</summary>

Example code included from
[../examples/storage/scope_external.cpp](../examples/storage/scope_external.cpp):

```c++
struct A {};

A instance;
container<> container;
// Register existing instance of A, stored as a pointer.
container.register_type<scope<external>, storage<A*>>(&instance);
// Resolution will return an existing instance of A casted to required type.
assert(&container.resolve<A&>() == container.resolve<A*>());
```

</details>
<!-- } -->

<!-- { include("../examples/storage/scope_unique.cpp", scope="////", summary="Unique scope example") -->

<details>
<summary>Unique scope example</summary>

Example code included from
[../examples/storage/scope_unique.cpp](../examples/storage/scope_unique.cpp):

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

<!-- { include("../examples/storage/scope_shared.cpp", scope="////", summary="Shared scope example") -->

<details>
<summary>Shared scope example</summary>

Example code included from
[../examples/storage/scope_shared.cpp](../examples/storage/scope_shared.cpp):

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

<!-- { include("../examples/storage/scope_shared_cyclical.cpp", scope="////", summary="Shared cyclical scope example") -->

<details>
<summary>Shared cyclical scope example</summary>

Example code included from
[../examples/storage/scope_shared_cyclical.cpp](../examples/storage/scope_shared_cyclical.cpp):

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

- [examples/storage/scope_external.cpp](../examples/storage/scope_external.cpp)
- [examples/storage/scope_unique.cpp](../examples/storage/scope_unique.cpp)
- [examples/storage/scope_shared.cpp](../examples/storage/scope_shared.cpp)
- [examples/storage/scope_shared_cyclical.cpp](../examples/storage/scope_shared_cyclical.cpp)
- [include/dingo/storage/external.h](../include/dingo/storage/external.h)
- [include/dingo/storage/unique.h](../include/dingo/storage/unique.h)
- [include/dingo/storage/shared.h](../include/dingo/storage/shared.h)
- [include/dingo/storage/shared_cyclical.h](../include/dingo/storage/shared_cyclical.h)

## Arrays

Dingo supports raw C++ arrays, `std::unique_ptr<T[]>`, and
`std::shared_ptr<T[]>`, including N-D array shapes.

In practice:

- register the exact array shape you want to store
- resolve either the exact shape or the supported borrowed/owning array view for
  that scope
- keep shape in mind, because nested arrays do not flatten to `T*`

The main constraints are:

- `resolve<T[N]>()` is not supported because arrays are not returned by value
- unique array storage can hand out owning handles such as
  `std::unique_ptr<T[]>`
- shared array storage can hand out stable borrowed views and shared handles

<!-- { include("../examples/storage/array.cpp", scope="////", summary="Array storage and resolution example") -->

<details>
<summary>Array storage and resolution example</summary>

Example code included from
[../examples/storage/array.cpp](../examples/storage/array.cpp):

```c++
using namespace dingo;

struct cell {
    cell() = default;
};

struct row_consumer {
    cell (*rows)[3];

    explicit row_consumer(cell (*init)[3]) : rows(init) {}
};

container<> raw_container;
// Register a raw N-D array in shared scope.
raw_container.register_type<scope<shared>, storage<cell[2][3]>>();

// Resolve row view, exact pointer view and inject the row view.
/*auto* rows =*/raw_container.resolve<cell (*)[3]>();
/*auto& exact =*/raw_container.resolve<cell (&)[2][3]>();
/*row_consumer borrowed =*/
raw_container.construct<row_consumer, constructor<row_consumer(cell (*)[3])>>();

container<> unique_container;
// Register a fixed-size array in unique scope and resolve it as an owning
// dynamic array handle.
unique_container.register_type<scope<unique>, storage<cell[4]>>();
/*auto owned =*/unique_container.resolve<std::unique_ptr<cell[]>>();

container<> shared_container;
// Register a shared smart array directly.
shared_container.register_type<scope<shared>, storage<std::shared_ptr<cell[]>>>(
    callable([] { return std::shared_ptr<cell[]>(new cell[4]); }));
/*auto shared =*/shared_container.resolve<std::shared_ptr<cell[]>>();
```

</details>
<!-- } -->

See:

- [examples/storage/array.cpp](../examples/storage/array.cpp)
- [include/dingo/type/type_traits.h](../include/dingo/type/type_traits.h)

## Variants

Dingo handles variants in two distinct places:

- `construct<std::variant<A, B>, constructor_detection<A>>()` constructs the
  variant by selecting `A` as the alternative to build
- `register_type<..., storage<std::variant<A, B>>, factory<...>>()` lets the
  container store the variant and later resolve either the whole variant or its
  currently held alternative

The current rules are narrow on purpose:

- the selected alternative must appear exactly once in the variant type
- the whole variant remains resolvable
- uniquely occurring alternatives are also resolvable from variant storage
- if a requested alternative is published but the current instance holds a
  different alternative, resolution fails as `type_not_convertible_exception`
- duplicate alternative types are rejected at compile time for direct resolution
- unique variant storage resolves the whole variant as a value or rvalue, and a
  held alternative as a value or rvalue
- shared and external variant storage resolve the whole variant as values,
  references, or pointers, and a held alternative as values, references, or
  pointers

<!-- { include("../examples/container/variant.cpp", scope="////", summary="Variant construction and storage example") -->

<details>
<summary>Variant construction and storage example</summary>

Example code included from
[../examples/container/variant.cpp](../examples/container/variant.cpp):

```c++
struct A {
    explicit A(int init) : value(init) {}
    int value;
};

struct B {
    explicit B(float init) : value(init) {}
    float value;
};

container<> construct_container;
construct_container.register_type<scope<external>, storage<int>>(7);
construct_container.register_type<scope<external>, storage<float>>(3.5f);

// Construct a variant by selecting which alternative should be built.
[[maybe_unused]] auto detected =
    construct_container
        .construct<std::variant<A, B>, constructor_detection<A>>();
assert(std::holds_alternative<A>(detected));

[[maybe_unused]] auto explicit_ctor =
    construct_container.construct<std::variant<A, B>, constructor<B(float)>>();
assert(std::holds_alternative<B>(explicit_ctor));

container<> unique_container;
unique_container.register_type<scope<unique>, storage<int>>();
unique_container.register_type<scope<unique>, storage<std::variant<A, B>>,
                               factory<constructor_detection<A>>>();

// Resolve either the whole variant or its currently held alternative.
[[maybe_unused]] auto value = unique_container.resolve<std::variant<A, B>>();
assert(std::holds_alternative<A>(value));

[[maybe_unused]] auto selected = unique_container.resolve<A>();

try {
    unique_container.resolve<B>();
    assert(false);
} catch (const type_not_convertible_exception&) {
}

std::variant<A, B> existing(std::in_place_type<A>, 9);
container<> external_container;
external_container.register_type<scope<external>, storage<std::variant<A, B>&>>(
    existing);

[[maybe_unused]] auto& ref = external_container.resolve<std::variant<A, B>&>();
assert(&ref == &existing);
assert(std::holds_alternative<A>(ref));

[[maybe_unused]] auto& held = external_container.resolve<A&>();
assert(&held == &std::get<A>(existing));
```

</details>
<!-- } -->

See:

- [examples/container/variant.cpp](../examples/container/variant.cpp)
- [test/container/construct.cpp](../test/container/construct.cpp)
- [test/storage/unique.cpp](../test/storage/unique.cpp)
- [test/storage/shared.cpp](../test/storage/shared.cpp)
- [test/storage/external.cpp](../test/storage/external.cpp)
- [include/dingo/type/type_traits.h](../include/dingo/type/type_traits.h)

## Factories

By default, Dingo tries to select a usable constructor automatically. That keeps
the common case concise, but you can override construction when needed.

Factory styles in the repo:

- automatic constructor deduction
- explicit constructor selection
- static function factory
- stateful callable factory

### Constructor Deduction

The default registration path uses `constructor_detection<T>` from
[include/dingo/factory/constructor_detection.h](../include/dingo/factory/constructor_detection.h).
When you register a type without an explicit `factory<...>`, Dingo tries to pick
a constructor automatically.

In practice, the default path is:

- try constructor deduction for registered types
- use the highest-arity constructor shape that Dingo can prove is constructible
- resolve the detected arguments from the container

`construct<T>()` uses the same deduction machinery for unmanaged objects.

### Constructor-Deduction Limits

Constructor deduction is intentionally useful, not magical. The main limits are:

- detection is based on compile-time constructibility checks, not on every
  overload-resolution nuance you might expect from handwritten code
- the detector prefers the highest-arity constructible shape, which is not
  always the overload you want to commit to in public code
- constructor templates and other generic catch-all constructor shapes are
  rejected from auto-detection; those types must use `factory<constructor<...>>`
- ambiguous or unsupported cases should be resolved explicitly with
  `factory<constructor<...>>`, `factory<function<...>>`, `callable(...)`, or
  `callable<Signature>(...)`
- `construct<T>()` uses constructor deduction directly, but unregistered
  `resolve<T>()` only auto-constructs plain types when they are aggregates or
  explicitly opted in through `is_auto_constructible<T>`
- opting a type into auto-construction does not bypass ambiguity checks
- reference-style wrapper values only participate in the reference-based
  deduction path when the wrapper opts in through
  `type_traits<T>::is_reference_resolvable`
- detection depth is bounded by `DINGO_CONSTRUCTOR_DETECTION_ARGS`

That makes constructor deduction a good default for ordinary classes and
aggregates, but not a substitute for an explicit factory when constructor choice
is part of the type's contract.

Reach for an explicit factory when:

- constructor deduction is ambiguous
- the type exposes a templated or forwarding constructor
- the type should be built through a named factory function
- construction depends on extra callable state
- you want to select a specific alternative for variant construction or storage

<!-- { include("../examples/factory/factory_constructor_deduction.cpp", scope="////", summary="Automatically deduced constructor factory") -->

<details>
<summary>Automatically deduced constructor factory</summary>

Example code included from
[../examples/factory/factory_constructor_deduction.cpp](../examples/factory/factory_constructor_deduction.cpp):

```c++
struct A {
    A(int); // Definition is not required as constructor is not called
    A(double, double) {} // Definition is required as constructor is called
};

container<> container;
container.register_type<scope<external>, storage<double>>(1.1);

// Constructor with a highest arity will be used (factory<> is deduced
// automatically)
container.register_type<scope<unique>,
                        storage<A> /*, factory<constructor_deduction<A>> */>();
```

</details>
<!-- } -->

<!-- { include("../examples/factory/factory_constructor.cpp", scope="////", summary="Explicit constructor selection") -->

<details>
<summary>Explicit constructor selection</summary>

Example code included from
[../examples/factory/factory_constructor.cpp](../examples/factory/factory_constructor.cpp):

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

Templated constructors are supported when the type is registered with an
explicit constructor signature:

```c++
struct Generic {
    template <class First, class Second>
    Generic(First, Second) {}
};

container.register_type<scope<external>, storage<int>>(7);
container.register_type<scope<external>, storage<float>>(3.5f);
container.register_type<scope<unique>, storage<Generic>,
                        factory<constructor<Generic(int, float)>>>();
```

<!-- { include("../examples/factory/factory_function.cpp", scope="////", summary="Static factory-function example") -->

<details>
<summary>Static factory-function example</summary>

Example code included from
[../examples/factory/factory_function.cpp](../examples/factory/factory_function.cpp):

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

<!-- { include("../examples/factory/factory_callable.cpp", scope="////", summary="Stateful callable factory example") -->

<details>
<summary>Stateful callable factory example</summary>

Example code included from
[../examples/factory/factory_callable.cpp](../examples/factory/factory_callable.cpp):

```c++
struct A {
    int value;
};

struct overloaded_factory {
    A operator()(int value) const { return A{value * 2}; }
    A operator()(float value) const { return A{static_cast<int>(value) + 100}; }
};

container<> container;
// Register int that will be passed to the callable below
container.register_type<scope<external>, storage<int>>(2);
// Register A that will be instantiated by calling provided lambda function
// with arguments resolved using the container.
container.register_type<scope<unique>, storage<A>>(callable([](int value) {
    return A{value * 2};
}));
assert(container.resolve<A>().value == 4);
// Explicit signatures also work for overloaded functors.
assert(container.construct<A>(callable<A(int)>(overloaded_factory{})).value ==
       4);
```

</details>
<!-- } -->

See:

- [examples/factory/factory_constructor_deduction.cpp](../examples/factory/factory_constructor_deduction.cpp)
- [examples/factory/factory_constructor.cpp](../examples/factory/factory_constructor.cpp)
- [examples/factory/factory_function.cpp](../examples/factory/factory_function.cpp)
- [examples/factory/factory_callable.cpp](../examples/factory/factory_callable.cpp)
- [test/factory/constructor_detection.cpp](../test/factory/constructor_detection.cpp)
- [test/container/construct.cpp](../test/container/construct.cpp)
- [include/dingo/factory/constructor_detection.h](../include/dingo/factory/constructor_detection.h)
- [include/dingo/factory/constructor.h](../include/dingo/factory/constructor.h)
- [include/dingo/factory/function.h](../include/dingo/factory/function.h)
- [include/dingo/factory/callable.h](../include/dingo/factory/callable.h)

## Interfaces And Service-Locator Style Resolution

Registrations can be exposed through base classes or other interface types. The
upcast is fixed at registration time, which keeps multiple-inheritance cases
predictable without relying on `dynamic_cast` for ordinary resolution.

This is the right fit when:

- concrete implementations should stay hidden behind an interface
- one concrete type should be resolvable through several interfaces

<!-- { include("../examples/container/service_locator.cpp", scope="////", summary="Resolve through an interface") -->

<details>
<summary>Resolve through an interface</summary>

Example code included from
[../examples/container/service_locator.cpp](../examples/container/service_locator.cpp):

```c++
// Interface that will be resolved
struct IA {
    virtual ~IA() {}
};
// Struct implementing the interface
struct A : IA {};

container<> container;

// Register struct A, resolvable as interface IA
container.register_type<scope<shared>, storage<A>, interfaces<IA>>();
// Resolve instance A through interface IA
IA& instance = container.resolve<IA&>();
assert(dynamic_cast<A*>(&instance));
```

</details>
<!-- } -->

See:

- [examples/container/service_locator.cpp](../examples/container/service_locator.cpp)

## Multibindings And Collections

Dingo can register multiple implementations under one interface and then resolve
them as a collection.

This is a good fit for extension-point style code such as:

- processors
- handlers
- strategy lists

The collection can use a standard aggregation rule or a custom one.

<!-- { include("../examples/index/multibindings.cpp", scope="////", summary="Multibindings example") -->

<details>
<summary>Multibindings example</summary>

Example code included from
[../examples/index/multibindings.cpp](../examples/index/multibindings.cpp):

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
                                 interfaces<IProcessor>>();
container.template register_type<scope<shared>, storage<Processor<1>>,
                                 interfaces<IProcessor>>();

// Resolve the collection
container.template resolve<std::vector<IProcessor*>>();
```

</details>
<!-- } -->

<!-- { include("../examples/registration/collection.cpp", scope="////", summary="Custom collection aggregation example") -->

<details>
<summary>Custom collection aggregation example</summary>

Example code included from
[../examples/registration/collection.cpp](../examples/registration/collection.cpp):

```c++
struct ProcessorBase {
    virtual ~ProcessorBase() {}
    virtual void process(const void*) = 0;
    virtual std::type_index type() = 0;
};

template <typename T, typename ProcessorT> struct Processor : ProcessorBase {
    virtual std::type_index type() override { return typeid(T); }

    void process(const void* transaction) override {
        static_cast<ProcessorT*>(this)->process_impl(
            *reinterpret_cast<const T*>(transaction));
    }
};

struct StringProcessor : Processor<std::string, StringProcessor> {
    void process_impl([[maybe_unused]] const std::string& value) {};
};

struct VectorIntProcessor : Processor<std::vector<int>, VectorIntProcessor> {
    void process_impl([[maybe_unused]] const std::vector<int>& value) {};
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
                   interfaces<ProcessorBase>>();
container
    .register_type<scope<unique>, storage<std::unique_ptr<VectorIntProcessor>>,
                   interfaces<ProcessorBase>>();
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

- [examples/index/multibindings.cpp](../examples/index/multibindings.cpp)
- [examples/registration/collection.cpp](../examples/registration/collection.cpp)
- [include/dingo/registration/collection_traits.h](../include/dingo/registration/collection_traits.h)

## Runtime Model And Error Handling

Dingo is runtime-based. That makes cross-module usage practical, but it also
means some wiring errors surface as exceptions during resolution rather than as
pure compile-time failures.

When user code throws during resolution, the container remains in a valid state.
Already completed shared resolutions may stay cached, because resolution is a
recursive process and some work may already have finished successfully.

See:

- [Advanced Topics](advanced-topics.md)
