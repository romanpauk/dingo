# Registration And Resolution

This page follows the main runtime path through Dingo.

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
[include/dingo/type_registration.h](../../include/dingo/type_registration.h)
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
2. Normalize the request type and look up the matching factory set.
3. Pick the single factory or indexed factory for the request.
4. Ask `class_instance_factory_traits` how this request should be serviced.
5. Resolve through the selected factory and convert the result back to `T`.

[include/dingo/class_instance_factory_traits.h](../../include/dingo/class_instance_factory_traits.h)
is the small dispatch layer that maps the requested form to the factory API:

- `T` -> `get_value`
- `T&` / `const T&` -> `get_lvalue_reference`
- `T&&` -> `get_rvalue_reference`
- `T*` -> `get_pointer`

This keeps the container itself from needing to understand every conversion
shape.

## Factory Path

The factory implementation in
[include/dingo/class_instance_factory.h](../../include/dingo/class_instance_factory.h)
owns:

- a storage object
- a child container for constructor injection
- a resolver specialized for the stored type and scope

When the factory receives a request, it selects the appropriate list of possible
conversion shapes from `Storage::conversions` and tries to match the requested
lookup type against that list.

If a match is found, the resolver produces the source object and
[include/dingo/type_conversion.h](../../include/dingo/type_conversion.h)
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
- [include/dingo/type_registration.h](../../include/dingo/type_registration.h)
- [include/dingo/class_instance_factory.h](../../include/dingo/class_instance_factory.h)
- [include/dingo/class_instance_factory_traits.h](../../include/dingo/class_instance_factory_traits.h)
- [include/dingo/factory/constructor_detection.h](../../include/dingo/factory/constructor_detection.h)
