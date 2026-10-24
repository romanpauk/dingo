# Handles, Leaf Types, And Lookup

Much of Dingo's architecture depends on separating the outer type tree of a
request or a stored type from the underlying type that is actually being
matched.

## Reachable Types

### Type Tree

A type tree is a leaf wrapped in wrappers. `dingo::type_tree_v<T>`, of
[type/wrapper_traits.h](../../include/dingo/type/wrapper_traits.h), describes
the type tree of `T` as a `constexpr` value: an array of nodes in preorder,
defined in [type/type_tree.h](../../include/dingo/type/type_tree.h), so a
wrapper is followed by the type trees of its arguments. A node of a wrapper
holds the descriptor of its wrapper semantics, the family of its wrapper, and
its cv-qualifiers; a node of a leaf holds the identity of the leaf type, the
address of a per-type tag.

### Wrapper

A wrapper is of a wrapper semantics, a tag type whose `wrapper_semantics`
declare its properties and forms. The kinds of the library are declared so in
[type/wrapper_semantics.gen.h](../../include/dingo/type/wrapper_semantics.gen.h),
and the conversion rules know a semantics only by its properties and operations:

- `T*` is a wrapper of `::dingo::semantics::pointer`, `T&` of
  `::dingo::semantics::reference`, `T[N]` of `::dingo::semantics::fixed_array`,
  and `T[]` of `::dingo::semantics::dynamic_array`
- `std::unique_ptr<T>` is a wrapper of `::dingo::semantics::sole_owner`,
  `std::shared_ptr<T>` of `::dingo::semantics::shared_owner`, `std::optional<T>`
  of `::dingo::semantics::optional_value`, and `std::variant<T...>` of
  `::dingo::semantics::variant_value`, by their `wrapper_traits`
- a custom wrapper is a wrapper of the kind its `wrapper_traits` name

The language wrappers are classified by `dingo::wrapper_traits`, and every
wrapper by its `wrapper_traits`; no wrapper gets other treatment. A type that is
not a wrapper is a leaf.

### Handle

A handle is the outer form of a request or stored type:

- `T`
- `T&`
- `T*`
- `std::unique_ptr<T>`
- `std::shared_ptr<T>`
- a custom wrapper with `wrapper_traits`

Handles carry ownership and borrowing semantics through the properties of their
kinds: owning, sharing, nullable, and borrowable among them.

### Leaf Type

The leaf type is the underlying type after references, cv-qualifiers, and
wrappers are stripped, through the first argument of every wrapper. A wrapper of
a variadic kind, such as a variant, holds several leaves and is its own leaf.

The public operation is `leaf_type_t<T>` in
[include/dingo/type/rebind_type.h](../../include/dingo/type/rebind_type.h),
which is `dingo::leaf_of_t<T>`.

Examples:

- `A` -> `A`
- `A&` -> `A`
- `std::shared_ptr<A>` -> `A`
- `test_shared<A>` -> `A`
- `std::shared_ptr<A[4]>` -> `A`
- `std::variant<A, B>` -> `std::variant<A, B>`

### Rebinding

Rebinding means keeping the outer wrappers while swapping the leaf type.

`rebind_type_t<T, U>` in
[include/dingo/type/rebind_type.h](../../include/dingo/type/rebind_type.h)
replaces the leaf of `T` with `U` through the first argument of every wrapper,
by the `rebind` of each wrapper's traits: `std::shared_ptr<A>` becomes
`std::shared_ptr<U>`, and `A*` becomes `U*`.

## Lookup

A request is matched in two steps, both in
[conversion/route.h](../../include/dingo/conversion/route.h).

First, the lookup filter names the bindings of a request: a binding is named
when it serves the interface of the request, the leaf of its type tree, with a
key of the same domain, and the backend compares the key values. A binding
serves its declared interfaces, or, without declared interfaces, the leaves its
stored type tree names. An annotated request names the leaf with the identity of
the annotated interface.

Second, the route selection takes the reachable type of the named binding that
serves the type tree of the request: a reachable type whose type tree and value
category the request accepts, or one from which the request makes up to two
wrappers of its own, by the forms of its kinds that create a wrapper. Of
several, it prefers the one with fewer rules, then one without a copy, then the
one whose forms come first in the order of the conversion rules.

The lookup filter is the same for every key kind and every container: no key, a
tag key, a fixed key, and a runtime key value. The indexes of the lookup
backends only accelerate the comparison of key values; see
[Lookup Indexes](extending-dingo.md#lookup-indexes).

## Exact Lookup

`exact_lookup<T>` is defined in
[include/dingo/type/rebind_type.h](../../include/dingo/type/rebind_type.h) as a
marker whose `type` is `T`. It remains part of the public API. The route
selection matches every request by its whole type tree, so a request of a
specific wrapper spelling is served only by a route that reaches that type tree,
and the conversion rules do not read the marker.

## Request Type

`dingo::request_traits<Request, Key>` in
[selection/request.h](../../include/dingo/selection/request.h) is the descriptor
of a request: its kind, value, lvalue reference, or rvalue reference;
`value_type`, the type it asks for without references; `made_type`, the type a
binding makes the object of a constructed request as; and `descriptor`, its
kind, type tree, and key, which the conversion rules read.

This keeps request normalization at the public boundary. Runtime, static, local,
and parent sources then pass one descriptor through the selection.

## Why This Exists

Without type trees and wrapper semantics, every conversion rule would have to
special-case every supported wrapper combination. Dingo instead factors the
problem into:

- the wrapper a wrapper is, in `wrapper_traits`
- the properties and forms of a wrapper semantics, in `wrapper_semantics`
- the routes, derived from the forms of the kinds of a type tree and the rules
  of a leaf

The conversion rules is what allows those three parts to compose.

## Good Source Companions

- [include/dingo/type/rebind_type.h](../../include/dingo/type/rebind_type.h)
- [include/dingo/type/wrapper_traits.h](../../include/dingo/type/wrapper_traits.h)
- [include/dingo/type/wrapper_traits.h](../../include/dingo/type/wrapper_traits.h)
- [include/dingo/type/type_tree.h](../../include/dingo/type/type_tree.h)
- [include/dingo/conversion/route.h](../../include/dingo/conversion/route.h)
- [include/dingo/selection/request.h](../../include/dingo/selection/request.h)
