# Architecture

This section documents Dingo's internal model: how registrations are reduced to
factories, how requests are matched to stored types, and where custom wrappers
and conversions plug in.

Read these pages in order for the full picture:

- [Containers](containers.md): the public entry points, include layout, and how
  combined, runtime-only, and static-only containers relate.
- [Overview](overview.md): the main data flow and the key owners.
- [Registration And Resolution](registration-and-resolution.md): runtime and
  compile-time registration, and the shared path to `resolve<T>()`.
- [Shared Kernel](shared-kernel.md): the proposed unification of runtime and
  compile-time selection and resolution, and the generated coverage model.
- [Container Engine](container-engine.md): the decision-function design note for
  making the static and mixed front-ends configurations of one operations layer.
- [Handles, Leaf Types, And Lookup](handles-leaf-types-and-lookup.md): wrapper
  algebra, `leaf_type_t`, `rebind_leaf_t`, `lookup_type_t`, and
  `exact_lookup<T>`.
- [Extending Dingo](extending-dingo.md): which trait to specialize for a new
  wrapper, storage exposure policy, or conversion.
- [Conversion Model](conversion-model.md): how Dingo chooses value, reference,
  pointer, and wrapper conversions at resolution time.
- [Conversion Path](conversion-path.md): the design note for deriving route
  properties once from the selected conversion instead of re-deriving them in
  storage pairing.
- [Coverage Model](coverage-model.md): capability rows, the claims over them,
  operator classes, and what completeness means.
- [Test Corpus Retirement](retired-corpus.md): what replaced the retired the
  generated test corpus corpus, feature by feature.
- [Complexity Review](complexity-review.md): a maintenance report on dead
  surface, forwarding layers, and rules that are encoded more than once, with a
  proposed order for removing them.

The public API is centered on three container entry points:

```text
include/dingo/container.h          -> combined public container
include/dingo/runtime_container.h  -> runtime-only facade
include/dingo/static_container.h   -> static-only facade
```

Below those headers, the architecture is centered on one resolution flow fed by
two registration modes:

```text
register_type / bindings -> type_registration -> storage/factory record
resolve<T> -> lookup type -> factory traits -> resolver -> conversion -> T
```

The shared binding and resolution kernel for that flow lives under
`include/dingo/binding/`. The public operations shared by the container
front-ends live in `include/dingo/container_operations.h`.

Runtime and static implementation code sits under:

- `include/dingo/runtime/` for runtime registry internals
- `include/dingo/static/` for static registry and graph internals

The main owners for the current architecture are:

- [include/dingo/container.h](../../include/dingo/container.h)
- [include/dingo/container_operations.h](../../include/dingo/container_operations.h)
- [include/dingo/runtime_container.h](../../include/dingo/runtime_container.h)
- [include/dingo/static_container.h](../../include/dingo/static_container.h)
- [include/dingo/binding/binding_model.h](../../include/dingo/binding/binding_model.h)
- [include/dingo/binding/binding_resolution.h](../../include/dingo/binding/binding_resolution.h)
- [include/dingo/factory/factory_traits.h](../../include/dingo/factory/factory_traits.h)
- [include/dingo/runtime/registry.h](../../include/dingo/runtime/registry.h)
- [include/dingo/static/registry.h](../../include/dingo/static/registry.h)
- [include/dingo/static/graph.h](../../include/dingo/static/graph.h)
- [include/dingo/registration/type_registration.h](../../include/dingo/registration/type_registration.h)
- [include/dingo/type/rebind_type.h](../../include/dingo/type/rebind_type.h)
- [include/dingo/resolution/resolution_operation.h](../../include/dingo/resolution/resolution_operation.h)
