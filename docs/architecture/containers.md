# Containers

This page documents the current public container model.

## Public Entry Points

The top-level include surface is intentionally small:

- [include/dingo/container.h](../../include/dingo/container.h): unified public
  container
- [include/dingo/runtime_container.h](../../include/dingo/runtime_container.h):
  runtime-only facade
- [include/dingo/static_container.h](../../include/dingo/static_container.h):
  static-only facade

Everything else is either lane-specific implementation detail or shared core.

## Internal Layout

The container stack is split into three layers:

- `include/dingo/core/`: shared runtime/static substrate
- `include/dingo/runtime/`: runtime registry and injector internals
- `include/dingo/static/`: static registry, graph, and injector internals

That gives Dingo one public story at the top and narrower implementation
surfaces underneath it.

## Unified Container

`container` is the primary public entry point.

It supports two main forms:

```c++
dingo::container<> runtime_only;

using app_bindings = dingo::bindings<
    dingo::bind<dingo::scope<dingo::shared>, dingo::storage<Config>>,
    dingo::bind<dingo::scope<dingo::unique>, dingo::storage<Service>>
>;

dingo::container<app_bindings> hybrid;
```

The runtime-only form owns mutable runtime registrations.

The `bindings<...>` form hosts both:

- compile-time bindings from the `bindings<...>` source
- runtime registrations added later through `register_type<...>()`

That makes `container<bindings<...>>` the hybrid entry point.

## Static And Runtime Facades

The split container facades remain available when the narrower shape is useful:

- `runtime_container` for explicitly runtime-only code
- `static_container` for explicitly static-only code

Those are lane-specific surfaces. `container` is the unified public front door.

## Binding Semantics

For singular resolution in a hybrid container:

- runtime-only match: selected
- static-only match: selected
- runtime and static both provide the same singular binding: ambiguous

For collection resolution in a hybrid container:

- runtime and static results are merged

For local `bindings<...>` attached to a registered binding:

- singular requests prefer local bindings over host bindings
- collection requests merge local and host results
- missing local dependencies fall back to the host container

## Direct Construction

`construct<T>()` reuses a registered binding for direct object construction when
that binding exists for `T`.

Wrapper construction such as `construct<std::shared_ptr<T>>()` also reuses the
registered/local-binding path when Dingo can resolve the normalized object and
wrap the result. If no suitable registered path exists, construction falls back
to the normal constructor-based path.

## Cycle Rules

Purely static cycles are rejected at compile time.

Hybrid containers also reject statically known static cycles at compile time.

Mixed runtime/static recursion that only becomes visible during actual runtime
resolution is rejected at runtime through the normal recursion exception path.

## Related Headers

The key implementation headers behind this model are:

- [include/dingo/container.h](../../include/dingo/container.h)
- [include/dingo/runtime/registry.h](../../include/dingo/runtime/registry.h)
- [include/dingo/runtime/injector.h](../../include/dingo/runtime/injector.h)
- [include/dingo/static/registry.h](../../include/dingo/static/registry.h)
- [include/dingo/static/graph.h](../../include/dingo/static/graph.h)
- [include/dingo/static/injector.h](../../include/dingo/static/injector.h)
- [include/dingo/core/binding_selection.h](../../include/dingo/core/binding_selection.h)
- [include/dingo/core/binding_resolution.h](../../include/dingo/core/binding_resolution.h)
- [include/dingo/core/binding_resolution_status.h](../../include/dingo/core/binding_resolution_status.h)
