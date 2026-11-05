# Registration And Resolution

This page follows the current registration and resolution path through Dingo.
Runtime registration and compile-time registration use different storage
backends, then meet at the shared binding selection and resolution model.

## Runtime Registration

The public API starts at `container::register_type<...>()` in
[include/dingo/container.h](../../include/dingo/container.h).

`prepare_binding(...)` does three important things:

1. It builds a complete `type_registration<...>` from the supplied policies.
2. It computes the stored type, including the interface-storage rewrite
   optimization for the single-interface virtual-destructor case.
3. It chooses the binding owner and lookup commit path for the normalized
   registration.

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

## Compile-Time Registration

`static_container<bindings<...>>` accepts a compile-time binding source:

```c++
using app_bindings = dingo::bindings<
    dingo::bind<dingo::scope<dingo::shared>, dingo::storage<Config>>,
    dingo::bind<dingo::scope<dingo::unique>, dingo::storage<Service>>
>;

dingo::static_container<app_bindings> container;
```

That path is normalized through
[include/dingo/static/registry.h](../../include/dingo/static/registry.h) and
validated by [include/dingo/static/graph.h](../../include/dingo/static/graph.h).

There are two registration modes:

- runtime registration through `register_type<...>()` on `container<>`
- compile-time registration through `bindings<...>` on `static_container`

A container uses one of them; the modes do not mix.

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
[include/dingo/container.h](../../include/dingo/container.h). At that boundary
Dingo normalizes two independent request descriptions:

- The type request becomes `request_type<T, RemoveRvalueReferences>`.
- The key request becomes the result of `make_lookup_key(...)`.

`request_type` is a zero-size compile-time descriptor. Internal resolution code
uses its `lookup_type`, `interface_type`, `result_type`, `value_type`, and
`exact_type` members instead of recomputing request-derived type aliases along
each path.

Both containers resolve a request with the algorithm in
[include/dingo/binding/request_resolution.h](../../include/dingo/binding/request_resolution.h).
The sequence is:

1. The runtime container checks the request cache of the selected binding.
2. The container selects the bindings of the normalized type request and lookup
   key. A static selection is a type, so its status is known at compile time; a
   runtime selection is a value.
3. The algorithm continues by status:
   - one binding: resolve it
   - several bindings: ambiguous, a `type_ambiguous_exception` at run time or a
     compile-time error in a static container
   - no binding: the parent container, if it declares the lookup, and otherwise
     the container the request entered through, which may construct the type
4. A collection request without a key resolves a binding of the collection type
   first. Otherwise the collection is constructed from the bindings of its
   element, and a container without any falls back as in step 3.
5. A selected binding applies its resolution through a binding activation. A
   static container selects the resolution at compile time; a runtime binding
   matches the requested type descriptor in its `instance_request`.

[include/dingo/binding/runtime_binding_interface.h](../../include/dingo/binding/runtime_binding_interface.h)
defines the request object the runtime container passes to a binding. The object
stores:

- the lookup type index
- the requested type descriptor

The containers differ in their binding table and their selection, not in the
algorithm: registration child containers, the runtime container and the static
container all provide the same hooks to it.

### Atomic Container Changes

Public registration and resolution operations run in a transaction. If an
operation succeeds, its container changes are committed together. If an
exception escapes, Dingo restores the container to its state at the start of the
operation. Rollback removes registrations, cache and lookup entries, and other
pointers installed by the operation. It also destroys newly constructed
container-owned objects in reverse construction order before reclaiming their
arena storage.

Recursive registration and resolution share the enclosing transaction. A nested
operation may finish successfully, but its changes remain provisional until the
outermost operation commits. Consequently, a later failure in the same operation
also rolls back those nested changes. The registration-created child containers
used for constructor injection share their root container's runtime domain and
transaction. Independently constructed public containers, including explicit
parent/child container objects, keep separate runtime domains and transactions.

The atomicity boundary covers container-managed state only. Side effects that
user constructors, factories, callbacks, or destructors make outside the
container cannot be rolled back.

## Factory Path

The binding state in
[include/dingo/runtime/runtime_binding.h](../../include/dingo/runtime/runtime_binding.h)
owns:

- a binding cell with the storage object and its cached conversions
- a lightweight child-container view for constructor injection

Materialization goes through the binding activation that the static container
uses as well.

The root container owns the runtime arena and a dense table of lookup states for
those child views. A child stores only its root pointer and scope ID. Resolution
uses the ID to select the child's state in constant time, then uses the original
lookup backend and logical key. A child's lookup state is allocated only when it
first registers a local binding, avoiding a complete runtime arena and lookup
set in every registration-created container.

When the factory receives one of those calls, it selects the appropriate list of
possible conversion shapes from `Storage::conversions` and tries to match the
requested lookup type against that list.

If a match is found, the factory produces the source object and
[include/dingo/resolution/resolution_operation.h](../../include/dingo/resolution/resolution_operation.h)
converts it to the requested shape.

The static path uses the same binding model and conversion rules, but it selects
from a validated compile-time binding source instead of mutable runtime binding
state.

## Registration Child Bindings

`register_type<...>()` returns a proxy to its registration-created child
container, which accepts further runtime registrations. A static `bind<...>` can
carry local `bindings<...>` that play the same role for compile-time bindings.

Dependencies are selected with the following rules:

- local-only singular match: selected
- local and host both provide the same singular binding: local wins
- host-only singular match: selected
- collection request: merge local and host results

## Parent Containers And Auto Construction

If a factory is not found locally, the container can delegate to its parent.

If the requested type is unmanaged, unwrapped, and marked as auto-constructible,
the container can still build it directly through constructor deduction. That
behavior also lives in
[include/dingo/container.h](../../include/dingo/container.h).

The deduction logic itself lives in
[include/dingo/factory/constructor_detection.h](../../include/dingo/factory/constructor_detection.h).
Constructor detection first selects a shape: its constructor kind and the
highest arity Dingo can prove constructible. The default construction path uses
that result directly and supplies conversion-based arguments at the selected
arity.

When compile-time dependency information is needed, signature detection reuses
the selected shape and recovers the concrete parameter types. Its construction
path consumes that recovered type list, making it the single source of truth for
both dependency reporting and injection. Signature recovery does not choose a
different overload or expand the search range.

Dingo does not try to model the full C++ overload-resolution space, which is why
ambiguous, policy-sensitive, or readability-critical cases should move to an
explicit factory.

`static_container` checks statically known cycles at compile time. Ordinary
static cycles are rejected; cycles where every participating binding uses
`scope<shared_cyclical>` are allowed through the static binding path. Recursion
that only appears during actual construction is rejected at runtime by the
recursion guard.

## Good Source Companions

These files are useful while reading this flow:

- [include/dingo/container.h](../../include/dingo/container.h)
- [include/dingo/registration/type_registration.h](../../include/dingo/registration/type_registration.h)
- [include/dingo/runtime/registry.h](../../include/dingo/runtime/registry.h)
- [include/dingo/static/registry.h](../../include/dingo/static/registry.h)
- [include/dingo/static/graph.h](../../include/dingo/static/graph.h)
- [include/dingo/runtime/runtime_binding.h](../../include/dingo/runtime/runtime_binding.h)
- [include/dingo/binding/runtime_binding_interface.h](../../include/dingo/binding/runtime_binding_interface.h)
- [include/dingo/factory/constructor_detection.h](../../include/dingo/factory/constructor_detection.h)
