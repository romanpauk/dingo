# Registration And Resolution

This page follows the registration and resolution path. Runtime and compile-time
registration use different storage backends. Both then use the shared binding
selection and resolution model.

## Runtime Registration

The public API starts at `container::register_type<...>()`. It is a member of
the runtime registry in
[include/dingo/runtime/registry.h](../../include/dingo/runtime/registry.h).

`prepare_binding(...)` does three things:

1. It builds a complete `type_registration<...>` from the supplied policies.
2. It computes the stored type through `binding_model`. This includes the
   interface storage rewrite for one interface with a virtual destructor.
3. It selects the binding owner and the lookup commit path for the normalized
   registration.

`type_registration<...>` in
[include/dingo/registration/type_registration.h](../../include/dingo/registration/type_registration.h)
deduces these parts:

- scope
- storage
- factory
- interfaces
- conversions

A short registration such as `scope<shared>, storage<T>` expands to the full
policy set before Dingo creates the factory.

## Compile-Time Registration

`static_container<bindings<...>>` accepts a compile-time binding source:

```c++
using app_bindings = dingo::bindings<
    dingo::bind<dingo::scope<dingo::shared>, dingo::storage<Config>>,
    dingo::bind<dingo::scope<dingo::unique>, dingo::storage<Service>>
>;

dingo::static_container<app_bindings> container;
```

[include/dingo/static/registry.h](../../include/dingo/static/registry.h)
normalizes this source.
[include/dingo/static/graph.h](../../include/dingo/static/graph.h) validates it.
A container uses one registration mode. The modes do not mix.

## Stored Type Versus Exposed Interface

A registration can expose a type through one or more interfaces. The stored type
does not always match the interface.

[include/dingo/binding/model.h](../../include/dingo/binding/model.h) rewrites
the stored leaf type to the interface leaf type when all of these conditions are
true:

- the registration exposes exactly one interface
- the interface is a class with a virtual destructor
- the storage is a pointer-like handle that can be rebound to the interface

The rule is `use_interface_as_stored_leaf_v` in
`storage/interface_storage_traits.h`. The rewrite keeps the stored object close
to the interface shape and avoids temporary conversions.

## Resolution

Both containers derive the public `resolve<T>()` from `container_requests` in
`resolution/entry_points.h`. It calls `resolve_entry` of the container, for
example in
[include/dingo/runtime/container.h](../../include/dingo/runtime/container.h). At
this boundary Dingo normalizes two independent request descriptions:

- The type request becomes `request_type<T>`.
- The key request becomes the result of `make_lookup_key(...)`.

`request_type` is a zero-size compile-time descriptor. Internal resolution code
uses its `user_type`, `lookup_type`, `interface_type`, `value_type`, and
`exact_type` members. It does not compute these aliases again on each path.

Both containers resolve a request with the algorithm in
[include/dingo/resolution/algorithm.h](../../include/dingo/resolution/algorithm.h).
The sequence is:

1. For a reference or pointer request, the runtime container first reads the
   request cache of the selected binding.
2. The container selects the bindings of the type request and the lookup key. A
   static selection is a type, so its status is known at compile time. A runtime
   selection is a value.
3. The algorithm continues by the status of the selection:
   - one binding: it resolves the binding
   - several bindings: the request is ambiguous; a runtime container throws
     `type_ambiguous_exception`, a static container fails to compile
   - no binding: the parent container answers if it declares the lookup;
     otherwise the container that the request entered through answers, and it
     can construct the type
4. A collection request without a key first resolves a binding of the collection
   type. If there is none, Dingo constructs the collection from the bindings of
   its element. A container without such bindings falls back as in step 3.
5. The selected binding applies its resolution through a binding activation. A
   static container selects the resolution at compile time. A runtime binding
   matches the requested type descriptor in its `instance_request`.
6. If the binding does not publish the request, Dingo derives it from a request
   that the binding resolves. Both containers apply the rules in
   [include/dingo/request/derivation.h](../../include/dingo/request/derivation.h).

The derivation rules are:

| Rule    | Request                                                                | Resolved as                                    |
| ------- | ---------------------------------------------------------------------- | ---------------------------------------------- |
| adopt   | a reference to the object that a pointer made by the binding points to | the pointer; an owner in the context adopts it |
| keep    | a reference or pointer to a wrapper that shares what the binding keeps | the wrapper; the binding keeps it              |
| place   | a reference or pointer to a value made for the request                 | the value; the context holds it                |
| take    | a value of the object that a pointer made by the binding points to     | the pointer; the value is moved out            |
| compose | a wrapper                                                              | the value that the wrapper holds               |

The same binding resolves the derived request, which can be derived again. A
wrapper of wrappers is composed at once, of the first value down its layers that
the binding resolves by another rule. Dingo moves that value once. The caller
cannot get a reference into the context.

[include/dingo/binding/interface.h](../../include/dingo/binding/interface.h)
defines `instance_request`. The runtime container passes this object to a
binding. The object stores:

- the requested type descriptor
- the derivation of the request; the binding calls it only for a request that it
  does not publish, and passes facts about itself (see `derivation_site`)
- the node of the request, which the derivation uses

The node of a request (`request_node` in
[include/dingo/binding/resolution.h](../../include/dingo/binding/resolution.h))
is constant data. It holds the descriptor of the request, the derivations whose
shape it has, the operations of the object that it is or refers to, and the
nodes of the requests that each derivation resolves through. One function
derives a value, a reference or a pointer of the node: it chooses the rule,
resolves the node of the derived request through the binding, and places a value
or an owner in the context with the operations of the object. A request type
adds only its node and the operations of its object to a program, not a function
for each of its derivations. A derivation of a wrapper (keep, convert, compose)
is still a function of the type of the request. The static container applies the
rules of the type, as it compiles.

The containers differ in their binding table and their selection, not in the
algorithm. Both containers give the algorithm the same hooks. A registration
container of local bindings is a runtime container, and gives it one more: the
container it falls back to before any parent.

### Atomic Container Changes

Public registration and resolution operations run in a transaction. If an
operation succeeds, Dingo commits all its container changes together. If an
exception escapes, Dingo restores the container to its state at the start of the
operation. Rollback removes the registrations, cache entries, lookup entries,
and other pointers that the operation installed. It also destroys the
container-owned objects that the operation constructed, in reverse order of
construction. Then it releases their arena storage.

Recursive registration and resolution share the enclosing transaction. The
changes of a nested operation stay provisional until the outermost operation
commits. A later failure in the same operation rolls them back too. The child
containers that registration creates share the runtime domain and the
transaction of their root container. Independent public containers, also
explicit parent and child containers, have separate runtime domains and
transactions.

The atomicity covers only container-managed state. Dingo cannot roll back side
effects that user constructors, factories, callbacks, or destructors make
outside the container.

## Factory Path

The binding state in
[include/dingo/runtime/binding.h](../../include/dingo/runtime/binding.h) owns:

- a binding cell with the storage object and its retained conversions
- the registration container of its local bindings, created when the first local
  binding is registered

The runtime binding materializes its instance through the same binding
activation as the static container. Its factory resolves its dependencies
through the registration container, when there is one, and else through the
container the registration is made in. Both have the same type, so the factory
and the activation are instantiated once for a registration.

The root container owns the runtime arena and a dense table of lookup states for
the registration containers. A registration container is a runtime container
that does not own runtime data. In place of the data, it stores its scope ID and
the container it falls back to, and it reaches the data through its root.
Resolution uses the ID to find the state of the registration container in
constant time, then uses the original lookup backend and logical key. A
registration container allocates its lookup state only when it registers its
first local binding. Thus it needs no runtime arena and lookup set of its own.

The runtime binding has a list of resolutions, derived from
`Storage::conversions`. It matches the requested type descriptor against this
list. If a resolution matches, the storage produces the source object.
[include/dingo/conversion/apply.h](../../include/dingo/conversion/apply.h)
converts it to the requested shape.

The static path uses the same binding model and conversion rules. It selects
from a validated compile-time binding source, not from mutable runtime binding
state.

## Registration Child Bindings

`register_type<...>()` returns a proxy to the registration container of the
registration, which accepts more runtime registrations. A request it binds no
binding for falls back to the container the registration is made in, and then to
the parent of that container. Local `bindings<...>` on a static `bind<...>` have
the same role for compile-time bindings.

Dependencies are selected by these rules:

| Case                                                    | Selected                                                       |
| ------------------------------------------------------- | -------------------------------------------------------------- |
| only the local graph has a singular match               | the local binding                                              |
| the local graph and the host both have a singular match | the local binding                                              |
| only the host has a singular match                      | the host binding                                               |
| collection request                                      | the local entries if there are any, otherwise the host entries |

## Parent Containers And Auto Construction

If no binding is found locally, the container can delegate the request to its
parent.

If no container binds an unwrapped, auto-constructible type, Dingo constructs it
through constructor deduction. `request_resolution::unresolved` in
[include/dingo/resolution/algorithm.h](../../include/dingo/resolution/algorithm.h)
does this.

[include/dingo/factory/constructor_detection.h](../../include/dingo/factory/constructor_detection.h)
contains the deduction logic. Constructor detection first selects a shape: the
constructor kind and the highest arity that Dingo can prove constructible. The
default construction path uses this result directly. It supplies
conversion-based arguments at the selected arity.

Signature detection gives compile-time dependency information. It recovers the
concrete parameter types of the selected shape, and its construction path uses
this type list. Thus one type list drives both dependency reporting and
injection. Signature recovery does not select another overload or extend the
search range.

Dingo does not model the full C++ overload resolution. For ambiguous,
policy-sensitive, or readability-critical cases, use an explicit factory.

[Cycle Rules](containers.md#cycle-rules) describes how `static_container` and
the recursion guard handle cycles.

## Good Source Companions

These files are useful while you read this flow:

- [include/dingo/runtime/container.h](../../include/dingo/runtime/container.h)
- [include/dingo/registration/type_registration.h](../../include/dingo/registration/type_registration.h)
- [include/dingo/runtime/registry.h](../../include/dingo/runtime/registry.h)
- [include/dingo/static/registry.h](../../include/dingo/static/registry.h)
- [include/dingo/static/graph.h](../../include/dingo/static/graph.h)
- [include/dingo/runtime/binding.h](../../include/dingo/runtime/binding.h)
- [include/dingo/binding/interface.h](../../include/dingo/binding/interface.h)
- [include/dingo/factory/constructor_detection.h](../../include/dingo/factory/constructor_detection.h)
