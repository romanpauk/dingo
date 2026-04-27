# Overview

Dingo is easiest to understand when separated into four concerns:

- registration deduction
- lookup shape normalization
- storage and factory ownership
- conversion and wrapper adaptation

## Main Owners

### Public Containers

The public entry points are:

- [include/dingo/container.h](../../include/dingo/container.h): unified
  container
- [include/dingo/runtime_container.h](../../include/dingo/runtime_container.h):
  runtime-only facade
- [include/dingo/static_container.h](../../include/dingo/static_container.h):
  static-only facade

`container` is the primary public surface. It supports:

- `container<>` for runtime-only registrations
- `container<bindings<...>>` for hybrid static-plus-runtime usage

Under that surface, the runtime-backed path owns:

- the factory registry (`type_factories_`)
- the resolved-instance cache (`type_cache_`)
- parent lookup for nested containers
- the public registration and resolution entry points

The static-backed path owns:

- compile-time binding normalization
- static dependency graph validation
- compile-time binding selection and static resolution state

The container layer is the outer orchestration surface. It does not construct
objects directly. It stores or selects bindings and asks factories or static
resolution helpers for values, references, or pointers.

### Type Registration

[include/dingo/registration/type_registration.h](../../include/dingo/registration/type_registration.h)
reduces a `register_type<...>()` declaration into a complete policy set:

- `scope_type`
- `storage_type`
- `factory_type`
- `interface_type`
- `conversions_type`

That deduction step is the boundary between the public registration syntax and
the internal binding machinery.

For compile-time sources, the public spelling is `bindings<...>`. Those bindings
are normalized internally through the static registry path under
[include/dingo/static/registry.h](../../include/dingo/static/registry.h).

### Factory And Storage

[include/dingo/resolution/runtime_binding.h](../../include/dingo/resolution/runtime_binding.h)
wraps a storage object and the child container used to resolve constructor
dependencies.

The factory owns the conversion-facing interface:

- `get_value(instance_request)`
- `get_lvalue_reference(instance_request)`
- `get_rvalue_reference(instance_request)`
- `get_pointer(instance_request)`
- scope-specific materialization support

The storage classes under [include/dingo/storage/](../../include/dingo/storage)
own lifetime policy:

- `unique`: construct a fresh instance per resolution
- `shared`: cache a single instance
- `external`: refer to an existing object
- `shared_cyclical`: shared storage with cycle support

### Resolution And Conversion Path

[include/dingo/resolution/runtime_binding.h](../../include/dingo/resolution/runtime_binding.h)
now owns the conversion-facing resolution flow. It is responsible for:

- asking storage to `resolve(...)` its native source shape
- applying guard and closure scope through `storage_materialization_traits`
- constructing cached conversion objects for shared and external cases

[include/dingo/resolution/type_conversion.h](../../include/dingo/resolution/type_conversion.h)
contains the last-mile conversion rules from the stored source shape to the
requested target shape.

The shared runtime/static binding and resolution kernel now lives under
[include/dingo/core/](../../include/dingo/core):

- `binding_model.h`
- `binding_selection.h`
- `binding_resolution.h`
- `binding_resolution.h`

The lane-specific code sits under:

- [include/dingo/runtime/](../../include/dingo/runtime)
- [include/dingo/static/](../../include/dingo/static)

## Resolution Sketch

At a high level, Dingo runs this sequence:

1. `register_type<...>()` deduces the missing registration policies.
2. The runtime lane creates storage-backed factories for each exposed interface,
   or the static lane normalizes `bindings<...>` into a validated static source.
3. `resolve<T>()` converts `T` into the internal lookup form.
4. The container selects the matching runtime or static binding lane.
5. The selected lane builds an `instance_request` with the lookup type and
   requested descriptor, then calls the matching factory method for value,
   lvalue reference, rvalue reference, or pointer resolution.
6. The factory or static resolution path gets or builds the stored instance
   using its scope-specific state.
7. `type_conversion` converts that stored source shape into the requested `T`.

For hybrid `container<bindings<...>>`:

- singular runtime/static conflicts are ambiguous
- collection requests merge runtime and static results
- local `bindings<...>` overlays override host singular bindings and merge
  collections with the host container

The pieces above are not independent extension systems. They cooperate in one
pipeline.

## Reading The Code

For the core path in source order, start here:

- [include/dingo/container.h](../../include/dingo/container.h)
- [include/dingo/runtime_container.h](../../include/dingo/runtime_container.h)
- [include/dingo/static_container.h](../../include/dingo/static_container.h)
- [include/dingo/registration/type_registration.h](../../include/dingo/registration/type_registration.h)
- [include/dingo/runtime/registry.h](../../include/dingo/runtime/registry.h)
- [include/dingo/runtime/injector.h](../../include/dingo/runtime/injector.h)
- [include/dingo/static/registry.h](../../include/dingo/static/registry.h)
- [include/dingo/static/graph.h](../../include/dingo/static/graph.h)
- [include/dingo/static/injector.h](../../include/dingo/static/injector.h)
- [include/dingo/resolution/runtime_binding.h](../../include/dingo/resolution/runtime_binding.h)
- [include/dingo/resolution/type_conversion.h](../../include/dingo/resolution/type_conversion.h)
