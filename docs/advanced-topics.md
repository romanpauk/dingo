# Advanced Topics

This page covers the parts of Dingo that usually matter only after the basic
registration and resolution flow is already in place.

## Indexed Resolution

Container traits can define one or more indexes so that a registration is
resolved by both type and a user-supplied key.

Use indexes when:

- multiple implementations share the same interface
- the caller chooses the implementation by name or numeric ID
- the lookup structure should be a specific container type

Built-in index backends:

- `std::map`
- `std::unordered_map`
- `std::array`

Index definitions are scoped to the interface they serve. Every indexed
registration or lookup requires a matching `(Interface, Key)` definition.

Custom backends can specialize
`dingo::detail::index_storage<Backend, Key, Value, Allocator>`. The
specialization must provide `bool emplace(Key, Value)` and `Value* find(Key)`.

Constructor dependencies can also bind to a fixed indexed key:

```c++
struct Pipeline {
    Pipeline(dingo::indexed<IProcessor&, dingo::key<std::size_t, 1>> first,
             dingo::indexed<IProcessor&, dingo::key<std::size_t, 2>> second);
};
```

This is backend-independent. `indexed<IProcessor&, key<std::size_t, 1>>`
resolves the same object as `container.resolve<IProcessor&>(std::size_t{1})`;
the configured `(IProcessor, std::size_t)` index can be `array`, `map`,
`unordered_map`, or a custom backend.

The value in `key<T, Value>` must be a valid non-type template parameter and
must be usable as `T{Value}`. Class-type values such as `key<MyKey, MyKey{1}>`
require C++20 structural non-type template parameter support.

<!-- { include("../examples/index/index.cpp", scope="////") -->

Example code included from
[../examples/index/index.cpp](../examples/index/index.cpp):

```c++
struct IAnimal {
    virtual ~IAnimal() {}
};

struct Dog : IAnimal {};
struct Cat : IAnimal {};

// Declare traits with std::string based index
struct container_traits : dynamic_container_traits {
    using index_definition_type =
        indexes<index<IAnimal, std::string, index_type::unordered_map>>;
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
    template <typename T> MessageWrapper(T&& message) {
        messages_ = std::forward<T>(message);
    }

    size_t id() const { return messages_.index(); }
    const MessageA& GetA() const { return std::get<MessageA>(messages_); }
    const MessageB& GetB() const { return std::get<MessageB>(messages_); }

  private:
    std::variant<std::monostate, MessageA, MessageB> messages_;
};

// Declare message processor hierarchy with dependencies
struct IProcessor {
    virtual ~IProcessor() {}
    virtual void process(const MessageWrapper&) = 0;
};

struct RepositoryA {};
struct ProcessorA : IProcessor {
    ProcessorA(RepositoryA&) {}
    void process(const MessageWrapper& message) override { message.GetA(); }
};

struct RepositoryB {};
struct ProcessorB : IProcessor {
    ProcessorB(RepositoryB&) {}
    void process(const MessageWrapper& message) override { message.GetB(); }
};

struct Pipeline {
    Pipeline(
        dingo::indexed<IProcessor&, dingo::key<size_t, 1>> first_processor,
        dingo::indexed<IProcessor&, dingo::key<size_t, 2>> second_processor)
        : first(first_processor), second(second_processor) {}

    IProcessor& first;
    IProcessor& second;
};

// Define traits type with a single index using size_t as a key,
// backed by a std::array of size 10
struct container_traits : static_container_traits<void> {
    using index_definition_type =
        indexes<index<IProcessor, size_t, index_type::array<10>>>;
};

container<container_traits> container;

// Register processors into the container, indexed by the type they process
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
    container.template resolve<IProcessor&>(msg.id()).process(msg);
}

// Invokes the processor for MessageB that is stateless
{
    MessageWrapper msg((MessageB{1.1f}));
    container.template resolve<IProcessor&>(msg.id()).process(msg);
}
```

<!-- } -->

See:

- [include/dingo/index/index.h](../include/dingo/index/index.h)
- [include/dingo/index/map.h](../include/dingo/index/map.h)
- [include/dingo/index/unordered_map.h](../include/dingo/index/unordered_map.h)
- [include/dingo/index/array.h](../include/dingo/index/array.h)

## Annotated Types

Annotations support multiple implementations for the same interface and
disambiguate them with a tag type.

Use annotations when type-based registration is not enough but runtime key
lookup would be the wrong abstraction.

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
    repository(dingo::annotated<database&, primary_tag> primary_db,
               dingo::annotated<database&, replica_tag> replica_db)
        : primary(primary_db), replica(replica_db) {}

    database& primary;
    database& replica;
};

database primary{1};
database replica{2};
container<> container;

container.register_type<scope<external>, storage<database*>,
                        interfaces<annotated<database, primary_tag>>>(&primary);
container.register_type<scope<external>, storage<database*>,
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
- lookup should avoid mutable runtime registration state

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
    .register_type<scope<external>, storage<int>>(
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
