# Overview

Dingo is easiest to understand if you separate it into four concerns:

- registration deduction
- lookup shape normalization
- storage and factory ownership
- runtime conversion

## Main Owners

### Container

The container in [include/dingo/container.h](../../include/dingo/container.h)
owns:

- the factory registry (`type_factories_`)
- the resolved-instance cache (`type_cache_`)
- parent lookup for nested containers
- the public registration and resolution entry points

This is the outer orchestration layer. It does not construct objects directly.
It stores factories and asks them for values, references, or pointers.

### Type Registration

[include/dingo/registration/type_registration.h](../../include/dingo/registration/type_registration.h)
reduces a `register_type<...>()` declaration into a complete policy set:

- `scope_type`
- `storage_type`
- `factory_type`
- `interface_type`
- `conversions_type`

That deduction step is the boundary between the public registration syntax and
the internal runtime machinery.

### Factory And Storage

[include/dingo/resolution/instance_factory.h](../../include/dingo/resolution/instance_factory.h)
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

[include/dingo/resolution/instance_factory.h](../../include/dingo/resolution/instance_factory.h)
now owns the conversion-facing resolution flow. It is responsible for:

- asking storage to `resolve(...)` its native source shape
- applying guard and closure scope through `storage_materialization_traits`
- constructing cached conversion objects for shared and external cases

[include/dingo/resolution/type_conversion.h](../../include/dingo/resolution/type_conversion.h)
contains the last-mile conversion rules from the stored source shape to the
requested target shape.

## Runtime Sketch

At a high level, Dingo runs this sequence:

1. `register_type<...>()` deduces the missing registration policies.
2. The container creates a storage-backed factory for each exposed interface.
3. `resolve<T>()` converts `T` into the internal lookup form.
4. The container finds the matching factory.
5. The container builds an `instance_request` with the lookup type and requested
   descriptor, then calls the matching factory method for value, lvalue
   reference, rvalue reference, or pointer resolution.
6. The factory gets or builds the stored instance using its internal
   scope-specific state.
7. `type_conversion` converts that stored source shape into the requested `T`.

The pieces above are not independent extension systems. They cooperate in one
pipeline.

## Reading The Code

If you want to follow the core path in source order, start here:

- [include/dingo/container.h](../../include/dingo/container.h)
- [include/dingo/registration/type_registration.h](../../include/dingo/registration/type_registration.h)
- [include/dingo/resolution/instance_factory.h](../../include/dingo/resolution/instance_factory.h)
- [include/dingo/resolution/type_conversion.h](../../include/dingo/resolution/type_conversion.h)
