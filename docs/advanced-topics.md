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

<!-- { include("../examples/index/index.cpp", scope="////", summary="Indexed resolution example") -->

<details>
<summary>Indexed resolution example</summary>

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
        std::tuple<std::tuple<std::string, index_type::unordered_map>>;
};

container<container_traits> container;
container.template register_indexed_type<
    scope<shared>, storage<Dog>, interfaces<IAnimal>>(std::string("dog"));

container.template register_indexed_type<
    scope<shared>, storage<Cat>, interfaces<IAnimal>>(std::string("cat"));

// Resolve an instance of a dog
/*auto dog =*/container.template resolve<IAnimal>(std::string("dog"));
```

</details>
<!-- } -->

<!-- { include("../examples/index/message_processing.cpp", scope="////", summary="Indexed message dispatch example") -->

<details>
<summary>Indexed message dispatch example</summary>

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
```

</details>
<!-- } -->

See:

- [examples/index/index.cpp](../examples/index/index.cpp)
- [examples/index/message_processing.cpp](../examples/index/message_processing.cpp)
- [include/dingo/index.h](../include/dingo/index.h)
- [include/dingo/index/map.h](../include/dingo/index/map.h)
- [include/dingo/index/unordered_map.h](../include/dingo/index/unordered_map.h)
- [include/dingo/index/array.h](../include/dingo/index/array.h)

## Annotated Types

Annotations support multiple implementations for the same interface and
disambiguate them with a tag type.

Use annotations when type-based registration is not enough but runtime key
lookup would be the wrong abstraction.

See:

- [test/registration/annotated.cpp](../test/registration/annotated.cpp)
- [include/dingo/registration/annotated.h](../include/dingo/registration/annotated.h)

## Static And Dynamic Containers

Dingo offers different container traits with different tradeoffs.

- dynamic containers use standard runtime structures and are the default choice
- static containers use more compile-time structure for faster repeated lookup

Static containers are still runtime DI containers. Here, "static" refers to the
container internals, not to compile-time object construction.

Static containers make sense when:

- the participating types are known and stable
- distinct container tags can be managed correctly
- the performance tradeoff is worth the extra rigidity

See:

- [examples/index/message_processing.cpp](../examples/index/message_processing.cpp)
- [include/dingo/container.h](../include/dingo/container.h)
- [include/dingo/type/type_map.h](../include/dingo/type/type_map.h)
- [include/dingo/resolution/type_cache.h](../include/dingo/resolution/type_cache.h)

## Container Nesting

Containers can form a parent-child hierarchy. Resolution walks from the child
toward the parent chain, which allows local overrides on top of broader shared
configuration.

Nesting is most useful for:

- per-feature overrides
- test-specific replacements
- local configuration layered on top of a base application container

<!-- { include("../examples/container/nesting.cpp", scope="////", summary="Nested container override example") -->

<details>
<summary>Nested container override example</summary>

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

</details>
<!-- } -->

See:

- [examples/container/nesting.cpp](../examples/container/nesting.cpp)
- [test/container/nesting.cpp](../test/container/nesting.cpp)

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

<!-- { include("../examples/container/allocator.cpp", scope="////", summary="Custom allocator example") -->

<details>
<summary>Custom allocator example</summary>

Example code included from
[../examples/container/allocator.cpp](../examples/container/allocator.cpp):

```c++
// Define a container with user-provided allocator type
std::allocator<char> alloc;
container<dingo::dynamic_container_traits, std::allocator<char>>
    container(alloc);
```

</details>
<!-- } -->

See:

- [examples/container/allocator.cpp](../examples/container/allocator.cpp)
- [include/dingo/memory/allocator.h](../include/dingo/memory/allocator.h)
- [include/dingo/memory/arena_allocator.h](../include/dingo/memory/arena_allocator.h)

## Runtime Notes

Two runtime details are easy to miss:

- Dingo is runtime-based, so some wiring errors surface as exceptions
- shared resolutions can act as a cache for already-built objects
- `shared_cyclical` supports cycles through two-phase construction and should be
  treated as a constrained escape hatch rather than the default model

For a first pass through the project, read [Getting Started](getting-started.md)
and [Core Concepts](core-concepts.md) first.
