# Containers

This page documents the current public container model.

## Public Entry Points

The top-level include set is intentionally small:

- [include/dingo/container.h](../../include/dingo/container.h): combined public
  container
- [include/dingo/runtime_container.h](../../include/dingo/runtime_container.h):
  runtime-only facade
- [include/dingo/static_container.h](../../include/dingo/static_container.h):
  static-only facade

Everything else is either runtime/static implementation detail or shared core.

The operations themselves are shared:
[include/dingo/container_operations.h](../../include/dingo/container_operations.h)
owns `resolve`, `construct`, `invoke`, the collection entries, and the
parent/miss hand-off for both registration modes.

## Internal Layout

The container stack is split into three groups:

- `include/dingo/binding/`: shared runtime/static substrate
- `include/dingo/runtime/`: runtime registry internals
- `include/dingo/static/`: static registry and graph internals

The public operations shared by the front-ends live in
`include/dingo/container_operations.h`. The front-ends are configurations plus
state: they provide their registries and answer whether they have a runtime
source, and the operations layer owns the decision order.

The split keeps public headers small while leaving runtime and static
implementation details in their own directories. The full header hierarchy is
described in [Header Layers](overview.md#header-layers).

## Unified Container

`container` is the primary public entry point.

It supports two main forms:

```c++
dingo::container<> runtime_only;

using app_bindings = dingo::bindings<
    dingo::bind<dingo::scope<dingo::shared>, dingo::storage<Config>>,
    dingo::bind<dingo::scope<dingo::unique>, dingo::storage<Service>>
>;

dingo::container<app_bindings> app_container;
```

The runtime-only form owns mutable runtime registrations.

The `bindings<...>` form hosts both:

- compile-time bindings from the `bindings<...>` source
- runtime registrations added later through `register_type<...>()`

`container<bindings<...>>` is the container form for mixed graphs: static
bindings are known when the container type is named, and runtime registrations
can still be added afterward.

Both forms share the operations layer in
[include/dingo/container_operations.h](../../include/dingo/container_operations.h).
The container type and whether it has a runtime source are the only
configuration inputs; resolving, constructing, invoking, and collecting follow
one order.

## Static And Runtime Facades

The split container facades remain available when the narrower shape is useful:

- `runtime_container` for explicitly runtime-only code
- `static_container` for explicitly static-only code

Those headers expose only one registration mode. `container` is the public
container that can represent runtime-only or mixed registration.

## Binding Semantics

`container<bindings<...>>` can have two binding sources: the static bindings in
the container type and the runtime registrations added to the object.

A request for one object must have one source. If only the runtime source
matches, Dingo uses it. If only the static source matches, Dingo uses it. If
both sources provide the same singular binding, the request is ambiguous because
neither source is an override of the other.

A request for a collection can use both sources. Runtime and static matches are
merged into the returned collection.

Parent containers are another fallback source.
`container<bindings<...>, Parent>` and `static_container<bindings<...>, Parent>`
keep the child graph local first: a child static or runtime binding wins for
singular requests, and a missing child dependency can be resolved from the
parent graph.

Local `bindings<...>` on a registration declare the static part of that
registration's child container. Runtime bindings added through the returned
proxy live in the same child. Singular resolution checks that child before the
root container, while collection resolution merges the child's static and
runtime matches with root matches.

## Direct Construction

`construct<T>()` reuses a registered binding for direct object construction when
that binding exists for `T`.

Wrapper construction such as `construct<std::shared_ptr<T>>()` also reuses the
registered/local-binding path when Dingo can resolve the normalized object and
wrap the result. If no suitable registered path exists, construction falls back
to the normal constructor-based path.

## Cycle Rules

Purely static cycles are rejected at compile time unless every binding in the
cycle uses `scope<shared_cyclical>`. A supported static cycle uses the same
two-phase storage model as runtime `shared_cyclical` registration, so each
participating slot can be published before its dependencies finish resolving.

`container<bindings<...>>` applies the same rule to statically known static
cycles: ordinary static cycles are rejected, while all-`shared_cyclical` static
cycles can resolve through the static binding path.

In static bindings, cycles through declared dependencies (`dependencies<...>`,
an explicit `constructor<T(Args...)>` or a function factory) are visible to
compile-time validation and are rejected at compile time. Cycles through
auto-detected constructors are not: detected constructors do not expose their
dependencies, so they are detected at runtime and reported with
`type_recursion_exception`. This applies to `static_container`,
`container<bindings<...>>` and runtime containers alike. A binding skips the
runtime check only when its factory provably has no dependencies, such as a
detected zero-argument constructor.

Mixed runtime/static recursion that only becomes visible during actual runtime
resolution is rejected at runtime through the normal recursion exception path.

## Related Headers

The key implementation headers behind this model are:

- [include/dingo/container.h](../../include/dingo/container.h)
- [include/dingo/container_operations.h](../../include/dingo/container_operations.h)
- [include/dingo/runtime/registry.h](../../include/dingo/runtime/registry.h)
- [include/dingo/static/registry.h](../../include/dingo/static/registry.h)
- [include/dingo/static/graph.h](../../include/dingo/static/graph.h)
- [include/dingo/binding/binding_selection.h](../../include/dingo/binding/binding_selection.h)
- [include/dingo/binding/binding_resolution.h](../../include/dingo/binding/binding_resolution.h)
- [include/dingo/binding/binding_resolution_policy.h](../../include/dingo/binding/binding_resolution_policy.h)
