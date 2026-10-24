# Overview

Dingo is easiest to understand when separated into four concerns:

- registration: the reduction of the registration policies to a binding type
- type trees and routes: the type tree of a stored type, and the routes from it
  that a binding serves
- selection: which binding answers a request, decided when the request is
  compiled or when it is resolved
- execution: the slot that keeps the object of a binding, the factory that makes
  it, and the walk of a route over the stored object

## Components

The library is a set of components. Each is a directory of `include/dingo/` that
holds the generated declarations, the extension points, and the functions of one
subject, and each depends only on the components listed for it, so the
dependencies form a graph without cycles. The public headers that no component
holds, the containers and the registration declarations, are the top of the
graph and may include any component. The `layers-check` target enforces this,
and the dependencies live in [tools/layers.py](../../tools/layers.py).

The four concerns above are spread over them: registration is the public
headers, type trees and routes are `type`, `conversion`, and the declarations of
`storage`, selection is `lookup` and `selection`, and execution is `memory`,
`factory`, `storage`, and `backend`. From the lowest component to the highest:

01. `base/`: the configuration macros of `config.h`, the generated language
    capabilities and compiler variants, and the mechanics: the type list,
    `dependent_false`, and `pack_at_t`. Depends on nothing. Spec:
    `cpp_capabilities`, `variant_points`.
02. `diagnostics/`: the messages of a diagnostic, the exception types, the
    diagnostics generated from the spec, the names of types, and the path of the
    types being constructed. Depends on `base`. Spec: `diagnostics`.
03. `memory/`: the aligned allocation, the objects a request retains, and the
    lifetime slots that say where a binding keeps its object. Depends on `base`.
04. `constructor/`: the detection of the constructor a factory calls, in the
    portable and the msvc backends, with its arity and its signature. Depends on
    `base`.
05. `type/`: what a stored type is made of: the type tree of a C++ type, the
    wrapper semantics and the operations of the conversion rules, the wrapper
    traits, the annotated interfaces, the collection traits, the conversions of
    a wrapper, and auto-construction. These are the public `type/` headers.
    Depends on `base`, `diagnostics`, and `constructor`. Spec:
    `wrapper_semantics`, `operations`.
06. `lookup/`: how a binding is found by a key: the keys with their key kinds,
    the keys of annotated requests, the lookup declarations of a container, and
    the indexes of key values. These are the public `lookup/` headers and
    `key.h`. Depends on `base`, `diagnostics`, and `type`. Spec: `key_kinds`,
    `lookup_declarations`, `index_structures`, `lookup_filters`.
07. `storage/`: the scopes and the storage kinds: the scope descriptors and tags
    and the request categories each exposes for a storage kind. These are the
    public `storage/` headers. Depends on `base` and `type`. Spec:
    `storage_kinds`, `scopes`.
08. `factory/`: the factories of a stored type tree, which construct the leaf
    and make its wrappers. These are the public `factory/` headers. Depends on
    `base`, `diagnostics`, `type`, and `constructor`. Spec: `factory_kinds`.
09. `conversion/`: the conversion rules as C++: the registration, derivation,
    and route selection of `route.h`, the binding type and the walk of a
    reachable type of `convert.h`, and where two bindings conflict in
    `binding_conflicts.h`. Depends on `type`, `lookup`, `storage`, `factory`,
    `memory`, `diagnostics`, and `base`. Spec: `rules`.
10. `selection/`: which binding answers a request: the request descriptor, the
    selection rules and their interpreter over the sources of a container, and
    the reports of its outcomes. Depends on `conversion` and below. Spec:
    `selection_rules`, `selection_sources`, `decorations`.
11. `backend/`: the static and the runtime backend, the memo of a request, the
    slot of a binding, and the local bindings of a registration. Depends on
    `selection` and below. Spec: `transaction_rules`, `graph_rules`,
    `binding_time_constraints`.
12. The public headers: the containers, the registration declarations, the
    container traits, the collection requests, and the runtime type information.
    A public header in the directory of a component that includes
    `registration.h` is one of them. Spec: `public_api`.

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
[conversion/route.h](../../include/dingo/conversion/route.h), the reachable
types it serves, when it is compiled. Both registration modes produce the same
binding type for the same policies.

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
[factory/construct.h](../../include/dingo/factory/construct.h) holds the
factories: a constructor factory constructs the leaf by its detected or declared
constructor and makes every wrapper around it in place, and a function or
callable factory returns the stored object.

### Routes And Walks

A route is a sequence of operations from the stored type tree to the requested
type tree, as described in the [Conversion Model](conversion-model.md). The
conversion component derives the routes of a binding at compile time. `walk` in
[conversion/convert.h](../../include/dingo/conversion/convert.h) applies the
rules of one reachable type to the stored object, one C++ operation per form,
through the wrapper traits of each wrapper. The binding walks a reachable type
up to its boundary, and the request completes it: it refers to the object,
copies it, takes its address, or makes the wrappers it requests of it.

The static backend walks the selected reachable type directly. The runtime
backend registers a thunk per reachable type, and a request calls the thunk of
the reachable type it selected.

### Selection

[selection/selection.h](../../include/dingo/selection/selection.h) interprets
the selection rules of the spec over the sources of a container: the local
bindings of the registration whose object is being constructed, the static and
runtime bindings of the container, its parent, auto-construction of an
unregistered type, and the factory of a request to construct an object. A step
whose sources all know what they name when the request is compiled is decided
then, and any other step when the request is resolved.

The local bindings of a registration are in
[backend/local.h](../../include/dingo/backend/local.h): the static bindings it
declares with `bindings<...>`, and the runtime bindings registered through the
proxy `register_type` returns, which are created by the first such registration.

## Resolution Sketch

At a high level, Dingo runs this sequence:

1. `register_type<...>()` or `bind<...>` reduces the policies to a binding type,
   which derives the reachable types it serves when it is compiled.
2. A runtime registration adds the binding with a thunk per reachable type to
   the runtime backend; a static binding is a binding of the static backend of
   the container type.
3. `resolve<T>()` describes `T` as a request: the interface it names, its key,
   and its type tree.
4. The selection asks the sources of the container in the order of the selection
   rules. The first step that names a binding answers the request; two bindings
   at that step are ambiguous, and no binding at any step is not found.
5. The backend that holds the binding selects the reachable type that serves the
   request. The static backend selects it when the request is compiled. The
   runtime backend selects it at the first resolution and keeps it in the memo
   of the request type until the next registration.
6. The slot of the binding yields the stored object, constructing it by the
   factory when the scope needs one; the dependencies of the factory are
   resolved from the container through the same selection.
7. The binding walks the reachable type to its boundary, and the request
   completes it to `T`.

A request that starts at a container is a transaction of it: when it fails, the
registrations into the container, the objects its slots constructed, and the
objects it kept for requests since it began are undone in reverse order.

## Reading The Code

For the core path in source order, start here:

- [include/dingo/container.h](../../include/dingo/container.h)
- [include/dingo/static_container.h](../../include/dingo/static_container.h)
- [include/dingo/registration.h](../../include/dingo/registration.h)
- [include/dingo/selection/selection.h](../../include/dingo/selection/selection.h)
- [include/dingo/backend/static_backend.h](../../include/dingo/backend/static_backend.h)
- [include/dingo/backend/runtime_backend.h](../../include/dingo/backend/runtime_backend.h)
- [include/dingo/backend/slot.h](../../include/dingo/backend/slot.h)
- [include/dingo/conversion/route.h](../../include/dingo/conversion/route.h)
- [include/dingo/conversion/convert.h](../../include/dingo/conversion/convert.h)
