# Architecture

This section documents the internal model of Dingo. It shows how a registration
becomes a binding, how a request selects a binding, and where custom wrappers
and conversions plug in.

Read these pages in this order:

- [Containers](containers.md): the public entry points, the include layout, and
  the runtime and static containers.
- [Overview](overview.md): the main data flow and the main owners.
- [Registration And Resolution](registration-and-resolution.md): runtime and
  compile-time registration, and the shared path to `resolve<T>()`.
- [Handles, Leaf Types, And Lookup](handles-leaf-types-and-lookup.md): wrapper
  algebra, `leaf_type_t`, `rebind_leaf_t`, `lookup_type_t`, and
  `exact_lookup<T>`.
- [Extending Dingo](extending-dingo.md): which trait to specialize for a new
  wrapper, storage scope, or conversion.
- [Conversion Model](conversion-model.md): how Dingo selects value, reference,
  pointer, and wrapper conversions at resolution time.

The public API has three container headers:

```text
include/dingo/runtime_container.h  -> runtime_container, also named container
include/dingo/static_container.h   -> static_container
include/dingo/container.h          -> both
```

Two registration modes feed one resolution flow:

```text
register_type / bindings -> type_registration -> binding_model -> binding cell
resolve<T> -> request_type -> binding selection -> binding activation -> conversion -> T
```

The shared binding and resolution code for this flow is in
`include/dingo/binding/`.

The code of each container is in its own directory:

- `include/dingo/runtime/`: the runtime registry
- `include/dingo/static/`: the static registry and the static graph

The main owners are:

- [include/dingo/container.h](../../include/dingo/container.h)
- [include/dingo/runtime_container.h](../../include/dingo/runtime_container.h)
- [include/dingo/static_container.h](../../include/dingo/static_container.h)
- [include/dingo/binding/binding_model.h](../../include/dingo/binding/binding_model.h)
- [include/dingo/binding/binding_resolution.h](../../include/dingo/binding/binding_resolution.h)
- [include/dingo/factory/factory_traits.h](../../include/dingo/factory/factory_traits.h)
- [include/dingo/runtime/registry.h](../../include/dingo/runtime/registry.h)
- [include/dingo/static/registry.h](../../include/dingo/static/registry.h)
- [include/dingo/static/graph.h](../../include/dingo/static/graph.h)
- [include/dingo/registration/type_registration.h](../../include/dingo/registration/type_registration.h)
- [include/dingo/type/shape.h](../../include/dingo/type/shape.h)
- [include/dingo/resolution/resolution_operation.h](../../include/dingo/resolution/resolution_operation.h)
