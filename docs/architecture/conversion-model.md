# Conversion Model

Dingo resolves requests in two stages:

1. derive the reachable types a binding serves from its stored type tree, and
   select the reachable type that serves the requested type tree
2. walk the selected reachable type over the stored object, and complete it to
   the requested type

Both stages are the same for every container. The first runs when the binding
type and the request are compiled, and the second when the request is resolved.

## Where The Rules Come From

The full conversion behavior comes from a few sources of data:

- the scope descriptor, held by `scope_traits`, says which value category the
  slot materializes its object as, whether its objects keep their addresses, and
  which request categories it exposes for each storage form
- the operations say which reachable type each rule derives from which, with
  which access and requirements
- the `wrapper_semantics` of each wrapper semantics say which operations its
  wrappers have
- `type_conversion_traits` declares a conversion of a wrapper to the wrapper of
  an interface, or of a leaf to an interface, that is not a converting
  constructor

The important source files are:

- [include/dingo/type/operations.gen.h](../../include/dingo/type/operations.gen.h)
- [include/dingo/type/wrapper_semantics.gen.h](../../include/dingo/type/wrapper_semantics.gen.h)
- [include/dingo/storage/scopes.gen.h](../../include/dingo/storage/scopes.gen.h)
- [include/dingo/conversion/route.h](../../include/dingo/conversion/route.h)
- [include/dingo/conversion/convert.h](../../include/dingo/conversion/convert.h)
- [include/dingo/type/type_conversion_traits.h](../../include/dingo/type/type_conversion_traits.h)

## Reachable Types And Operations

A reachable type is a type tree with a value category, lvalue, rvalue, or
prvalue, and the route that reaches it. A binding starts from its stored type
tree in the category its scope materializes: an lvalue for the borrowing scopes,
`shared`, `external`, and `shared_cyclical`, and an rvalue for `unique`, which
constructs an object for every request. Each rule derives a reachable type from
a reachable type:

- `borrow`: the element of a wrapper, borrowed
- `copy`: a copy of a borrowed leaf, which needs a copyable leaf
- `address`: the address of a borrowed object that can be pointed to
- `decay`: the address of the first element of a borrowed array
- `take`: the element of a consumed wrapper
- `wrap`: a wrapper made around a consumed element, such as an optional
- `share`: the sharing handle of its family that takes the element of a consumed
  sole-owner handle
- `adopt`: an owning handle that takes the ownership of a consumed address
- `release` and `release_whole`: the address of the elements, or of the whole
  array, of a consumed array that a scope allocates for the request
- `upcast_reference`, `upcast_copied`, and `upcast_consumed`: a reference to a
  declared interface of a leaf, and the handle of an interface converted from a
  copied or a consumed handle
- `convert_borrowed` and `convert_consumed`: an object of a declared interface
  made by the user conversion a leaf declares to it
- `retain`: the handle of an interface converted from a borrowed handle, which
  the binding keeps so a request can refer to it

`copy`, `upcast_reference`, and the two conversions are rules of a leaf. The
others are operations of a wrapper semantics, and a wrapper has the operations
its semantics declares: a `std::unique_ptr` borrows, shares, adopts, and upcasts
a consumed handle, and a `std::shared_ptr` borrows, adopts, upcasts, and
retains. An operation that leads to another semantics names it, such as the
sharing semantics a sole-owner handle shares its element with, and a route
converts a wrapper only to another wrapper of its family.

Each rule borrows its source, consumes it, or inherits the access of the
reachable type it starts from. A borrowing scope exposes nothing consumed, so a
request of a `shared` binding never moves the stored object, while a `unique`
binding hands its object over.

## Derivation

`derive` in [conversion/route.h](../../include/dingo/conversion/route.h) applies
every form that does not create a wrapper to the reachable types of a binding,
in the order of the conversion rules, and keeps the reachable types it finds. A
route derives type trees at most one wrapper larger than the stored type tree,
so a binding can publish every reachable type it serves before any request is
known. A static binding derives its reachable types when the container type is
compiled, and a runtime binding when its registration is compiled.

A form that creates a wrapper, `wrap`, `adopt`, or `address`, applies at the
request instead: a binding cannot know every kind requested of it, so a request
of a wrapper whose kind wraps or adopts resolves its element and makes the
wrapper of it. A request takes a reachable type of the binding, or makes one by
up to two wrappers of its own kinds. Of several routes, it prefers the one with
fewer rules, then one without a copy, then the one whose forms come first in the
order of the conversion rules.

A request through a variant is served by the alternative its object holds. A
derivation keeps, beside a reachable type, its siblings: routes to the same
reachable type through the other alternatives. A walk that reaches an object
holding another alternative continues along the sibling that takes it, and a
request fails with `type_not_convertible_exception`, the diagnostic
`alternative_not_held`, only when no sibling takes the alternative held.

## Storage Exposure

A storage form classifies the outer wrappers of a stored type tree: plain,
reference, pointer, a unique or a shared handle, a unique or a shared array
handle, an optional, an alternative, a bounded or an unbounded array. The scope
descriptor lists the request categories, value, lvalue reference, rvalue
reference, and pointer, it exposes for each storage form; a route whose request
category the scope does not expose for the stored form is not served. A
registration whose scope does not store its form, or its nested wrappers, is
rejected when it is compiled.

Storage stability is a separate property of the scope. A stable scope, `shared`
or `external`, keeps the addresses of the objects it borrows, so a runtime
container keeps the address a reachable type borrows once the slot is settled.
`shared_cyclical` is not stable, as a failed publication resets its slots.

## Execution

`walk` in [conversion/convert.h](../../include/dingo/conversion/convert.h)
applies the rules of one reachable type to the stored object, one C++ operation
per form: the operation of the wrapper traits of the object, or of the language
for an address. The binding applies the rules of a reachable type up to its
boundary, and the request completes it:

- a last copy, address-of, or decay of a borrowed object depends only on the
  requested type, so the binding returns the borrowed object and the request
  copies it, takes its address, or decays it
- a wrapper that a rule creates from a leaf or a pointer may be of any family,
  so the binding makes the boundary object and the request wraps or adopts it
  into the wrapper it names

The static backend walks and completes the reachable type directly. A runtime
binding registers a thunk per reachable type that walks it to its boundary, and
the request checks the type of the boundary object before it completes it, so
neither backend moves an object the other does not.

A request of an rvalue reference to an object a binding constructs for it refers
to an object the container keeps, constructed in place, until the container is
destroyed. An interface handle that `retain` converts from a borrowed handle is
kept in the context of the slot of the binding, and forgotten when the slot
destroys its object.

## Custom Conversions

Specialize `type_conversion_traits<Target, Source>` in
[include/dingo/type/type_conversion_traits.h](../../include/dingo/type/type_conversion_traits.h)
when a conversion cannot use the converting constructor of the target. Without a
specialization, a wrapper converts to the wrapper of an interface by its
converting constructor, and a leaf converts to no interface it does not derive
from.

A specialization provides a static `convert`, which takes the source and returns
the target or an object the target is constructed from. A borrowed source is
passed as an lvalue and a consumed one as an rvalue, so a `convert` that takes a
const lvalue reference serves both. It also declares `required_access<From>` as
`borrow` or `consume` for the reference `From` to the source: a borrowed source
converts only by a conversion that borrows, and a consumed source by either. A
specialization that declares a conversion without `required_access` is rejected
with the diagnostic `conversion_access_undeclared`.

A leaf that declares a conversion to one of the declared interfaces of its
binding serves that interface by `convert_borrowed` or `convert_consumed`. A
value request copies a leaf by its copy constructor; a conversion of a leaf to
its own type is not a route.
