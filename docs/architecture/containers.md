# Containers

This page documents the current public container model.

## Public Entry Points

The top-level include set is intentionally small:

- [include/dingo/container.h](../../include/dingo/container.h): runtime
  container
- [include/dingo/runtime_container.h](../../include/dingo/runtime_container.h):
  the same runtime container with an explicit name and a stable heap address
- [include/dingo/static_container.h](../../include/dingo/static_container.h):
  compile-time container

Everything else is either runtime/static implementation detail or shared core.

The operations themselves are shared:
[include/dingo/container_operations.h](../../include/dingo/container_operations.h)
owns `resolve`, `construct`, `invoke`, the collection entries, and the
parent/miss hand-off for both registration modes.

## Two Container Shapes

A container has exactly one registration mode:

```c++
// Runtime registration: the binding list is open until the program runs.
dingo::container<> runtime;
runtime.register_type<dingo::scope<dingo::shared>, dingo::storage<Service>>();

// Compile-time registration: the binding list is part of the container type.
using app_bindings = dingo::bindings<
    dingo::bind<dingo::scope<dingo::shared>, dingo::storage<Config>>,
    dingo::bind<dingo::scope<dingo::unique>, dingo::storage<Service>>
>;
dingo::static_container<app_bindings> app;
```

`container<>` and `runtime_container<>` are the runtime shape. They own mutable
runtime registrations and expose `register_type<...>()`.

`static_container<bindings<...>>` is the compile-time shape. It has no runtime
registration API and validates the graph when the container type is formed.

The two shapes are not mixed. A container cannot carry both a compile-time
binding list and runtime registrations.

## Internal Layout

The container stack is split into three groups:

- `include/dingo/binding/`: shared runtime/static substrate
- `include/dingo/runtime/`: runtime registry internals
- `include/dingo/static/`: static registry and graph internals

The public operations shared by the front-ends live in
`include/dingo/container_operations.h`. The front-ends are configurations plus
state: they provide their registry and answer whether they have a runtime
source, and the operations layer owns the decision order.

The split keeps public headers small while leaving runtime and static
implementation details in their own directories. The full header hierarchy is
described in [Header Layers](overview.md#header-layers).

## Binding Semantics

A runtime container answers a request from its runtime registrations. A static
container answers it from its compile-time bindings.

Both shapes support a parent container:

- `container<> child(&parent)`
- `static_container<bindings<...>, Parent> child(&parent)`

A missing child dependency can be resolved from the parent graph. The child's
own binding wins for a singular request.

Collections are assembled from the bindings of the container that answers. A
runtime container can return a collection of its runtime bindings; a static
container can return a collection of its compile-time bindings.

## Direct Construction

`construct<T>()` reuses a registered binding for direct object construction when
that binding exists for `T`.

Wrapper construction such as `construct<std::shared_ptr<T>>()` also reuses the
registered path when Dingo can resolve the normalized object and wrap the
result. If no suitable registered path exists, construction falls back to the
normal constructor-based path.

## Cycle Rules

Purely static cycles are rejected at compile time unless every binding in the
cycle uses `scope<shared_cyclical>`. A supported static cycle uses the same
two-phase storage model as runtime `shared_cyclical` registration, so each
participating slot can be published before its dependencies finish resolving.

In static bindings, cycles through declared dependencies (`dependencies<...>`,
an explicit `constructor<T(Args...)>` or a function factory) are visible to
compile-time validation and are rejected at compile time. Cycles through
auto-detected constructors are not: detected constructors do not expose their
dependencies, so they are detected at runtime and reported with
`type_recursion_exception`. This applies to `static_container` and runtime
containers alike. A binding skips the runtime check only when its factory
provably has no dependencies, such as a detected zero-argument constructor.

## Related Headers

The key implementation headers behind this model are:

- [include/dingo/container.h](../../include/dingo/container.h)
- [include/dingo/container_operations.h](../../include/dingo/container_operations.h)
- [include/dingo/runtime/registry.h](../../include/dingo/runtime/registry.h)
- [include/dingo/static/registry.h](../../include/dingo/static/registry.h)
- [include/dingo/static/graph.h](../../include/dingo/static/graph.h)
- [include/dingo/binding/binding_selection.h](../../include/dingo/binding/binding_selection.h)
- [include/dingo/binding/binding_resolution.h](../../include/dingo/binding/binding_resolution.h)
- [include/dingo/binding/routes.h](../../include/dingo/binding/routes.h)
