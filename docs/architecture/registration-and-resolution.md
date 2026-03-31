# Registration And Resolution

This page follows the main runtime path through Dingo.

## Registration

The public API starts at `container::register_type<...>()` in
[include/dingo/container.h](../../include/dingo/container.h).

`register_type_impl(...)` does three important things:

1. It builds a complete `type_registration<...>` from the supplied policies.
1. It computes the stored type, including the interface-storage rewrite
   optimization for the single-interface virtual-destructor case.
1. It allocates one or more factories and inserts them into the container's
   factory map.

`type_registration<...>` in
[include/dingo/registration/type_registration.h](../../include/dingo/registration/type_registration.h)
deduces these pieces:

- scope
- storage
- factory
- interfaces
- conversions

That means a short registration such as `scope<shared>, storage<T>` is expanded
into the full internal policy set before the factory is created.

## Stored Type Versus Exposed Interface

Registration may expose a type through one or more interfaces. The stored type
does not always have to match the interface spelling exactly.

One notable optimization in
[include/dingo/container.h](../../include/dingo/container.h) rewrites the stored
leaf type to the interface leaf type when:

- there is exactly one exposed interface
- the interface has a virtual destructor
- the storage handle can be rebound to that interface

That keeps the stored object closer to the final interface shape and avoids some
temporary conversions.

## Resolution

The public `resolve<T>()` path is also in
[include/dingo/container.h](../../include/dingo/container.h).

The sequence is:

1. Check the cache if container caching is enabled.
1. Normalize the request type and look up the matching factory set.
1. Pick the single binding record or indexed binding record for the request.
1. Build a structural `resolution_plan`.
1. Lower that plan into a value-level runtime execution plan.
1. Ask the execution backend to execute that lowered plan.
1. Resolve through the selected factory and convert the result back to `T`
   through the shared conversion core in
   [include/dingo/resolution/type_conversion.h](../../include/dingo/resolution/type_conversion.h).

[include/dingo/resolution/runtime_execution_plan.h](../../include/dingo/resolution/runtime_execution_plan.h)
is the value-level backend step. It lowers the request portion of the structural
plan into a compact runtime object and executes that object against the factory API.

That lowered plan now carries:

- request opcode

- common-case conversion opcode

- selected binding metadata

- `T` -> `get_value`

- `T&` / `const T&` -> `get_lvalue_reference`

- `T&&` -> `get_rvalue_reference`

- `T*` -> `get_pointer`

This keeps the container itself from needing to understand every conversion
shape while still making the request semantics structural in
[include/dingo/resolution/ir.h](../../include/dingo/resolution/ir.h).

Concrete factories in
[include/dingo/resolution/instance_factory.h](../../include/dingo/resolution/instance_factory.h)
also expose `plan_for<T>()`, which carries the richer compile-time binding,
acquisition, invocation, and conversion structure.

The container registry now preserves a small binding record at lookup time, so
the runtime plan carries binding identity and basic metadata instead of only a
raw factory pointer.

## Factory Path

The factory implementation in
[include/dingo/resolution/instance_factory.h](../../include/dingo/resolution/instance_factory.h)
owns:

- a storage object
- a child container for constructor injection
- a resolver specialized for the stored type and scope

When the factory receives a request, it selects the appropriate list of possible
conversion shapes from `Storage::conversions` and tries to match the requested
lookup type against that list.

If a match is found, the resolver produces the source object and
[include/dingo/resolution/type_conversion.h](../../include/dingo/resolution/type_conversion.h)
converts it to the requested shape.

## Parent Containers And Auto Construction

If a factory is not found locally, the container can delegate to its parent.

If the requested type is unmanaged, unwrapped, and marked as auto-constructible,
the container can still build it directly through constructor deduction. That
behavior also lives in
[include/dingo/container.h](../../include/dingo/container.h).

The deduction logic itself lives in
[include/dingo/factory/constructor_detection.h](../../include/dingo/factory/constructor_detection.h).
Architecturally, the important limit is that Dingo picks the highest-arity
constructor shape it can prove constructible under its detection rules. It does
not try to model the full C++ overload-resolution space, which is why ambiguous,
policy-sensitive, or readability-critical cases should move to an explicit
factory.

## Good Source Companions

These files are useful while reading this flow:

- [include/dingo/container.h](../../include/dingo/container.h)
- [include/dingo/registration/type_registration.h](../../include/dingo/registration/type_registration.h)
- [include/dingo/resolution/instance_factory.h](../../include/dingo/resolution/instance_factory.h)
- [include/dingo/resolution/runtime_execution_plan.h](../../include/dingo/resolution/runtime_execution_plan.h)
- [include/dingo/resolution/type_conversion.h](../../include/dingo/resolution/type_conversion.h)
- [include/dingo/factory/constructor_detection.h](../../include/dingo/factory/constructor_detection.h)
