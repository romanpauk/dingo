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

## Internal Layout

The container stack is split into three layers:

- `include/dingo/core/`: shared runtime/static substrate
- `include/dingo/runtime/`: runtime registry internals
- `include/dingo/static/`: static registry and graph internals

The split keeps public headers small while leaving runtime and static
implementation details in their own directories.

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

Parent containers are another fallback source. `container<bindings<...>, Parent>`
and `static_container<bindings<...>, Parent>` keep the child graph local first:
a child static or runtime binding wins for singular requests, and a missing
child dependency can be resolved from the parent graph.

Local `bindings<...>` on a registration are different: they describe the private
dependencies used while building that registered type. For a single dependency,
the local binding is checked before the host container. For a dependency
collection, local matches and host matches are merged. If the local binding set
does not provide a dependency, resolution falls back to the host container.

## Direct Construction

`construct<T>()` reuses a registered binding for direct object construction when
that binding exists for `T`.

Wrapper construction such as `construct<std::shared_ptr<T>>()` also reuses the
registered/local-binding path when Dingo can resolve the normalized object and
wrap the result. If no suitable registered path exists, construction falls back
to the normal constructor-based path.

## Cycle Rules

Purely static cycles are rejected at compile time.

`container<bindings<...>>` also rejects statically known static cycles at
compile time.

Mixed runtime/static recursion that only becomes visible during actual runtime
resolution is rejected at runtime through the normal recursion exception path.

## Related Headers

The key implementation headers behind this model are:

- [include/dingo/container.h](../../include/dingo/container.h)
- [include/dingo/runtime/registry.h](../../include/dingo/runtime/registry.h)
- [include/dingo/static/registry.h](../../include/dingo/static/registry.h)
- [include/dingo/static/graph.h](../../include/dingo/static/graph.h)
- [include/dingo/core/binding_selection.h](../../include/dingo/core/binding_selection.h)
- [include/dingo/core/binding_resolution.h](../../include/dingo/core/binding_resolution.h)
- [include/dingo/core/binding_resolution_policy.h](../../include/dingo/core/binding_resolution_policy.h)
