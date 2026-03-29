# Advanced Topics

This guide covers the parts of Dingo that are usually only needed after the
basic registration and resolution flow is already working.

## Indexed Resolution

Container traits can define one or more indexes so that a registration is
resolved by both type and a user-supplied key.

This is useful when:

- multiple implementations share the same interface
- the caller chooses the implementation by name or numeric ID
- the lookup structure should be a specific container type

The library ships index support for:

- `std::map`
- `std::unordered_map`
- `std::array`

<!-- { include("../examples/index.cpp", scope="////", summary="Indexed resolution example") -->

<details>
<summary>Indexed resolution example</summary>

Example code included from [../examples/index.cpp](../examples/index.cpp):

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
    scope<shared>, storage<Dog>, interface<IAnimal>>(std::string("dog"));

container.template register_indexed_type<
    scope<shared>, storage<Cat>, interface<IAnimal>>(std::string("cat"));

// Resolve an instance of a dog
auto dog = container.template resolve<IAnimal>(std::string("dog"));
```

</details>
<!-- } -->

<!-- { include("../examples/message_processing.cpp", summary="Indexed message dispatch example") -->

<details>
<summary>Indexed message dispatch example</summary>

Example code included from
[../examples/message_processing.cpp](../examples/message_processing.cpp):

```c++
//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#include <dingo/container.h>
#include <dingo/index/array.h>
#include <dingo/storage/shared.h>

#include <string>

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

int main() {
    using namespace dingo;

    // Define traits type with a single index using size_t as a key,
    // backed by a std::array of size 10
    struct container_traits : static_container_traits<void> {
        using index_definition_type =
            std::tuple<std::tuple<size_t, index_type::array<10>>>;
    };

    container<container_traits> container;

    // Register processors into the container, indexed by the type they process
    container.register_indexed_type<scope<shared>,
                                    storage<std::shared_ptr<ProcessorA>>,
                                    interface<IProcessor>>(size_t(1));
    container.register_indexed_type<scope<unique>,
                                    storage<std::shared_ptr<ProcessorB>>,
                                    interface<IProcessor>>(size_t(2));

    // Register repositories used by the processors
    container.register_type<scope<shared>, storage<RepositoryA>>();
    container.register_type<scope<shared>, storage<RepositoryB>>();

    // Invokes the processor for MessageA that is stateful
    {
        MessageWrapper msg((MessageA{1}));
        container.template resolve<std::shared_ptr<IProcessor>>(msg.id())
            ->process(msg);
    }

    // Invokes the processor for MessageB that is stateless
    {
        MessageWrapper msg((MessageB{1.1}));
        container.template resolve<std::shared_ptr<IProcessor>>(msg.id())
            ->process(msg);
    }
}
```

</details>
<!-- } -->

See:

- [examples/index.cpp](../examples/index.cpp)
- [examples/message_processing.cpp](../examples/message_processing.cpp)
- [include/dingo/index.h](../include/dingo/index.h)
- [include/dingo/index/map.h](../include/dingo/index/map.h)
- [include/dingo/index/unordered_map.h](../include/dingo/index/unordered_map.h)
- [include/dingo/index/array.h](../include/dingo/index/array.h)

## Annotated Types

Annotations let you register multiple implementations for the same interface and
disambiguate them with a tag type.

Use this when type-based registration is not enough but you do not want to add
runtime key lookup.

See:

- [test/annotated.cpp](../test/annotated.cpp)
- [include/dingo/annotated.h](../include/dingo/annotated.h)

## Static And Dynamic Containers

Dingo supports different container traits with different tradeoffs.

- dynamic containers use standard runtime structures and are the default choice
- static containers use more compile-time structure for faster repeated lookup

Static containers are still runtime DI containers. Here, "static" refers to the
container internals, not to compile-time object construction.

Use a static container only when:

- the participating types are known and stable
- you can manage distinct container tags correctly
- the performance tradeoff is worth the extra rigidity

See:

- [examples/message_processing.cpp](../examples/message_processing.cpp)
- [include/dingo/container.h](../include/dingo/container.h)
- [include/dingo/type_map.h](../include/dingo/type_map.h)
- [include/dingo/type_cache.h](../include/dingo/type_cache.h)

## Container Nesting

Containers can form a parent-child hierarchy. Resolution walks from the child
toward the parent chain, which allows local overrides on top of broader shared
configuration.

That makes nesting useful for:

- per-feature overrides
- test-specific replacements
- local configuration layered on top of a base application container

<!-- { include("../examples/nesting.cpp", scope="////", summary="Nested container override example") -->

<details>
<summary>Nested container override example</summary>

Example code included from [../examples/nesting.cpp](../examples/nesting.cpp):

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

- [examples/nesting.cpp](../examples/nesting.cpp)
- [test/nesting.cpp](../test/nesting.cpp)

## Custom RTTI

If standard RTTI is unavailable or undesirable, the container can be
parameterized with a custom RTTI provider.

The repo includes:

- a dynamic RTTI provider based on `typeid`
- a static RTTI provider based on template specializations

See:

- [include/dingo/rtti/typeid_type_info.h](../include/dingo/rtti/typeid_type_info.h)
- [include/dingo/rtti/static_type_info.h](../include/dingo/rtti/static_type_info.h)

## Custom Allocation

The container can take a user-supplied allocator for its own internal data
structures.

This affects container bookkeeping, not the ownership model of every resolved
object. A resolved object may still be externally owned or stored in a standard
smart pointer, depending on its registration.

<!-- { include("../examples/allocator.cpp", scope="////", summary="Custom allocator example") -->

<details>
<summary>Custom allocator example</summary>

Example code included from
[../examples/allocator.cpp](../examples/allocator.cpp):

```c++
// Define a container with user-provided allocator type
std::allocator<char> alloc;
container<dingo::dynamic_container_traits, std::allocator<char>>
    container(alloc);
```

</details>
<!-- } -->

See:

- [examples/allocator.cpp](../examples/allocator.cpp)
- [include/dingo/allocator.h](../include/dingo/allocator.h)
- [include/dingo/arena_allocator.h](../include/dingo/arena_allocator.h)

## Runtime Notes

Some behavior is worth calling out explicitly:

- Dingo is runtime-based, so some wiring errors surface as exceptions
- shared resolutions can act as a cache for already-built objects
- `shared_cyclical` supports cycles through two-phase construction and should be
  treated as a constrained escape hatch rather than the default model

If you are new to the project, read [Getting Started](getting-started.md) and
[Core Concepts](core-concepts.md) first.
