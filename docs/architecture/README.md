# Architecture

This section documents Dingo's internal model: how a registration becomes a
binding, how a request selects a binding and a route, and where custom wrappers,
wrapper semantics, scopes, conversions, and lookup indexes plug in.

Read these pages in order for the full picture:

- [Containers](containers.md): the public entry points, and how the combined,
  runtime-only, and static-only containers relate.
- [Overview](overview.md): the header layers, the main data flow, and the key
  owners.
- [Registration And Resolution](registration-and-resolution.md): runtime and
  compile-time registration, selection, and the path of `resolve<T>()`.
- [Handles, Leaf Types, And Lookup](handles-leaf-types-and-lookup.md): type
  trees, wrappers and leaves, `leaf_type_t`, `rebind_type_t`, and how a request
  finds its bindings.
- [Extending Dingo](extending-dingo.md): which trait to specialize for a new
  wrapper, wrapper semantics, scope, conversion, or lookup index.
- [Conversion Model](conversion-model.md): how Dingo derives the value,
  reference, pointer, and wrapper routes a binding serves.

The target architecture, and the tracking of its implementation, is described in
[Spec-Driven Design](spec-driven-design.md). The
[Test Migration Ledger](test-migration.md) records where each test of the
replaced implementation is tested now.

The public API is centered on three container entry points:

```text
include/dingo/container.h          -> combined public container
include/dingo/runtime_container.h  -> runtime-only spelling of the container
include/dingo/static_container.h   -> static-only container
```

These and the other public include paths under `include/dingo/` are the API of
the library, over its modules, in the header layers described in
[Header Layers](overview.md#header-layers).

Both registration modes reduce to one binding type, and both backends serve a
request through the same engine:

```text
register_type<...>() / bind<...> -> registration -> binding_type -> derived reachable types
resolve<T>() -> request -> selection -> backend -> reachable type -> walk -> T
```

A binding type derives the reachable types it serves when it is compiled. The
static backend selects the binding and the reachable type of a request when the
request is compiled, and the runtime backend when it is resolved, by the same
engine functions.

The main owners are:

- [include/dingo/container.h](../../include/dingo/container.h)
- [include/dingo/runtime_container.h](../../include/dingo/runtime_container.h)
- [include/dingo/static_container.h](../../include/dingo/static_container.h)
- [include/dingo/registration.h](../../include/dingo/registration.h)
- [include/dingo/conversion/route.h](../../include/dingo/conversion/route.h)
- [include/dingo/conversion/convert.h](../../include/dingo/conversion/convert.h)
- [include/dingo/backend/selection.h](../../include/dingo/backend/selection.h)
- [include/dingo/backend/static_backend.h](../../include/dingo/backend/static_backend.h)
- [include/dingo/backend/runtime_backend.h](../../include/dingo/backend/runtime_backend.h)
- [include/dingo/backend/slot.h](../../include/dingo/backend/slot.h)
- [include/dingo/conversion/wrapper_traits.h](../../include/dingo/conversion/wrapper_traits.h)
