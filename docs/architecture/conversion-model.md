# Conversion Model

Dingo resolves a request in two stages:

1. It derives the request: the steps from what the binding stores to the
   requested shape.
2. It applies the conversion to the source that the storage gives.

The [layered semantics](layered-semantics.md) give the rules and the reasons for
them. This page tells where each part is in the code.

## Where The Rules Come From

- `scope_traits` tells whether a scope stores a form of a stored type, and what
  the scope does: whether it makes a new instance for each request, is given its
  instance, or publishes it early (`scope_semantics_v` in
  [include/dingo/storage/scope_traits.h](../../include/dingo/storage/scope_traits.h)).
- `type_traits` gives the properties of each layer of a type: what it owns,
  whether a copy of it shares its element, and whether it can be copied, moved,
  made of a value, made of a raw pointer, or be empty
  ([include/dingo/request/classification.h](../../include/dingo/request/classification.h)
  reads them).
- The semantics derive a request of these facts, with no C++ type
  ([include/dingo/semantics/model.h](../../include/dingo/semantics/model.h)).
- The steps apply a conversion to C++ types
  ([include/dingo/request/execution.h](../../include/dingo/request/execution.h)).

A type converts to another only as C++ converts it, or as the `type_traits` of
its layers say. A conversion is not an extension point of its own.

## Derivations

A derivation is the list of steps from the stored object to what satisfies the
request. Of all derivations, Dingo takes the one with the fewest copies of the
leaf, then the fewest moves, then one that is not absent when a layer is empty,
then the fewest objects kept as long as the container, then the fewest objects
made for the request.

A step belongs to the side that knows the type it is applied to:

- The binding takes the element of a wrapper, the pointer to it or its empty
  value, the upcast to the interface, the conversion of a sole owner to its
  sharing handle, a copy, the decay of an array, and the hand-over or the
  release of what a pointer it makes points to.
- The request takes the steps that make its own layers: a wrap, the adoption of
  a pointer it owns, and an observer of a pointer that owns nothing.

A request then refers to what the conversion reached, takes it as a pointer
value, moves it, or copies it. A derivation is known for each lifetime a request
needs: as long as the container, for what the caller of `resolve` takes, or as
long as the request, for a dependency.

## Conversions

Both containers run a derivation as a conversion
([include/dingo/request/execution.h](../../include/dingo/request/execution.h)).
`compile_conversion` turns a derivation into instructions, and takes the
decisions of how to run it in one place:

- A run of wraps of one side, the binding's or the request's, is one
  instruction. It makes its outermost layer at once, in place.
- A layer that a request makes of a value or of a pointer is made of the
  qualifiers its element needs at once. The conversion to them that follows it
  is the same instruction.
- A value that a request takes is made where the request takes it, when the last
  step makes it: a copy or a run of wraps that the request makes, and a copy of
  the element of a layer that may be empty.

## Static Derivations

A static binding derives a request as the code compiles (`static_execution` in
[include/dingo/static/binding.h](../../include/dingo/static/binding.h)). It runs
the conversion of the derivation as typed functions (`execute_conversion`),
which the compiler inlines. A request that no derivation satisfies does not
compile.

## Runtime Derivations

A runtime binding derives a request at its first use, and keeps the conversion
in the conversion cache of the request type
([include/dingo/runtime/request.h](../../include/dingo/runtime/request.h)). The
conversion of the derivation is run through tables of erased operations
([include/dingo/runtime/execution.h](../../include/dingo/runtime/execution.h)).
`plan_runtime` maps each instruction to an operation of a table:

- A binding has one table for each C++ type of its layer sources and of what a
  step makes of them (`binding_sources`). Each operation of a table names the
  table of the type it makes.
- A request has one table for each of its nested types, with the operations that
  make the type of the one below (`request_tables`).

Both are built from the typed steps of the static container, so the containers
apply one implementation of each step. A step that does the same to an object
that the request owns as to one that it does not has one operation for both.
Each table is checked as the code compiles against the steps that the semantics
allow from its type, so a derivation meets no missing operation. A request that
no derivation satisfies throws `type_not_convertible_exception`.

The operations keep what they make through `runtime_store`, an interface of no
context. It keeps an object by its layout (`object_layout`: size, alignment and
destructor) and one function that constructs it. The tables, the conversions and
`plan_runtime` do not depend on the context, and keeping is not instantiated for
each type.

A runtime resolution returns a `resolved_address`: the erased address and the
access of the result. A value is made where the call returns it, of what the
conversion reached, as the static container makes it.

## Objects Made Of A Value

A storage, a factory and `construct<T>()` make an object of a value with
`make_value`: the object constructed of the value, a run of wraps around it, or
a handle of an array of it that its `type_traits` emplace. A factory emplaces a
wrapper of what it constructs in place, down the layers that declare
`emplaces_element`, and makes it of the constructed value only where it cannot,
as an alternative type that selects an alternative of the value.

## Retention

A reference or a pointer to a wrapper that a conversion makes is kept or placed:

- A binding of a scope that keeps its instance keeps the wrapper in the
  retention of its cell,
  [include/dingo/storage/retention.h](../../include/dingo/storage/retention.h).
  The retention holds one object for each type, under the key of the type. It
  makes the object the first time a request asks for it. So every reference to
  it refers to the same object while the binding lives. A runtime binding also
  publishes the address in the request cache.
- A binding of a scope that makes an instance for each request keeps nothing. It
  places the wrapper in the context instead.

A runtime container makes the retained objects in its persistent memory. It
forgets them when the transaction that made them rolls back. A static container
can have no persistent memory, so its retention owns the objects.
