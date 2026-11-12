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

## Lookup Type

A runtime request is keyed by its shape, not by the exact type of the request.
`request_lookup_type_t<T>` in
[include/dingo/type/request.h](../../include/dingo/type/request.h) gives this
key. It replaces the leaf with `runtime_type`, keeps the references, pointers
and wrappers, and removes the qualifiers:

- `A&` -> `runtime_type&`
- `A*` -> `runtime_type*`
- `std::shared_ptr<A>` -> `std::shared_ptr<runtime_type>`
- `test_unique<A>` -> `test_unique<runtime_type>`

The erased request of a runtime binding,
[`instance_request`](../../include/dingo/binding/interface.h), holds this key as
`lookup_type`. The key tells whether a request asks for a value, a borrowed
reference, a pointer or a wrapper, independently of its leaf.

A request selects a binding by its normalized type or by its exact type. The
binding then matches the type descriptor of the request against its resolutions
(see [Factory Dispatch](conversion-model.md#factory-dispatch)).

## Exact Lookup

`exact_lookup<T>` marks a request in the request list of a storage that keeps
its exact wrapper type. Rebinding does not change it: `rebind_leaf_t` of
`exact_lookup<T>` is `T`. Without the mark, a request is rebound to the leaf of
the interface, and that is too permissive for a request whose wrapper shape
matters.

It is defined in [include/dingo/type/shape.h](../../include/dingo/type/shape.h).
A storage uses it to publish its stored wrapper as itself. Examples are the
`std::optional<L>` that a kept optional is copied as, and a reference or a
pointer to a stored bounded array. The runtime binding,
[include/dingo/runtime/binding.h](../../include/dingo/runtime/binding.h),
matches a request against these targets as against any other.

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
- the operation that the matched `resolution` carries

The rebinding utilities let these three layers work together.

## Good Source Companions

- [include/dingo/type/shape.h](../../include/dingo/type/shape.h)
- [include/dingo/type/type_traits.h](../../include/dingo/type/type_traits.h)
- [include/dingo/type/request.h](../../include/dingo/type/request.h)
- [include/dingo/runtime/binding.h](../../include/dingo/runtime/binding.h)
