# Handles, Leaf Types, And Lookup

Much of Dingo's architecture depends on separating the outer request shape from
the underlying type that is actually being matched.

## Terms

### Handle

A handle is the outer form of a request or stored type:

- `T`
- `T&`
- `T*`
- `std::unique_ptr<T>`
- `std::shared_ptr<T>`
- a custom wrapper with `type_traits`

Handles can carry ownership and borrowing semantics.

### Leaf Type

The leaf type is the underlying type after wrappers, references, and pointer
shape are stripped according to Dingo's wrapper rules.

The core operation is `leaf_type_t<T>` in
[include/dingo/type/rebind_type.h](../../include/dingo/type/rebind_type.h).

Examples:

- `A` -> `A`
- `A&` -> `A`
- `std::shared_ptr<A>` -> `A`
- `test_shared<A>` -> `A`
- `std::shared_ptr<A[4]>` -> `A`

### Rebinding

Rebinding means keeping the outer handle shape while swapping the leaf type.

The main operations are:

- `rebind_type_t<T, U>`: replace the wrapped type with `U`
- `rebind_leaf_t<T, U>`: replace only the leaf while preserving the outer shape

This is what lets Dingo talk about "the same handle shape over another leaf
type" without hardcoding every wrapper combination.

## Lookup Type

Dingo usually does not look up factories by the exact requested type spelling.
Instead it converts the request into a lookup form that replaces the leaf with
`runtime_type`.

That operation is `lookup_type_t<T>` in
[include/dingo/type/rebind_type.h](../../include/dingo/type/rebind_type.h).

The point of this step is to match requests by shape:

- `A&` -> `runtime_type&`
- `A*` -> `runtime_type*`
- `std::shared_ptr<A>` -> `std::shared_ptr<runtime_type>`
- `test_unique<A>` -> `test_unique<runtime_type>`

This is how the factory layer can decide whether the request is asking for a
value, borrowed reference, pointer, or wrapper-shaped handle without knowing the
concrete leaf in advance.

## Exact Lookup

`exact_lookup<T>` is the escape hatch when wrapper shape matters and the normal
leaf-based lookup would be too permissive.

It is defined in
[include/dingo/type/rebind_type.h](../../include/dingo/type/rebind_type.h) and
enforced by the matching logic in
[include/dingo/resolution/runtime_binding.h](../../include/dingo/resolution/runtime_binding.h).

Use it when the caller must request a specific wrapper spelling instead of "any
request with this leaf type."

## Why This Exists

Without leaf extraction and rebinding, every conversion rule would have to
special-case every supported wrapper combination. Dingo instead factors the
problem into:

- wrapper semantics in `type_traits`
- storage exposure in `storage_traits`
- runtime conversion in `type_conversion`

The rebinding utilities are what allow those three layers to compose.

## Good Source Companions

- [include/dingo/type/rebind_type.h](../../include/dingo/type/rebind_type.h)
- [include/dingo/type/type_traits.h](../../include/dingo/type/type_traits.h)
- [include/dingo/resolution/runtime_binding.h](../../include/dingo/resolution/runtime_binding.h)
