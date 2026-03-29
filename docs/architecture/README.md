# Architecture

This section documents Dingo's internal model: how registrations are reduced to
factories, how requests are matched to stored types, and where custom wrappers
and conversions plug in.

Read these pages in order if you want the full picture:

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

The architecture is centered on one flow:

```text
register_type -> type_registration -> storage/factory record
resolve<T> -> lookup type -> factory traits -> resolver -> conversion -> T
```

The code that owns that flow lives mainly in:

- [include/dingo/container.h](../../include/dingo/container.h)
- [include/dingo/type_registration.h](../../include/dingo/type_registration.h)
- [include/dingo/rebind_type.h](../../include/dingo/rebind_type.h)
- [include/dingo/type_conversion.h](../../include/dingo/type_conversion.h)
