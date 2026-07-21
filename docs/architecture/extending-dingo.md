# Extending Dingo

Dingo's extensibility is mostly trait-driven. The key is to specialize the trait
that matches the problem actually being solved.

Dingo's container and resolution pipeline do not hard-code a closed list of
supported wrapper forms. Library support for standard smart pointers, arrays,
variants, and similar shapes is implemented through the same traits and
conversion hooks available to external code. Adding support for another wrapper
or storage shape follows the same path as Dingo's built-in support.

## The Three Main Extension Points

### 1. `type_traits`

Specialize `type_traits` in
[include/dingo/type/type_traits.h](../../include/dingo/type/type_traits.h) when
introducing a new wrapper or handle type.

`type_traits` defines wrapper semantics:

- whether the type participates in Dingo's wrapper machinery
- whether it behaves like a pointer-like handle
- how to get or borrow the underlying object
- how to reset it
- how to rebind it to a different leaf type
- whether Dingo may resolve references through it
- how wrapper-shaped resolution should behave for that type

If Dingo needs to understand a wrapper's shape, start here.

### 2. `storage_traits`

Specialize `storage_traits` in
[include/dingo/storage/type_storage_traits.h](../../include/dingo/storage/type_storage_traits.h)
when defining which result forms a given storage/scope combination can expose.

This trait defines whether a registration can service requests such as:

- value
- lvalue reference
- rvalue reference
- pointer
- wrapper-to-wrapper conversion

The same wrapper may have different exposure rules under `unique`, `shared`, or
`external` storage.

Use `value_types` for values the storage may consume or freshly produce, and
`copy_value_types` for values that must be copied from retained storage.
`runtime_type` represents the registered leaf, while `runtime_interface`
represents the complete registered interface when an exact wrapper capability is
needed.

### 3. `type_conversion_traits`

Specialize `type_conversion_traits` in
[include/dingo/type/type_conversion_traits.h](../../include/dingo/type/type_conversion_traits.h)
when two wrapper types need a concrete conversion step that is not covered by a
direct converting constructor or pointer cast.

This is the last-mile "build target wrapper from source wrapper" hook.

## Extension Example

The best example is already in
[test/type/type_traits.cpp](../../test/type/type_traits.cpp).

That test adds:

- `test_shared<T>`
- `test_unique<T>`
- `test_optional<T>`

and then specializes the exact traits Dingo needs:

- `type_traits` for wrapper semantics
- `storage_traits` for scope-specific exposure
- `type_conversion_traits` for wrapper conversion

That file is worth treating as executable documentation.

## Interface Storage Rebinding

One extra extension point is
[include/dingo/storage/interface_storage_traits.h](../../include/dingo/storage/interface_storage_traits.h).

The container uses it to decide whether a storage handle can be rebound from a
concrete leaf type to an interface leaf type. That is the mechanism behind the
single-interface storage rewrite in registration.

In practice this means the wrapper must be rebindable in a way that preserves
correct ownership and deletion semantics for interface use.

## Good Source Companions

- [include/dingo/type/type_traits.h](../../include/dingo/type/type_traits.h)
- [include/dingo/storage/type_storage_traits.h](../../include/dingo/storage/type_storage_traits.h)
- [include/dingo/type/type_conversion_traits.h](../../include/dingo/type/type_conversion_traits.h)
- [test/type/type_traits.cpp](../../test/type/type_traits.cpp)
