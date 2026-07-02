# Advanced Topics

This page covers the parts of Dingo that usually matter only after the basic
registration and resolution flow is already in place.

## View Resolution

Container traits can define one or more lookups so that a registration is
resolved by interface, key domain, and cardinality.

Use lookups when:

- multiple implementations share the same interface
- the caller chooses the implementation by name or numeric ID
- the binding should remain separate from unkeyed resolution

View definitions are scoped to the interface they serve. Every keyed
registration or runtime request requires a matching lookup definition.

Unkeyed runtime registrations without an explicit lookup are treated as
`no_key`/`one`: a second registration for the same interface is rejected,
regardless of storage type. Declare `collection<I>` when an interface is meant
to have multiple unkeyed implementations:

```c++
struct container_traits : dynamic_container_traits {
  using lookup_definition_type = lookups<collection<IProcessor>>;
};
```

`collection<I>` makes unkeyed collection membership explicit:
`resolve<std::vector<I *>>()` enumerates the registrations in registration
order, while singular `resolve<I &>()` succeeds only when exactly one
registration matches.

Runtime-keyed lookups use `associative<K, I>` for unique bindings and
`associative<K, I, many>` for keyed collections:

```c++
struct container_traits : dynamic_container_traits {
  using lookup_definition_type =
      lookups<associative<std::size_t, IProcessor>,
                associative<std::string, IHandler, many>>;
};
```

`associative<K, I>` uses the `ordered` backend by default:
`associative<Key, Interface, Cardinality = one, Backend = ordered>`. The
built-in backend tags are:

- `ordered`: map-like lookup by runtime key, `O(log N)` plus rows for the key.
- `unordered`: hash-map-like lookup by runtime key, average `O(1)` plus rows for
  the key.
- `array<N>`: direct indexed lookup for dense integral or enum keys in `[0, N)`.

For example:

```c++
struct container_traits : dynamic_container_traits {
  using lookup_definition_type =
      lookups<associative<std::size_t, IProcessor, many, unordered>,
              associative<MessageId, IHandler, one, array<32>>>;
};
```

Custom backends can be used as `associative<MyKey, IService, many, my_backend>`.
A backend tag provides
`Backend::template storage<Key, Mapped, Cardinality, Allocator>`, where Dingo
chooses `Mapped` for its internal lookup entry pointer. The storage is
constructed with the container allocator and should expose an STL-like mapped
container API. For `one`, Dingo uses `find(key)`, `end()`,
`try_emplace(key, mapped)`, and `erase(iterator)`. For `many`, Dingo uses
`equal_range(key)`, `emplace(key, mapped)`, and `erase(iterator)`. Collection
iteration order is defined by the backend; `many` backends are not required to
preserve registration order.

Typed-key registrations can also use explicit lookups:

```c++
struct container_traits : dynamic_container_traits {
  using lookup_definition_type =
      lookups<typed<Primary, IProcessor, one>>;
};
```

`typed<K, I, one>` gives `register_type<interfaces<I>, key<K>>()` singular
identity for that interface and key type. `typed<K, I, many>` makes
`resolve<std::vector<I *>>(key<K>{})` enumerate keyed collection members in
registration order. A singular typed-key resolve succeeds only when the matching
`many` lookup contains exactly one registration.

When no explicit typed-key lookup is declared, `key<K>` registrations still use
implicit `typed_key<K>`/`one`: a second registration for the same interface and
key type is rejected, regardless of storage type. Use `typed<K, I, many>` when
one typed key should hold multiple implementations.

No-key and typed-key lookups use the library's internal row storage; only
runtime-keyed `associative` lookups have configurable storage backends.

Constructor dependencies can also bind to a fixed request key:

```c++
struct Pipeline {
    Pipeline(dingo::request<IProcessor&, dingo::key<std::size_t, 1>> first,
             dingo::request<IProcessor&, dingo::key<std::size_t, 2>> second);
};
```

`request<IProcessor&, key<std::size_t, 1>>` resolves the same object as
`container.resolve<IProcessor&>(std::size_t{1})`; the configured
`associative<std::size_t, IProcessor>` lookup determines the key domain and
cardinality, while the lookup storage remains an implementation detail.

The value in `key<T, Value>` must be a valid non-type template parameter and
must be usable as `T{Value}`. Class-type values such as `key<MyKey, MyKey{1}>`
require C++20 structural non-type template parameter support.

Static containers can use the same `associative<K, I>` lookups when the key
value is fixed in the type:

- `key<K>` remains a typed key and maps to `typed<K, I, ...>`.
- `key<K, Value>` maps to `associative<K, I, ...>` when `associative<K, I>` or
  `associative<K, I, many>` is declared.
- Static request selection is type-encoded: use `request<I &, key<K, Value>>` or
  `resolve<Collection>(key<K, Value>{})`; `resolve<I &>(K{...})` is runtime key
  lookup and is not supported by `static_container`.
- Static fixed runtime-key bindings require `static_container` with traits that
  declare the lookup. `container<bindings<...>>` rejects them because that mixed
  form cannot carry custom lookup traits.

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
      dingo::request<IProcessor &, dingo::key<std::size_t, 0>> first_processor)
      : first(first_processor) {}

  IProcessor &first;
};

using source = dingo::bindings<
    dingo::bind<dingo::scope<dingo::shared>, dingo::storage<Processor<0>>,
                dingo::interfaces<IProcessor>, dingo::key<std::size_t, 0>>,
    dingo::bind<dingo::scope<dingo::shared>, dingo::storage<Processor<1>>,
                dingo::interfaces<IProcessor>, dingo::key<std::size_t, 1>>>;

struct container_traits : dingo::static_container_traits<> {
  using lookup_definition_type =
      dingo::lookups<dingo::associative<std::size_t, IProcessor>>;
};

dingo::static_container<source, container_traits> container;

auto &first =
    container
        .resolve<dingo::request<IProcessor &, dingo::key<std::size_t, 0>>>();
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
struct container_traits : dynamic_container_traits {
  using lookup_definition_type = lookups<associative<std::string, IAnimal>>;
};

container<container_traits> container;
container.template register_indexed_type<
    scope<shared>, storage<Dog>, interfaces<IAnimal>>(std::string("dog"));

container.template register_indexed_type<
    scope<shared>, storage<Cat>, interfaces<IAnimal>>(std::string("cat"));

// Resolve an instance of a dog
auto dog = container.template resolve<IAnimal>(std::string("dog"));
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
  Pipeline(dingo::request<IProcessor &, dingo::key<size_t, 1>> first_processor,
           dingo::request<IProcessor &, dingo::key<size_t, 2>> second_processor)
      : first(first_processor), second(second_processor) {}

  IProcessor &first;
  IProcessor &second;
};

// Define traits type with a single lookup using size_t as a key
struct container_traits : static_container_traits<void> {
  using lookup_definition_type = lookups<associative<size_t, IProcessor>>;
};
// Runtime lookup storage is dynamic even when this
// example uses static_container_traits for the rest of the container.

container<container_traits> container;

// Register processors into the container, keyed by the type they process
container
    .register_indexed_type<scope<shared>, storage<std::shared_ptr<ProcessorA>>,
                           interfaces<IProcessor>>(size_t(1));
container
    .register_indexed_type<scope<shared>, storage<std::shared_ptr<ProcessorB>>,
                           interfaces<IProcessor>>(size_t(2));

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

Annotations support multiple implementations for the same interface and
disambiguate them with a tag type.

Use annotations when type-based registration is not enough but runtime key
lookups would be the wrong abstraction.

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

- [test/matrix/README.md](../test/matrix/README.md)
- [include/dingo/registration/annotated.h](../include/dingo/registration/annotated.h)

## Runtime And Compile-Time Registration

Dingo has two registration modes with different tradeoffs.

- Runtime registration uses `container<>` or `runtime_container` and adds
  registrations with `register_type<...>()`.
- Compile-time registration uses `bindings<...>` with `container<bindings<...>>`
  or `static_container<bindings<...>>`.
- `container<bindings<...>>` may also accept runtime registrations, which is
  useful when most of the graph is static but a small boundary remains dynamic.

The modes share the same resolution API and policy vocabulary. The difference is
where the registration list lives: runtime container state or the container
type.

Runtime registration makes sense when:

- registrations are discovered or selected at runtime
- plugins, tests, or modules need to add registrations independently
- the graph is intentionally open-ended

Compile-time bindings make sense when:

- the participating types are known and stable at compile time
- missing dependencies and unsupported cycles should fail during compilation
- lookups should avoid mutable runtime registration state

See:

- [examples/container/quick_runtime.cpp](../examples/container/quick_runtime.cpp)
- [examples/registration/compile_time_registration.cpp](../examples/registration/compile_time_registration.cpp)
- [architecture/containers.md](architecture/containers.md)
- [include/dingo/container.h](../include/dingo/container.h)
- [include/dingo/runtime_container.h](../include/dingo/runtime_container.h)
- [include/dingo/static_container.h](../include/dingo/static_container.h)

## Container Nesting

Containers can form a parent-child hierarchy. Resolution walks from the child
toward the parent chain, which allows local overrides on top of broader shared
configuration. Runtime-only containers use `container<> child(&parent)`. Mixed
static/runtime containers use `container<bindings<...>, Parent>` and construct
the child with `child(&parent)`. Purely static containers use
`static_container<bindings<...>, Parent>` the same way. Static bindings in the
child can resolve missing dependencies from the parent's static or runtime
graph.

Nesting is most useful for:

- per-feature overrides
- test-specific replacements
- local configuration layered on top of a base application container

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

- [test/matrix/README.md](../test/matrix/README.md)

## Custom RTTI

If standard RTTI is unavailable or undesirable, the container can be
parameterized with a custom RTTI provider.

Two providers ship in-tree:

- a dynamic RTTI provider based on `typeid`
- a static RTTI provider based on template specializations

See:

- [include/dingo/rtti/typeid_provider.h](../include/dingo/rtti/typeid_provider.h)
- [include/dingo/rtti/static_provider.h](../include/dingo/rtti/static_provider.h)

## Custom Allocation

The container can take a user-supplied allocator for its own internal data
structures.

This affects container bookkeeping, not the ownership model of every resolved
object. A resolved object may still be externally owned or stored in a standard
smart pointer, depending on its registration.

<!-- { include("../examples/container/allocator.cpp", scope="////") -->

Example code included from
[../examples/container/allocator.cpp](../examples/container/allocator.cpp):

```c++
// Define a container with user-provided allocator type
std::allocator<char> alloc;
container<dingo::dynamic_container_traits, std::allocator<char>>
    container(alloc);
```

<!-- } -->

See:

- [include/dingo/memory/allocator.h](../include/dingo/memory/allocator.h)
- [include/dingo/memory/arena_allocator.h](../include/dingo/memory/arena_allocator.h)

## Runtime Notes

Some resolution details are easy to miss:

- runtime registration errors surface as exceptions during resolution, while
  compile-time bindings can diagnose declared graph errors during compilation
- shared resolutions can act as a cache for already-built objects
- `shared_cyclical` supports runtime and static cycles through two-phase
  construction and should be treated as a constrained escape hatch rather than
  the default model

For a first pass through the project, read [Getting Started](getting-started.md)
and [Core Concepts](core-concepts.md) first.
