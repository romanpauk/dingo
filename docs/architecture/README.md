# Architecture

This section documents Dingo's internal model: how registrations are reduced to
factories, how requests are matched to stored types, and where custom wrappers
and conversions plug in.

Read these pages in order for the full picture:

- [Containers](containers.md): the public entry points, include layout, and how
  runtime, static, and hybrid containers relate.
- [Overview](overview.md): the main data flow and the key owners.
- [Registration And Resolution](registration-and-resolution.md): the runtime
  path from `register_type<...>()` to `resolve<T>()`.
- [Handles, Leaf Types, And Lookup](handles-leaf-types-and-lookup.md): wrapper
  algebra, `leaf_type_t`, `rebind_leaf_t`, `lookup_type_t`, and
  `exact_lookup<T>`.
- [Extending Dingo](extending-dingo.md): which trait to specialize for a new
  wrapper, storage exposure policy, or conversion.
- [Conversion Model](conversion-model.md): how Dingo chooses value, reference,
  pointer, and wrapper conversions at resolution time.

The public surface is centered on three container entry points:

```text
include/dingo/container.h          -> unified public container
include/dingo/runtime_container.h  -> runtime-only facade
include/dingo/static_container.h   -> static-only facade
```

Under that surface, the architecture is centered on one flow:

```text
register_type -> type_registration -> storage/factory record
resolve<T> -> lookup type -> factory traits -> resolver -> conversion -> T
```

The shared kernel for that flow lives under `include/dingo/core/`.

The lane-specific code now sits under:

- `include/dingo/runtime/` for runtime registry and injector internals
- `include/dingo/static/` for static registry, graph, and injector internals

The main owners for the current architecture are:

- [include/dingo/container.h](../../include/dingo/container.h)
- [include/dingo/runtime_container.h](../../include/dingo/runtime_container.h)
- [include/dingo/static_container.h](../../include/dingo/static_container.h)
- [include/dingo/core/binding_model.h](../../include/dingo/core/binding_model.h)
- [include/dingo/core/binding_resolution.h](../../include/dingo/core/binding_resolution.h)
- [include/dingo/core/factory_traits.h](../../include/dingo/core/factory_traits.h)
- [include/dingo/runtime/registry.h](../../include/dingo/runtime/registry.h)
- [include/dingo/runtime/injector.h](../../include/dingo/runtime/injector.h)
- [include/dingo/static/registry.h](../../include/dingo/static/registry.h)
- [include/dingo/static/graph.h](../../include/dingo/static/graph.h)
- [include/dingo/static/injector.h](../../include/dingo/static/injector.h)
- [include/dingo/registration/type_registration.h](../../include/dingo/registration/type_registration.h)
- [include/dingo/type/rebind_type.h](../../include/dingo/type/rebind_type.h)
- [include/dingo/resolution/type_conversion.h](../../include/dingo/resolution/type_conversion.h)
