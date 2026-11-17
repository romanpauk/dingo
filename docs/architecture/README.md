# Architecture

This section documents the internal model of Dingo. It shows how a registration
becomes a binding, how a request selects a binding, and where custom wrappers
and conversions plug in.

Read these pages in this order:

- [Containers](containers.md): the public entry points, the include layout, and
  the runtime and static containers.
- [Overview](overview.md): the main data flow and the main owners.
- [Shared Core](shared-core.md): what both containers share, and how a request
  goes from `resolve<T>()` to a binding and back in each.
- [Registration And Resolution](registration-and-resolution.md): runtime and
  compile-time registration, and the shared path to `resolve<T>()`.
- [Handles, Leaf Types, And Lookup](handles-leaf-types-and-lookup.md): wrapper
  algebra, `leaf_type_t`, `rebind_leaf_t` and binding lookup.
- [Extending Dingo](extending-dingo.md): which trait to specialize for a new
  wrapper or storage scope.
- [Conversion Model](conversion-model.md): how Dingo derives and applies value,
  reference, pointer, and wrapper conversions.
- [Layered Semantics](layered-semantics.md): the design that makes resolution
  correct by construction.

The public API has three container headers:

```text
include/dingo/runtime_container.h  -> runtime_container, also named container
include/dingo/static_container.h   -> static_container
include/dingo/container.h          -> both
```

Two registration modes feed one resolution flow:

```text
register_type / bindings -> type_registration -> binding_model -> binding cell
resolve<T> -> request_type -> binding selection -> derivation -> binding activation -> conversion -> T
```

Both containers share the code of this flow: the algorithm in
`include/dingo/resolution/`, the bindings in `include/dingo/binding/`, the
semantics in `include/dingo/semantics/` and the steps and conversions in
`include/dingo/request/execution.h`. They differ in how they select a binding
and how they run a conversion.

The code of each container is in its own directory:

- `include/dingo/runtime/`: the runtime container, its lookups and its engine
- `include/dingo/static/`: the static registry and the static graph

The main owners are:

- [include/dingo/container.h](../../include/dingo/container.h)
- [include/dingo/runtime_container.h](../../include/dingo/runtime_container.h)
- [include/dingo/static_container.h](../../include/dingo/static_container.h)
- [include/dingo/binding/model.h](../../include/dingo/binding/model.h)
- [include/dingo/binding/resolution.h](../../include/dingo/binding/resolution.h)
- [include/dingo/factory/factory_traits.h](../../include/dingo/factory/factory_traits.h)
- [include/dingo/runtime/container.h](../../include/dingo/runtime/container.h)
- [include/dingo/static/registry.h](../../include/dingo/static/registry.h)
- [include/dingo/static/graph.h](../../include/dingo/static/graph.h)
- [include/dingo/registration/type_registration.h](../../include/dingo/registration/type_registration.h)
- [include/dingo/type/shape.h](../../include/dingo/type/shape.h)
- [include/dingo/semantics/model.h](../../include/dingo/semantics/model.h)
- [include/dingo/request/execution.h](../../include/dingo/request/execution.h)
- [include/dingo/runtime/execution.h](../../include/dingo/runtime/execution.h)
- [include/dingo/runtime/request.h](../../include/dingo/runtime/request.h)
