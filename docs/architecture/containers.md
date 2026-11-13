# Containers

This page documents the public container model.

## Public Entry Points

The top-level include set is small:

- [include/dingo/runtime_container.h](../../include/dingo/runtime_container.h):
  the runtime container, `runtime_container<>`, also named `container<>`
- [include/dingo/static_container.h](../../include/dingo/static_container.h):
  the static container, `static_container<bindings<...>>`
- [include/dingo/container.h](../../include/dingo/container.h): both

Each container keeps its implementation in its own directory. Both directories
use the same file names:

| File                 | Contents               |
| -------------------- | ---------------------- |
| `container.h`        | the container          |
| `registry.h`         | its bindings           |
| `binding.h`          | one binding            |
| `context.h`          | its resolution context |
| `container_traits.h` | its traits             |

All other headers are implementation detail or shared core.

## Internal Layout

The container code has three groups:

- `include/dingo/binding/`: code that both containers share
- `include/dingo/runtime/`: the runtime registry
- `include/dingo/static/`: the static registry and the static graph

This split keeps the public headers small. The runtime and static details stay
in their own directories. [Header Layers](overview.md#header-layers) describes
the full header hierarchy.

## Two Containers

Dingo has two containers, one for each registration mode:

```c++
dingo::container<> runtime;
runtime.register_type<dingo::scope<dingo::shared>, dingo::storage<Config>>();

using app_bindings = dingo::bindings<
    dingo::bind<dingo::scope<dingo::shared>, dingo::storage<Config>>,
    dingo::bind<dingo::scope<dingo::unique>, dingo::storage<Service>>
>;

dingo::static_container<app_bindings> app_container;
```

- `container<>`, also named `runtime_container<>`, owns mutable runtime
  registrations. `register_type<...>()` adds them.
- `static_container<bindings<...>>` carries its bindings in the container type.
  It has no runtime registration API.

The modes do not mix. A runtime container does not hold compile-time bindings. A
runtime registration cannot declare local `bindings<...>`. The registration
container that `register_type<...>()` returns accepts only runtime
registrations.

## Binding Semantics

A request for one object must select one binding. If several bindings match, the
request is ambiguous. The runtime container throws `type_ambiguous_exception`.
The static container rejects the request at compile time.

A request for a collection collects every matching binding.

A parent container is a fallback source. The declarations
`container<> child(&parent)` and `static_container<bindings<...>, Parent>` both
search the child graph first:

- A child binding wins for singular requests.
- The parent graph resolves a dependency that the child does not bind.

The parent of a static container is a static container.

A static binding can carry local `bindings<...>`. They declare a child graph for
the dependencies of that binding. The local bindings shadow the host bindings.
If the local bindings answer a request, singular or collection, the request does
not reach the host.

## Direct Construction

`construct<T>()` uses the registered binding of `T` when one exists.

A wrapper request such as `construct<std::shared_ptr<T>>()` also uses the
registered binding. Dingo resolves the normalized object and wraps the result.
If no binding exists, `construct` uses the constructor of the type.

## Cycle Rules

The static container rejects a cycle at compile time. The exception is a cycle
in which every binding uses a scope that declares `allows_cycles`, such as
`scope<shared_cyclical>`. A supported static cycle uses the same two-phase
storage as a runtime `shared_cyclical` registration. Each binding in the cycle
publishes its instance before its dependencies finish resolving.

Compile-time validation sees the dependencies that a binding declares:

- `dependencies<...>`
- an explicit `constructor<T(Args...)>`
- a function factory

It rejects cycles through these dependencies at compile time. A detected
constructor does not expose its dependencies. A cycle through it is detected at
run time and reported with `type_recursion_exception`. Runtime containers detect
every cycle this way. A binding skips the runtime check only when its factory
has no dependencies, for example a detected zero-argument constructor.

## Related Headers

The main implementation headers of this model are:

- [include/dingo/container.h](../../include/dingo/container.h)
- [include/dingo/runtime/registry.h](../../include/dingo/runtime/registry.h)
- [include/dingo/static/registry.h](../../include/dingo/static/registry.h)
- [include/dingo/static/graph.h](../../include/dingo/static/graph.h)
- [include/dingo/binding/selection.h](../../include/dingo/binding/selection.h)
- [include/dingo/binding/resolution.h](../../include/dingo/binding/resolution.h)
- [include/dingo/resolution/algorithm.h](../../include/dingo/resolution/algorithm.h)
- [include/dingo/resolution/entry_points.h](../../include/dingo/resolution/entry_points.h)
