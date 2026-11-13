# Advanced Topics

This page covers topics beyond basic registration and resolution.

## Lookup Resolution

Container traits can define one or more lookups. A lookup resolves a
registration by interface, key domain and cardinality.

Use lookups when:

- several implementations share the same interface
- the caller selects the implementation by name or numeric ID
- the binding must stay separate from unkeyed resolution

A lookup definition applies to the interface that it names. Each keyed
registration and each keyed runtime request needs a matching lookup definition.

A request uses the lookup of its interface, or else the lookup of its normalized
interface. For example, `resolve<IProcessor *>(key)` and
`resolve<std::shared_ptr<IProcessor>>(key)` use the lookup of `IProcessor`. A
registration of an array binds two interfaces: the array and its element. With
`associative<K, T[N]>`, requests of the array, such as `resolve<T (&)[N]>(key)`,
are keyed. Without it, the registration is keyed only under the lookup of `T`.

An unkeyed runtime registration without an explicit lookup uses an implicit
singular lookup (`none_t` key, cardinality `one`). The container rejects a
second registration for the same interface, for every storage type. Declare
`collection<I>` when an interface has several unkeyed implementations:

```c++
struct container_traits : runtime_container_traits {
  using lookup_definition_type = lookups<collection<IProcessor>>;
};
```

With `collection<I>`:

- `resolve<std::vector<I *>>()` returns the registrations in registration order.
- `resolve<I &>()` succeeds only when exactly one registration matches.

Runtime keys use `associative<K, I>` for unique bindings and
`associative<K, I, many>` for keyed collections:

```c++
struct container_traits : runtime_container_traits {
  using lookup_definition_type =
      lookups<associative<std::size_t, IProcessor>,
                associative<std::string, IHandler, many>>;
};
```

The full form is
`associative<Key, Interface, Cardinality = one, Backend = ordered>`. The
built-in backends are:

- `ordered`: map lookup by runtime key, `O(log N)` plus the entries of the key.
- `unordered`: hash map lookup by runtime key, average `O(1)` plus the entries
  of the key.
- `array<N>`: direct indexed lookup for dense integral or enum keys in `[0, N)`.

For example:

```c++
struct container_traits : runtime_container_traits {
  using lookup_definition_type =
      lookups<associative<std::size_t, IProcessor, many, unordered>,
              associative<MessageId, IHandler, one, array<32>>>;
};
```

A custom backend is a tag type, used as
`associative<MyKey, IService, many, my_backend>`. The tag provides
`Backend::template storage<Key, Value, Cardinality, Allocator>`. Dingo selects
`Value`, an internal lookup entry that points to the binding. Dingo constructs
the storage with the container allocator. The storage must have an STL-like map
interface:

- For `one`: `find(key)`, `end()`, `try_emplace(key, mapped)` and
  `erase(iterator)`.
- For `many`: `equal_range(key)`, `emplace(key, mapped)` and `erase(iterator)`.

The backend defines the iteration order of a collection. A `many` backend does
not have to keep the registration order.

The registration containers that `register_type<...>()` returns have the type of
the container they are made in. They keep their lookups in a dense table of
lookup scopes. The root container owns this table. A registration container uses
its scope ID as the index into the table. Then it does the lookup in its own
state, with the declared backend, cardinality and logical key. The container
allocates this state when the registration container registers its first local
binding.

Typed-key registrations can also use explicit lookups:

```c++
struct container_traits : runtime_container_traits {
  using lookup_definition_type =
      lookups<typed<Primary, IProcessor, one>>;
};
```

`typed<K, I, one>` allows one registration of
`register_type<interfaces<I>, key_type<K>>()` for each interface and key type.
`typed<K, I, many>` allows several registrations.
`resolve<std::vector<I *>>(key_type<K>{})` then returns them in registration
order. A singular typed-key resolve succeeds only when the `many` lookup
contains exactly one registration.

If no typed-key lookup is declared, a `key_type<K>` registration uses an
implicit `typed<K, I, one>` lookup. The container rejects a second registration
for the same interface and key type, for every storage type. Use
`typed<K, I, many>` when one typed key must hold several implementations.

Unkeyed and typed-key lookups use internal storage. Only runtime-keyed
`associative` lookups have configurable backends.

A constructor dependency can bind to a fixed request key:

```c++
struct Pipeline {
    Pipeline(dingo::dependency<IProcessor&, dingo::key_type<std::size_t, 1>> first,
             dingo::dependency<IProcessor&, dingo::key_type<std::size_t, 2>> second);
};
```

`dependency<IProcessor&, key_type<std::size_t, 1>>` resolves the same object as
`container.resolve<IProcessor&>(std::size_t{1})`. The declared
`associative<std::size_t, IProcessor>` lookup sets the key domain and the
cardinality. The lookup storage is an implementation detail.

The value in `key_type<T, Value>` must be a valid non-type template parameter.
It must be usable as `T{Value}`. Class-type values, such as
`key_type<MyKey, MyKey{1}>`, need C++20 structural non-type template parameters.

Static containers can use the same `associative<K, I>` lookups when the type
fixes the key value:

- `key_type<K>` is a typed key. It maps to `typed<K, I, ...>`.
- `key_type<K, Value>` maps to `associative<K, I, ...>` when the traits declare
  `associative<K, I>` or `associative<K, I, many>`.
- Static dependency selection uses types: `dependency<I &, key_type<K, Value>>`
  or `resolve<Collection>(key_type<K, Value>{})`. `resolve<I &>(K{...})` is a
  runtime key lookup, and `static_container` does not support it.
- A static binding with a fixed runtime key needs `static_container` traits that
  declare the lookup.

<!-- { include("../examples/index/static_fixed.cpp", scope="////") -->

Example code included from
[../examples/index/static_fixed.cpp](../examples/index/static_fixed.cpp):

```c++
struct IProcessor {
  virtual ~IProcessor() = default;
  virtual int id() const = 0;
};

template <int Id> struct Processor : IProcessor {
  int id() const override { return Id; }
};

struct Pipeline {
  explicit Pipeline(
      dingo::dependency<IProcessor &, dingo::key_type<std::size_t, 0>>
          first_processor)
      : first(first_processor) {}

  IProcessor &first;
};

using source = dingo::bindings<
    dingo::bind<dingo::scope<dingo::shared>, dingo::storage<Processor<0>>,
                dingo::interfaces<IProcessor>, dingo::key_type<std::size_t, 0>>,
    dingo::bind<dingo::scope<dingo::shared>, dingo::storage<Processor<1>>,
                dingo::interfaces<IProcessor>,
                dingo::key_type<std::size_t, 1>>>;

struct container_traits : dingo::static_container_traits {
  using lookup_definition_type =
      dingo::lookups<dingo::associative<std::size_t, IProcessor>>;
};

dingo::static_container<source, container_traits> container;

auto &first = container.resolve<
    dingo::dependency<IProcessor &, dingo::key_type<std::size_t, 0>>>();
auto pipeline = container.construct<Pipeline>();

return first.id() == 0 && &pipeline.first == &first ? 0 : 1;
```

<!-- } -->

<!-- { include("../examples/index/index.cpp", scope="////") -->

Example code included from
[../examples/index/index.cpp](../examples/index/index.cpp):

```c++
struct IAnimal {
  virtual ~IAnimal() = default;
};

struct Dog : IAnimal {};
struct Cat : IAnimal {};

// Declare traits with a std::string based lookup
struct container_traits : runtime_container_traits {
  using lookup_definition_type = lookups<associative<std::string, IAnimal>>;
};

container<container_traits> container;
container
    .template register_type<scope<shared>, storage<Dog>, interfaces<IAnimal>>(
        dingo::key_value{std::string("dog")});

container
    .template register_type<scope<shared>, storage<Cat>, interfaces<IAnimal>>(
        dingo::key_value{std::string("cat")});

// Resolve the dog by its interface
IAnimal &dog = container.template resolve<IAnimal &>(std::string("dog"));
```

<!-- } -->

<!-- { include("../examples/index/message_processing.cpp", scope="////") -->

Example code included from
[../examples/index/message_processing.cpp](../examples/index/message_processing.cpp):

```c++
// Declare Messages and a wrapper that can hold one of the messages,
// resembling protobuf structure
struct MessageA {
  int value;
};
struct MessageB {
  float value;
};

struct MessageWrapper {
  template <typename T> MessageWrapper(T &&message) {
    messages_ = std::forward<T>(message);
  }

  size_t id() const { return messages_.index(); }
  const MessageA &GetA() const { return std::get<MessageA>(messages_); }
  const MessageB &GetB() const { return std::get<MessageB>(messages_); }

private:
  std::variant<std::monostate, MessageA, MessageB> messages_;
};

// Declare message processor hierarchy with dependencies
struct IProcessor {
  virtual ~IProcessor() = default;
  virtual void process(const MessageWrapper &) = 0;
};

struct RepositoryA {};
struct ProcessorA : IProcessor {
  ProcessorA(RepositoryA &) {}
  void process(const MessageWrapper &message) override { message.GetA(); }
};

struct RepositoryB {};
struct ProcessorB : IProcessor {
  ProcessorB(RepositoryB &) {}
  void process(const MessageWrapper &message) override { message.GetB(); }
};

struct Pipeline {
  Pipeline(dingo::dependency<IProcessor &, dingo::key_type<size_t, 1>>
               first_processor,
           dingo::dependency<IProcessor &, dingo::key_type<size_t, 2>>
               second_processor)
      : first(first_processor), second(second_processor) {}

  IProcessor &first;
  IProcessor &second;
};

// Define traits type with a single lookup using size_t as a key
struct container_traits : static_container_traits {
  using lookup_definition_type = lookups<associative<size_t, IProcessor>>;
};
// Runtime lookup storage is dynamic even when this
// example uses static_container_traits for the rest of the container.

container<container_traits> container;

// Register processors into the container, keyed by the type they process
container.register_type<scope<shared>, storage<std::shared_ptr<ProcessorA>>,
                        interfaces<IProcessor>>(dingo::key_value{size_t(1)});
container.register_type<scope<shared>, storage<std::shared_ptr<ProcessorB>>,
                        interfaces<IProcessor>>(dingo::key_value{size_t(2)});

// Register repositories used by the processors
container.register_type<scope<shared>, storage<RepositoryA>>();
container.register_type<scope<shared>, storage<RepositoryB>>();

auto pipeline = container.construct<Pipeline>();

// Invokes the processor for MessageA that is stateful
{
  MessageWrapper msg((MessageA{1}));
  container.template resolve<IProcessor &>(msg.id()).process(msg);
}

// Invokes the processor for MessageB that is stateless
{
  MessageWrapper msg((MessageB{1.1f}));
  container.template resolve<IProcessor &>(msg.id()).process(msg);
}
```

<!-- } -->

See:

- [include/dingo/lookup/lookup.h](../include/dingo/lookup/lookup.h)

## Annotated Types

Annotations let several implementations share one interface. A tag type tells
them apart.

Use annotations when type-based registration is not sufficient, but runtime key
lookups are the wrong abstraction.

<!-- { include("../examples/registration/annotated.cpp", scope="////") -->

Example code included from
[../examples/registration/annotated.cpp](../examples/registration/annotated.cpp):

```c++
struct primary_tag {};
struct replica_tag {};

struct database {
  int id;
};

struct repository {
  repository(dingo::annotated<database &, primary_tag> primary_db,
             dingo::annotated<database &, replica_tag> replica_db)
      : primary(primary_db), replica(replica_db) {}

  database &primary;
  database &replica;
};

database primary{1};
database replica{2};
container<> container;

container.register_type<scope<external>, storage<database *>,
                        interfaces<annotated<database, primary_tag>>>(&primary);
container.register_type<scope<external>, storage<database *>,
                        interfaces<annotated<database, replica_tag>>>(&replica);
container.register_type<scope<unique>, storage<repository>>();

[[maybe_unused]] auto repo = container.resolve<repository>();
assert(&repo.primary == &primary);
assert(&repo.replica == &replica);
```

<!-- } -->

See:

- [spec/semantics.py](../spec/semantics.py)
- [include/dingo/type/annotated.h](../include/dingo/type/annotated.h)

## Runtime And Compile-Time Registration

Dingo has two registration modes with different tradeoffs:

- Runtime registration uses `container<>` or `runtime_container`. `container<>`
  is an alias of `runtime_container`. `register_type<...>()` adds the
  registrations.
- Compile-time registration uses `bindings<...>` with
  `static_container<bindings<...>>`.

Both modes use the same resolution API and the same policies. A runtime
container keeps the registration list in its state. A static container keeps it
in its type. A container uses one mode only. A runtime container cannot hold
compile-time bindings, and a runtime registration cannot declare local
`bindings<...>`.

Use runtime registration when:

- the program discovers or selects registrations at runtime
- plugins, tests or modules add registrations independently
- the graph is open-ended by design

Use compile-time bindings when:

- the participating types are known and stable at compile time
- missing dependencies and unsupported cycles must fail during compilation
- lookups must not use mutable runtime registration state

See:

- [examples/container/quick_runtime.cpp](../examples/container/quick_runtime.cpp)
- [examples/registration/compile_time_registration.cpp](../examples/registration/compile_time_registration.cpp)
- [architecture/containers.md](architecture/containers.md)
- [include/dingo/container.h](../include/dingo/container.h)
- [include/dingo/runtime_container.h](../include/dingo/runtime_container.h)
- [include/dingo/static_container.h](../include/dingo/static_container.h)

## Template Depth Of Compile-Time Bindings

A compile-time container resolves a binding with code that the compiler
generates for that binding. That code resolves the dependencies of the binding
with code generated for each dependency. The compiler instantiates these
functions one inside the other. In a dependency chain, each type depends on one
other type, and each link adds one nested set of instantiations.

`-ftemplate-depth` limits the nesting depth. The depth follows the longest
dependency chain, not the number of bindings. Bindings that do not depend on
each other, or that all depend on the same type, do not nest. A binding with a
detected constructor, and a `unique` binding, add more levels to each link than
a binding with an explicit factory.

If a long chain does not compile, do one of these:

- Raise `-ftemplate-depth`.
- Register the chain through `container<>` or `runtime_container`. Runtime
  registration resolves through type-erased bindings, so its depth does not
  depend on the length of the chain.

The levels of one link are the function templates of one resolution that Clang
instantiates one inside the other: the storage, the factory, the constructor
argument, the context and the container lookup. The compiler instantiates a
function with a deduced return type (`auto`, `decltype(auto)`) at the point of
the call, so the function does not nest. For this reason, the placement
`construct` overloads of the storages and factories return `auto`. A function
that is part of a dependency cycle, such as the constructor argument conversion,
must keep an explicit return type. A function with a deduced return type cannot
be used while its own return type is being deduced.

## Container Nesting

Containers can form a parent-child hierarchy. Resolution searches the child
first and then the parent chain. This allows local overrides on top of a shared
base configuration.

- A runtime child is `container<> child(&parent)`.
- A static child is `static_container<bindings<...>, Parent>`. Construct it the
  same way, with a static parent. Static bindings in the child can resolve
  missing dependencies from the graph of the parent.

Nesting is useful for:

- per-feature overrides
- test-specific replacements
- local configuration on top of a base application container

<!-- { include("../examples/container/nesting.cpp", scope="////") -->

Example code included from
[../examples/container/nesting.cpp](../examples/container/nesting.cpp):

```c++
struct A {
  int value;
};
struct B {
  int value;
};

container<> base_container;
base_container.register_type<scope<external>, storage<int>>(1);
base_container.register_type<scope<unique>, storage<A>>();
// Resolving A will use A{1} to construct A
assert(base_container.resolve<A>().value == 1);

base_container.register_type<scope<unique>, storage<B>>()
    ->register_type<scope<external>, storage<int>>(
        2); // Override value of int for struct B
// Resolving B will use B{2} to construct B
assert(base_container.resolve<B>().value == 2);

// Create a nested container, overriding B (effectively removing override in
// base_container)
container<> nested_container(&base_container);
nested_container.register_type<scope<unique>, storage<B>>();
// Resolving B using nested container will use B{1} as provided by the
// parent container to construct B
assert(nested_container.resolve<B>().value == 1);
```

<!-- } -->

See:

- [test/container/parent/](../test/container/parent)

## Replacing Registrations

If the singular lookup of a type is already taken, `register_type<...>()` throws
`lookup_already_registered_exception`. This is the default policy,
`reject_duplicates`. The `replace_until_resolved` policy lets the latest
registration win until the first use of the container. Tests use it to start
from the application registrations and replace some services with fakes:

```c++
struct test_traits : runtime_container_traits {
  using duplicate_registration_policy = replace_until_resolved;
};

container<test_traits> container;
container.register_type<scope<shared>, storage<EmailNotifier>,
                        interfaces<INotifier>>();
container.register_type<scope<shared>, storage<BillingService>>();

// Redirects the INotifier lookup, BillingService is constructed with
// FakeNotifier.
container.register_type<scope<shared>, storage<FakeNotifier>,
                        interfaces<INotifier>>();

container.resolve<BillingService &>();

// The first successful resolve sealed the container. A replacement throws
// container_sealed_exception. Registering new types is still allowed.
```

Replacement rules:

- Replacement works per lookup. The container keeps ownership of the replaced
  binding. Only the taken lookup points to the new binding.
- Collections are not affected. A registration into a collection always appends.
- The first successful resolve seals the container. A resolve that throws does
  not seal it.
- A resolve through a child container seals the parent only when the parent
  takes part in the resolution.

## Moving Containers

Runtime containers, `container<>` and `runtime_container`, are movable but not
copyable. A runtime container keeps its state in one allocation from the
container allocator. A move transfers this allocation. For this reason, these
stay valid when the container moves:

- registrations
- resolved instances
- the registration containers that `register_type<...>()` returns
- child containers constructed with `container<> child(&parent)`

This supports builder-style setup that registers types and then hands the
container over. A moved-from container can only be destroyed or assigned to.

Static containers keep their instances inline. They are not movable and not
copyable.

## Custom RTTI

If standard RTTI is not available or not wanted, use a custom RTTI provider. The
`rtti_type` member of the container traits selects the provider.

Dingo has two providers:

- a dynamic provider based on `typeid`, the default of
  `runtime_container_traits`
- a static provider based on the address of a per-type variable, the default of
  `static_container_traits`

See:

- [include/dingo/rtti/typeid_provider.h](../include/dingo/rtti/typeid_provider.h)
- [include/dingo/rtti/static_provider.h](../include/dingo/rtti/static_provider.h)

## Custom Allocation

A container can use a user-supplied allocator for its internal data structures.

The allocator affects the bookkeeping of the container. It does not change the
ownership of resolved objects. Depending on its registration, a resolved object
can still be externally owned or stored in a standard smart pointer.

<!-- { include("../examples/container/allocator.cpp", scope="////") -->

Example code included from
[../examples/container/allocator.cpp](../examples/container/allocator.cpp):

```c++
// Define a container with user-provided allocator type
std::allocator<char> alloc;
container<dingo::runtime_container_traits, std::allocator<char>>
    container(alloc);
```

<!-- } -->

See:

- [include/dingo/memory/allocator.h](../include/dingo/memory/allocator.h)
- [include/dingo/memory/arena_allocator.h](../include/dingo/memory/arena_allocator.h)

## Runtime Notes

These resolution details are easy to miss:

- A runtime registration error is reported as an exception during resolution.
  Compile-time bindings can report declared graph errors during compilation.
- Shared resolutions can act as a cache for objects that are already built.
- `shared_cyclical` supports runtime and static cycles through two-phase
  construction. It is a constrained exception for cycles, not the default model.

Before this page, read [Getting Started](getting-started.md) and
[Core Concepts](core-concepts.md).
