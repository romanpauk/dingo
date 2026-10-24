# Overview

Dingo is easiest to understand when separated into four concerns:

- registration: the reduction of the registration policies to a binding type
- shapes and routes: the shape of a stored type, and the routes from it that a
  binding serves
- selection: which binding answers a request, decided when the request is
  compiled or when it is resolved
- execution: the slot that keeps the object of a binding, the factory that makes
  it, and the walk of a route over the stored object

## Header Layers

The modules of the library are the directories of `include/dingo/` that the list
below names, and the public headers are every other header under
`include/dingo/`, the topmost layer, which holds the public include paths. A
header may include headers of the layers below its own and of its own module;
modules that share a layer do not include each other, and no module below the
public headers includes them. The `layers-check` target enforces this, and the
layer list lives in [tools/layers.py](../../tools/layers.py).

From the lowest layer to the highest:

1. `base/`: the configuration macros of `config.h`, the generated language
   capabilities and compiler variants, and the mechanics: the type list,
   `dependent_false`, and `pack_at_t`.
2. `primitives/`: the primitives that make no decision of the spec: the lifetime
   slots and the transaction log, allocation, retained objects, constructor
   detection, the type trees of C++ types, type names and the path of the types
   being constructed, messages, the exception types, and the diagnostics
   generated from the spec.
3. `conversion/`: the conversion rules as C++. The declarations generated from
   the spec: the operations and properties, the wrapper semantics of the
   library, the scope tags and descriptors, the storage forms, the selection
   rules, and the key kinds. The extension points over them: the wrapper traits,
   keys, annotations, collections, conversions, auto-construction, and the
   factories of a stored type tree. And the `constexpr` functions over
   descriptors: the registration, derivation, and route selection of `route.h`,
   the lookup declarations of `lookup.h`, the request descriptor of `request.h`,
   and the binding type and the walk of a reachable type of `convert.h`.
4. `backend/`: the backends: the static and the runtime backend, the memo of a
   request and the index of its key values, the slots of the scopes, the local
   bindings of a registration, the diagnostics of a backend, and the selection,
   the interpreter of the selection rules over the sources of a container.
5. The public headers: the containers, the registration declarations, the
   container traits with their lookup declarations and indexes, and the public
   include paths, which include what they declare and define it.

## Main Owners

### Public Containers

The public entry points are:

- [include/dingo/container.h](../../include/dingo/container.h): combined
  container
- [include/dingo/runtime_container.h](../../include/dingo/runtime_container.h):
  runtime-only spelling of the container
- [include/dingo/static_container.h](../../include/dingo/static_container.h):
  static-only container

`container` is the primary public API. It supports:

- `container<>`, `container<Traits>`, `container<Traits, Allocator>`, and
  `container<Traits, Allocator, Parent>` for runtime registrations
- `container<bindings<...>>`, with optional traits and parent, for compile-time
  bindings with optional runtime registrations

`runtime_container<Traits, Allocator, Parent>` is an alias of
`container<Traits, Allocator, Parent>`. `static_container<bindings<...>>`,
`static_container<bindings<...>, Traits>`, and
`static_container<bindings<...>, Parent>` hold compile-time bindings only.

Every container derives from `core::selection`, which resolves requests by the
selection rules over its sources, and from `container_operations`, which
provides `construct`, `construct_collection`, `invoke`, and
`register_type_collection` on top of the selection. A container declares its
sources: its static bindings, a `core::static_backend`; its runtime bindings, a
`core::runtime_backend`; and its parent.

A runtime container keeps its state apart from itself, in one allocation made by
its allocator: the runtime backend with its bindings, the objects it keeps for
requests, and the link to its parent. A mixed container keeps the same state
with its static backend; without static bindings it allocates the state as a
runtime container does, and with them it keeps the state in place. A static
container keeps its static backend in place. See
[Moving Containers](../advanced-topics.md#moving-containers).

### Registration

[registration.h](../../include/dingo/registration.h) reduces a
`register_type<...>()` declaration, and a `bind<...>` entry of `bindings<...>`,
to a `core::binding_type` of
[conversion/convert.h](../../include/dingo/conversion/convert.h):

- the scope tag, whose `scope_traits` hold the scope descriptor
- the stored type
- the declared interfaces, none when the registration declares no interface or
  only its stored type
- the factory: a declared `factory<...>`, the constructor of the types of a
  `dependencies<...>` part, a callable passed at registration, or the detected
  constructor
- the key: none, a tag key or a fixed key of `key_type<...>`, or the runtime key
  of a `key_value` argument
- the local static bindings of a `bindings<...>` part

The binding type computes its descriptor and, by
[conversion/route.h](../../include/dingo/conversion/route.h), the terms it
serves, when it is compiled. Both registration modes produce the same binding
type for the same policies.

### Slots And Factories

[backend/slot.h](../../include/dingo/backend/slot.h) selects the slot of a
binding by the slot kind of its scope descriptor, or the slot class its
`scope_traits` name:

- `unique`: no slot; the factory makes an object for every request, which the
  request consumes
- `shared`: a lazy slot, constructed on first use
- `external`: a reference to an object supplied at registration
- `shared_cyclical`: a two-phase slot that publishes its object before it
  constructs it

The slot keeps the factory of its binding, empty unless it holds a callable
object, and constructs the stored object by it.
[conversion/construct.h](../../include/dingo/conversion/construct.h) holds the
factories: a constructor factory constructs the leaf by its detected or declared
constructor and makes every layer around it in place, and a function or callable
factory returns the stored object.

### Routes And Walks

A route is a sequence of route forms from the stored shape to the requested
shape, as described in the [Conversion Model](conversion-model.md). The engine
derives the routes of a binding at compile time. `walk` in
[conversion/convert.h](../../include/dingo/conversion/convert.h) applies the
rules of one term to the stored object, one C++ operation per form, through the
layer traits of each layer. The binding walks a term up to its boundary, and the
request completes it: it refers to the object, copies it, takes its address, or
makes the layers it requests of it.

The static backend walks the selected term directly. The runtime backend
registers a thunk per term, and a request calls the thunk of the term it
selected.

### Selection

[backend/selection.h](../../include/dingo/backend/selection.h) interprets the
selection rules of the spec over the sources of a container: the local bindings
of the registration whose object is being constructed, the static and runtime
bindings of the container, its parent, auto-construction of an unregistered
type, and the factory of a request to construct an object. A step whose sources
all know what they name when the request is compiled is decided then, and any
other step when the request is resolved.

The local bindings of a registration are in
[backend/local.h](../../include/dingo/backend/local.h): the static bindings it
declares with `bindings<...>`, and the runtime bindings registered through the
proxy `register_type` returns, which are created by the first such registration.

## Resolution Sketch

At a high level, Dingo runs this sequence:

1. `register_type<...>()` or `bind<...>` reduces the policies to a binding type,
   which derives the terms it serves when it is compiled.
2. A runtime registration adds the binding with a thunk per term to the runtime
   backend; a static binding is a binding of the static backend of the container
   type.
3. `resolve<T>()` describes `T` as a request: the interface it names, its key,
   and its shape.
4. The selection asks the sources of the container in the order of the selection
   rules. The first step that names a binding answers the request; two bindings
   at that step are ambiguous, and no binding at any step is not found.
5. The backend that holds the binding selects the term that serves the request.
   The static backend selects it when the request is compiled. The runtime
   backend selects it at the first resolution and keeps it in the memo of the
   request type until the next registration.
6. The slot of the binding yields the stored object, constructing it by the
   factory when the scope needs one; the dependencies of the factory are
   resolved from the container through the same selection.
7. The binding walks the term to its boundary, and the request completes it to
   `T`.

A request that starts at a container is a transaction of it: when it fails, the
registrations into the container, the objects its slots constructed, and the
objects it kept for requests since it began are undone in reverse order.

## Reading The Code

For the core path in source order, start here:

- [include/dingo/container.h](../../include/dingo/container.h)
- [include/dingo/static_container.h](../../include/dingo/static_container.h)
- [include/dingo/registration.h](../../include/dingo/registration.h)
- [include/dingo/backend/selection.h](../../include/dingo/backend/selection.h)
- [include/dingo/backend/static_backend.h](../../include/dingo/backend/static_backend.h)
- [include/dingo/backend/runtime_backend.h](../../include/dingo/backend/runtime_backend.h)
- [include/dingo/backend/slot.h](../../include/dingo/backend/slot.h)
- [include/dingo/conversion/route.h](../../include/dingo/conversion/route.h)
- [include/dingo/conversion/convert.h](../../include/dingo/conversion/convert.h)
