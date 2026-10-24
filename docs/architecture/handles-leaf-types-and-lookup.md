# Handles, Leaf Types, And Lookup

Much of Dingo's architecture depends on separating the outer shape of a request
or a stored type from the underlying type that is actually being matched.

## Terms

### Shape

A shape is a leaf wrapped in layers. `core::shape_v<T>`, of
[conversion/wrapper_traits.h](../../include/dingo/conversion/wrapper_traits.h),
describes the shape of `T` as a `constexpr` value: an array of nodes in
preorder, defined in
[primitives/type_tree.h](../../include/dingo/primitives/type_tree.h), so a layer
is followed by the shapes of its arguments. A node of a layer holds the
descriptor of its wrapper semantics, the family of its wrapper, and its
cv-qualifiers; a node of a leaf holds the identity of the leaf type, the address
of a per-type tag.

### Layer

A layer is of a wrapper semantics, a tag type whose `wrapper_semantics` declare
its properties and forms. The kinds of the library are declared so in
[conversion/wrapper_semantics.gen.h](../../include/dingo/conversion/wrapper_semantics.gen.h),
and the engine knows a kind only by its properties and forms:

- `T*` is a layer of `::dingo::semantics::pointer`, `T&` of
  `::dingo::semantics::reference`, `T[N]` of `::dingo::semantics::fixed_array`,
  and `T[]` of `::dingo::semantics::dynamic_array`
- `std::unique_ptr<T>` is a layer of `::dingo::semantics::sole_owner`,
  `std::shared_ptr<T>` of `::dingo::semantics::shared_owner`, `std::optional<T>`
  of `::dingo::semantics::optional_value`, and `std::variant<T...>` of
  `::dingo::semantics::variant_value`, by their `wrapper_traits`
- a custom wrapper is a layer of the kind its `wrapper_traits` name

The language layers are classified by `core::wrapper_traits`, and every wrapper
by its `wrapper_traits`; no wrapper gets other treatment. A type that is not a
layer is a leaf.

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

The leaf type is the underlying type after references, cv-qualifiers, and layers
are stripped, through the first argument of every layer. A layer of a variadic
kind, such as a variant, holds several leaves and is its own leaf.

The public operation is `leaf_type_t<T>` in
[include/dingo/type/rebind_type.h](../../include/dingo/type/rebind_type.h),
which is `core::leaf_of_t<T>`.

Examples:

- `A` -> `A`
- `A&` -> `A`
- `std::shared_ptr<A>` -> `A`
- `test_shared<A>` -> `A`
- `std::shared_ptr<A[4]>` -> `A`
- `std::variant<A, B>` -> `std::variant<A, B>`

### Rebinding

Rebinding means keeping the outer layers while swapping the leaf type.

`rebind_type_t<T, U>` in
[include/dingo/type/rebind_type.h](../../include/dingo/type/rebind_type.h)
replaces the leaf of `T` with `U` through the first argument of every layer, by
the `rebind` of each layer's traits: `std::shared_ptr<A>` becomes
`std::shared_ptr<U>`, and `A*` becomes `U*`.

## Lookup

A request is matched in two steps, both in
[conversion/route.h](../../include/dingo/conversion/route.h).

First, the lookup filter names the bindings of a request: a binding is named
when it serves the interface of the request, the leaf of its shape, with a key
of the same domain, and the backend compares the key values. A binding serves
its declared interfaces, or, without declared interfaces, the leaves its stored
shape names. An annotated request names the leaf with the identity of the
annotated interface.

Second, the route selection takes the term of the named binding that serves the
shape of the request: a term whose shape and value category the request accepts,
or one from which the request makes up to two layers of its own, by the forms of
its kinds that create a layer. Of several, it prefers the one with fewer rules,
then one without a copy, then the one whose forms come first in the order of the
algebra.

The lookup filter is the same for every key kind and every container: no key, a
tag key, a fixed key, and a runtime key value. The indexes of the lookup
backends only accelerate the comparison of key values; see
[Lookup Indexes](extending-dingo.md#lookup-indexes).

## Exact Lookup

`exact_lookup<T>` is defined in
[include/dingo/type/rebind_type.h](../../include/dingo/type/rebind_type.h) as a
marker whose `type` is `T`. It remains part of the public API. The route
selection matches every request by its whole shape, so a request of a specific
wrapper spelling is served only by a route that reaches that shape, and the
engine does not read the marker.

## Request Type

`core::request_traits<Request, Key>` in
[conversion/request.h](../../include/dingo/conversion/request.h) is the
descriptor of a request: its kind, value, lvalue reference, or rvalue reference;
`value_type`, the type it asks for without references; `made_type`, the type a
binding makes the object of a constructed request as; and `descriptor`, its
kind, shape, and key, which the engine reads.

This keeps request normalization at the public boundary. Runtime, static, local,
and parent sources then pass one descriptor through the selection.

## Why This Exists

Without shapes and wrapper semantics, every conversion rule would have to
special-case every supported wrapper combination. Dingo instead factors the
problem into:

- the layer a wrapper is, in `wrapper_traits`
- the properties and forms of a wrapper semantics, in `wrapper_semantics`
- the routes, derived from the forms of the kinds of a shape and the rules of a
  leaf

The shape algebra is what allows those three parts to compose.

## Good Source Companions

- [include/dingo/type/rebind_type.h](../../include/dingo/type/rebind_type.h)
- [include/dingo/type/type_traits.h](../../include/dingo/type/type_traits.h)
- [include/dingo/conversion/wrapper_traits.h](../../include/dingo/conversion/wrapper_traits.h)
- [include/dingo/primitives/type_tree.h](../../include/dingo/primitives/type_tree.h)
- [include/dingo/conversion/route.h](../../include/dingo/conversion/route.h)
- [include/dingo/conversion/request.h](../../include/dingo/conversion/request.h)
