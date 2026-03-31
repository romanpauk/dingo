# Overview

Dingo is easiest to understand if you separate it into five concerns:

- registration deduction
- lookup shape normalization
- resolution IR
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
It stores binding records and asks the selected binding's factory for values,
references, or pointers.

### Type Registration

[include/dingo/registration/type_registration.h](../../include/dingo/registration/type_registration.h)
reduces a `register_type<...>()` declaration into a complete policy set:

- `scope_type`
- `storage_type`
- `factory_type`
- `interface_type`
- `conversions_type`

That deduction step is the boundary between the public registration syntax and
the internal resolution machinery.

### Resolution IR

[include/dingo/resolution/ir.h](../../include/dingo/resolution/ir.h) is the
structural layer between the public/container-facing API and the execution
machinery.

It models:

- request shape
- selected binding
- acquisition policy
- invocation intent
- conversion intent

The IR is compile-time metadata only. Execution still happens in the existing
factory, resolver, and conversion code.

### Factory And Storage

[include/dingo/resolution/instance_factory.h](../../include/dingo/resolution/instance_factory.h)
wraps a storage object and the child container used to resolve constructor
dependencies.

The factory owns the conversion-facing interface:

- `get_value`
- `get_lvalue_reference`
- `get_rvalue_reference`
- `get_pointer`

The storage classes under [include/dingo/storage/](../../include/dingo/storage)
own lifetime policy:

- `unique`: construct a fresh instance per resolution
- `shared`: cache a single instance
- `external`: refer to an existing object
- `shared_cyclical`: shared storage with cycle support

### Resolver And Conversion Path

[include/dingo/resolution/instance_resolver.h](../../include/dingo/resolution/instance_resolver.h)
bridges storage and resolution. It is responsible for:

- driving the storage's `resolve(...)`
- preserving temporaries in the resolving context when needed
- constructing cached conversion objects for shared and external cases

[include/dingo/resolution/type_conversion.h](../../include/dingo/resolution/type_conversion.h)
contains the last-mile conversion rules from the stored source shape to the
requested target shape.

## Runtime Sketch

At a high level, Dingo runs this sequence:

1. `register_type<...>()` deduces the missing registration policies.
1. The container creates a storage-backed factory for each exposed interface.
1. `resolve<T>()` converts `T` into the internal lookup form.
1. The container finds the matching factory.
1. The request is reduced to structural resolution IR.
1. The runtime path lowers that IR into a compact execution plan.
1. The execution backend uses that plan to call the factory access path for `T`.
1. The factory asks its resolver to get or build the stored instance.
1. The shared conversion core in `type_conversion` converts that stored source
   shape into the requested `T`.

The pieces above are not independent extension systems. They cooperate in one
pipeline.

## Reading The Code

If you want to follow the core path in source order, start here:

- [include/dingo/container.h](../../include/dingo/container.h)
- [include/dingo/registration/type_registration.h](../../include/dingo/registration/type_registration.h)
- [include/dingo/resolution/ir.h](../../include/dingo/resolution/ir.h)
- [include/dingo/resolution/instance_factory.h](../../include/dingo/resolution/instance_factory.h)
- [include/dingo/resolution/instance_resolver.h](../../include/dingo/resolution/instance_resolver.h)
- [include/dingo/resolution/type_conversion.h](../../include/dingo/resolution/type_conversion.h)
