# Handles, Leaf Types, And Lookup

Dingo keeps the outer shape of a request apart from the type that it matches.
Much of its architecture depends on this.

## Terms

### Handle

A handle is the outer form of a request or a stored type:

- `T`
- `T&`
- `T*`
- `std::unique_ptr<T>`
- `std::shared_ptr<T>`
- a custom wrapper with `type_traits`

A handle can own or borrow what it holds.

### Leaf Type

The leaf type is the type that remains after Dingo removes the wrappers,
references, pointers and arrays, by its wrapper rules.

`leaf_type_t<T>` in
[include/dingo/type/shape.h](../../include/dingo/type/shape.h) gives the leaf
type.

Examples:

- `A` -> `A`
- `A&` -> `A`
- `std::shared_ptr<A>` -> `A`
- `test_shared<A>` -> `A`
- `std::shared_ptr<A[4]>` -> `A`

### Rebinding

Rebinding keeps the outer handle shape and replaces the leaf type.

`rebind_leaf_t<T, U>` replaces the leaf with `U` and keeps every wrapper,
reference and pointer around it.

So Dingo can express "the same handle shape over another leaf type" without a
rule for each wrapper combination.

## Binding Lookup

A request selects a binding by its normalized type or by its exact type. The
binding then derives the request (see
[Derivations](conversion-model.md#derivations)).

## Request Type

`request_type<T, RemoveRvalueReferences>` takes a request type apart:

- `user_type`: the type that the caller wrote.
- `lookup_type`: the request without its rvalue reference, if
  `RemoveRvalueReferences` is set.
- `interface_type`: `lookup_type` without its annotation or key selection.
- `value_type`: the normalized value form of `lookup_type`.
- `exact_type`: `interface_type` without `const`, `volatile` and reference.

Dingo normalizes a request once, at the public boundary. The runtime, static,
local and parent resolution paths then pass one descriptor through selection and
activation. They do not derive these types again.

## Why This Exists

Without leaf extraction and rebinding, each conversion rule would need a special
case for each wrapper combination. Dingo divides the problem into:

- wrapper semantics in `type_traits`
- storage exposure in `scope_traits`
- the derivation of a request, which the
  [layered semantics](layered-semantics.md) choose, and the steps that apply it

The rebinding utilities let these three layers work together.

## Good Source Companions

- [include/dingo/type/shape.h](../../include/dingo/type/shape.h)
- [include/dingo/type/type_traits.h](../../include/dingo/type/type_traits.h)
- [include/dingo/request/decomposition.h](../../include/dingo/request/decomposition.h)
- [include/dingo/runtime/binding.h](../../include/dingo/runtime/binding.h)
