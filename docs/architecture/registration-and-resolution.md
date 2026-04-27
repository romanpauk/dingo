# Registration And Resolution

This page follows the current registration and resolution path through Dingo.
The main mechanics are still runtime-backed, but the unified `container` surface
can also host compile-time `bindings<...>` and local binding overlays.

## Registration

The public API starts at `container::register_type<...>()` in
[include/dingo/container.h](../../include/dingo/container.h).

`register_type_impl(...)` does three important things:

1. It builds a complete `type_registration<...>` from the supplied policies.
2. It computes the stored type, including the interface-storage rewrite
   optimization for the single-interface virtual-destructor case.
3. It allocates one or more factories and inserts them into the container's
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

The unified container also accepts a compile-time binding source:

```c++
using app_bindings = dingo::bindings<
    dingo::bind<dingo::scope<dingo::shared>, dingo::storage<Config>>,
    dingo::bind<dingo::scope<dingo::unique>, dingo::storage<Service>>
>;

dingo::container<app_bindings> container;
```

That path is normalized through
[include/dingo/static/registry.h](../../include/dingo/static/registry.h) and
validated by [include/dingo/static/graph.h](../../include/dingo/static/graph.h)
/ [include/dingo/static/injector.h](../../include/dingo/static/injector.h).

So there are two registration lanes:

- runtime registrations added through `register_type<...>()`
- compile-time bindings supplied through `bindings<...>`

The unified `container` can host both at once.

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
2. Normalize the request type and look up the matching runtime and/or static
   binding set.
3. Apply the container's selection rules:
   - runtime-only singular match: selected
   - static-only singular match: selected
   - runtime and static both provide the same singular binding: ambiguous
   - collection request: merge both lanes
4. Build an `instance_request` for the requested lookup type and descriptor.
5. Call the matching runtime factory entry point or static resolution path for
   the requested form and convert the result back to `T`.

[include/dingo/resolution/runtime_binding_interface.h](../../include/dingo/resolution/runtime_binding_interface.h)
defines the request object the container passes to the factory. The object
stores:

- the lookup type index
- the requested type descriptor

The container then chooses the appropriate factory entry point:

- `get_value(...)`
- `get_lvalue_reference(...)`
- `get_rvalue_reference(...)`
- `get_pointer(...)`

The runtime and static lanes share the same core selection and per-binding
resolution vocabulary under [include/dingo/core/](../../include/dingo/core),
even though their registration/storage backends differ.

## Factory Path

The factory implementation in
[include/dingo/resolution/runtime_binding.h](../../include/dingo/resolution/runtime_binding.h)
owns:

- a storage object
- a child container for constructor injection
- scope-specific materialization and cached-conversion state

When the factory receives one of those calls, it selects the appropriate list of
possible conversion shapes from `Storage::conversions` and tries to match the
requested lookup type against that list.

If a match is found, the factory produces the source object and
[include/dingo/resolution/type_conversion.h](../../include/dingo/resolution/type_conversion.h)
converts it to the requested shape.

The static path uses the same binding model and conversion rules, but it selects
from a validated compile-time binding source instead of a mutable runtime
factory map.

## Local `bindings<...>` Overlays

`register_type<...>()` can also carry local `bindings<...>` used while
constructing that registered binding.

Those local bindings are resolved before falling back to the host container, but
they do get a private singular override rule:

- local-only singular match: selected
- local and host both provide the same singular binding: local wins
- host-only singular match: selected
- collection request: merge local and host results

That keeps collection behavior aligned with hybrid resolution while still
letting local overlays replace host singular dependencies.

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

Hybrid containers also apply compile-time cycle rejection to statically known
static cycles, while mixed runtime/static recursion that only appears during
actual construction is still rejected at runtime by the recursion guard.

## Good Source Companions

These files are useful while reading this flow:

- [include/dingo/container.h](../../include/dingo/container.h)
- [include/dingo/registration/type_registration.h](../../include/dingo/registration/type_registration.h)
- [include/dingo/runtime/registry.h](../../include/dingo/runtime/registry.h)
- [include/dingo/runtime/injector.h](../../include/dingo/runtime/injector.h)
- [include/dingo/static/registry.h](../../include/dingo/static/registry.h)
- [include/dingo/static/graph.h](../../include/dingo/static/graph.h)
- [include/dingo/static/injector.h](../../include/dingo/static/injector.h)
- [include/dingo/resolution/runtime_binding.h](../../include/dingo/resolution/runtime_binding.h)
- [include/dingo/resolution/runtime_binding_interface.h](../../include/dingo/resolution/runtime_binding_interface.h)
- [include/dingo/factory/constructor_detection.h](../../include/dingo/factory/constructor_detection.h)
