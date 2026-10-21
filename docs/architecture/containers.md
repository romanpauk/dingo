# Containers

This page documents the public container model.

## Public Entry Points

The top-level include set is intentionally small:

- [include/dingo/container.h](../../include/dingo/container.h): combined public
  container
- [include/dingo/runtime_container.h](../../include/dingo/runtime_container.h):
  runtime-only spelling of the container
- [include/dingo/static_container.h](../../include/dingo/static_container.h):
  static-only container

Each forwards to its facade under `include/dingo/core/api/`. Everything else is
the core of the library, described in
[Header Layers](overview.md#header-layers).

## Unified Container

`container` is the primary public entry point.

It supports two main forms:

```c++
dingo::container<> runtime_only;

using app_bindings = dingo::bindings<
    dingo::bind<dingo::scope<dingo::shared>, dingo::storage<Config>>,
    dingo::bind<dingo::scope<dingo::unique>, dingo::storage<Service>>
>;

dingo::container<app_bindings> app_container;
```

The runtime-only form, `container<Traits, Allocator, Parent>`, holds the
bindings registered through `register_type<...>()`. Its traits declare its
lookups, its allocator allocates its state and its bindings, and its parent is
the container a request falls back to.

The `bindings<...>` form, `container<bindings<...>>`,
`container<bindings<...>, Traits>`, `container<bindings<...>, Parent>`, or
`container<bindings<...>, Traits, Parent>`, hosts both:

- compile-time bindings from the `bindings<...>` source
- runtime registrations added later through `register_type<...>()`

The static and runtime bindings share the lookup declarations of its traits.
Static bindings are known when the container type is named, and runtime
registrations can still be added afterward. The container takes the parent, when
it has one, and the instance of every external static binding as constructor
arguments, in the order of the bindings.

## Static And Runtime Facades

The split container facades remain available when the narrower shape is useful:

- `runtime_container<Traits, Allocator, Parent>`, an alias of
  `container<Traits, Allocator, Parent>`, for explicitly runtime-only code
- `static_container<bindings<...>>`, `static_container<bindings<...>, Traits>`,
  or `static_container<bindings<...>, Parent>` for explicitly static-only code

`static_container` has no runtime registration API. A static child of a parent
has the traits of its parent. `container` is the public container that can
represent runtime-only or mixed registration.

## Binding Semantics

`container<bindings<...>>` can have two binding sources: the static bindings in
the container type and the runtime registrations added to the object. They are
one level of the selection: a static and a runtime binding of one key are two
candidates.

A runtime registration whose key a static binding of a common interface holds is
rejected with `lookup_already_registered_exception`, unless the lookup
declarations declare the domain many. A request for one object of a domain
declared many that both sources name is ambiguous, because neither source is an
override of the other. A request that two static bindings name when it is
compiled is rejected then, as in a static container, since a runtime
registration only adds candidates.

A request for a collection holds the bindings of both sources, the runtime ones
before the static ones.

A parent container is the next level. `container<Traits, Allocator, Parent>`,
`container<bindings<...>, Parent>`, and
`static_container<bindings<...>, Parent>` keep the child level first: a child
binding shadows the parent for singular and collection requests, and a request
the child names no binding of is resolved from the parent. A binding the parent
serves resolves its dependencies in the parent.

Local `bindings<...>` on a registration declare the static part of the local
bindings of that registration. Runtime bindings added through the returned proxy
are local to it too. Its object resolves its dependencies through a view whose
local level is those bindings, whose container level is the host's own bindings,
and whose parent is the host's parent: local bindings shadow the host's for
singular and collection requests alike.

The selection rules are listed in
[Registration And Resolution](registration-and-resolution.md#selection).

## Direct Construction

`construct<T>()` follows the construct rule: a binding of `T`, local, of the
container, or of its parent, answers it as a singular request does, and
otherwise the factory of `T` makes the object, whether or not `T` could be
auto-constructed. `construct<T, Factory>()` and `construct<T>(factory)` make the
object by the given factory.

A wrapper such as `construct<std::shared_ptr<T>>()` whose element has a binding
that answers the request but cannot serve the wrapper is made from a copy of the
bound element. If no binding names `T`, construction uses the constructor of
`T`.

## Cycle Rules

A cycle of dependencies is legal only when every binding in it has a scope with
a two-phase slot, which `scope<shared_cyclical>` has. A supported cycle uses the
same two-phase slot in every container, so each participating object can be
published before its dependencies finish resolving.

The dependencies a factory declares, by `dependencies<...>`, an explicit
`constructor<T(Args...)>`, a constructor the type declares with
`DINGO_CONSTRUCTOR`, or the signature of a function or a callable, are known
when a static container is compiled: the static backend builds their graph and
rejects the shortest illegal cycle with the `type_recursion` diagnostic and its
path, `left -> right -> left`. This applies to `static_container` and to the
static bindings of `container<bindings<...>>`.

The dependencies of a detected constructor are known only when the object is
constructed. A cycle through one is detected when it is resolved, by the slot of
the binding being constructed, and reported with `type_recursion_exception` and
the path of the types being constructed. This applies to every container, as
does recursion through runtime bindings.

## Related Headers

The key implementation headers behind this model are:

- [include/dingo/core/api/container.h](../../include/dingo/core/api/container.h)
- [include/dingo/core/api/static_container.h](../../include/dingo/core/api/static_container.h)
- [include/dingo/core/api/container_traits.h](../../include/dingo/core/api/container_traits.h)
- [include/dingo/core/backend/selection.h](../../include/dingo/core/backend/selection.h)
- [include/dingo/core/backend/static_backend.h](../../include/dingo/core/backend/static_backend.h)
- [include/dingo/core/backend/runtime_backend.h](../../include/dingo/core/backend/runtime_backend.h)
- [include/dingo/core/backend/local.h](../../include/dingo/core/backend/local.h)
