# Containers

This page documents the current public container model.

## Public Entry Points

The top-level include set is intentionally small:

- [include/dingo/runtime_container.h](../../include/dingo/runtime_container.h):
  the runtime container, `runtime_container<>`, also named `container<>`
- [include/dingo/static_container.h](../../include/dingo/static_container.h):
  the static container, `static_container<bindings<...>>`
- [include/dingo/container.h](../../include/dingo/container.h): both

Each container keeps its implementation in its own directory under the same file
names: `runtime/container.h` and `static/container.h` for the container,
`registry.h` for its bindings, `binding.h` for a binding, `context.h` for its
resolution context and `container_traits.h` for its traits.

Everything else is either runtime/static implementation detail or shared core.

## Internal Layout

The container stack is split into three groups:

- `include/dingo/binding/`: shared runtime/static substrate
- `include/dingo/runtime/`: runtime registry internals
- `include/dingo/static/`: static registry and graph internals

The split keeps public headers small while leaving runtime and static
implementation details in their own directories. The full header hierarchy is
described in [Header Layers](overview.md#header-layers).

## Two Containers

Dingo has two containers, one per registration mode:

```c++
dingo::container<> runtime;
runtime.register_type<dingo::scope<dingo::shared>, dingo::storage<Config>>();

using app_bindings = dingo::bindings<
    dingo::bind<dingo::scope<dingo::shared>, dingo::storage<Config>>,
    dingo::bind<dingo::scope<dingo::unique>, dingo::storage<Service>>
>;

dingo::static_container<app_bindings> app_container;
```

- `container<>`, also spelled `runtime_container<>`, owns mutable runtime
  registrations added through `register_type<...>()`.
- `static_container<bindings<...>>` carries its bindings in the container type
  and has no runtime registration API.

The modes do not mix. A runtime container does not host compile-time bindings,
and a runtime registration cannot declare local `bindings<...>`; the
registration container returned by `register_type<...>()` accepts runtime
registrations only.

## Binding Semantics

A request for one object must select one binding. Several matching bindings make
the request ambiguous: the runtime container reports it with
`type_ambiguous_exception`, the static container rejects it at compile time.

A request for a collection gathers every matching binding.

Parent containers are a fallback source. `container<> child(&parent)` and
`static_container<bindings<...>, Parent>` keep the child graph local first: a
child binding wins for singular requests, and a missing child dependency is
resolved from the parent graph. A static container's parent is a static
container.

Local `bindings<...>` on a static binding declare a child graph for that
binding's dependencies. They shadow the host bindings: a request the local
bindings answer, singular or collection, does not reach the host.

## Direct Construction

`construct<T>()` reuses a registered binding for direct object construction when
that binding exists for `T`.

Wrapper construction such as `construct<std::shared_ptr<T>>()` also reuses the
registered binding path when Dingo can resolve the normalized object and wrap
the result. If no suitable registered path exists, construction falls back to
the normal constructor-based path.

## Cycle Rules

Static cycles are rejected at compile time unless every binding in the cycle
uses `scope<shared_cyclical>`. A supported static cycle uses the same two-phase
storage model as runtime `shared_cyclical` registration, so each participating
slot can be published before its dependencies finish resolving.

In static bindings, cycles through declared dependencies (`dependencies<...>`,
an explicit `constructor<T(Args...)>` or a function factory) are visible to
compile-time validation and are rejected at compile time. Cycles through
auto-detected constructors are not: detected constructors do not expose their
dependencies, so they are detected at runtime and reported with
`type_recursion_exception`. Runtime containers detect every cycle this way. A
binding skips the runtime check only when its factory provably has no
dependencies, such as a detected zero-argument constructor.

## Related Headers

The key implementation headers behind this model are:

- [include/dingo/container.h](../../include/dingo/container.h)
- [include/dingo/runtime/registry.h](../../include/dingo/runtime/registry.h)
- [include/dingo/static/registry.h](../../include/dingo/static/registry.h)
- [include/dingo/static/graph.h](../../include/dingo/static/graph.h)
- [include/dingo/binding/binding_selection.h](../../include/dingo/binding/binding_selection.h)
- [include/dingo/binding/binding_resolution.h](../../include/dingo/binding/binding_resolution.h)
- [include/dingo/binding/container_interface.h](../../include/dingo/binding/container_interface.h)
