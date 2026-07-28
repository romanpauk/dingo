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
- whether it owns the pointee
- how to get or borrow the underlying object
- how to reset it
- how to rebind it to a different leaf type
- whether Dingo may resolve references through it
- how wrapper-shaped resolution should behave for that type

If Dingo needs to understand a wrapper's shape, start here.

Set `is_owning_handle` to `true` for adopting or shared-ownership handles and to
`false` for observers. Dingo does not create an owning handle from a borrowed
non-owning source through a default conversion. This rule applies to the leaf
handle conversion. Alternative and optional-style conversions first select or
unwrap their contained conversion, then inherit its `borrow` or `consume`
requirement.

An optional-style wrapper provides `value_type`, `get`, `empty`, `wrap`, and
`make_empty`. These operations let Dingo preserve an empty source and construct
the target wrapper around the converted contained value. An alternative provides
`selected_type<Source>` and `wrap<Selected>` so Dingo can apply the same rule to
the alternative selected by overload resolution.

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

Put a result shape in `value_types` when it is copied from borrowed storage;
Dingo exposes it only when the resolved type is copy constructible. Put `T&&` in
`rvalue_reference_types` when the storage produces `T` for consumption; Dingo
derives the corresponding value request when `type_conversion_traits` accepts
the concrete source category. A consumable result does not also need to appear
in `value_types`. These declarations are converted into concrete `resolution`
descriptors, so the downstream resolver no longer has to infer source ownership
or exact lookup behavior from the request spelling.

### 3. `type_conversion_traits`

Specialize `type_conversion_traits` in
[include/dingo/type/type_conversion_traits.h](../../include/dingo/type/type_conversion_traits.h)
when two wrapper types need a concrete conversion step that is not covered by a
direct converting constructor or pointer cast.

The `convert` function performs the last-mile "build target wrapper from source
wrapper" step and is the availability signal. A conversion is available exactly
when `convert(Source)` is well-formed and its result can construct the target.
Restrict source categories through `convert` overloads or SFINAE.

Every specialization also declares `required_access<Source>` as `borrow` or
`consume`. Use `borrow` when conversion may read the source without transferring
from it, including conversions that allocate an independent copy. Use `consume`
when conversion can move from the source or acquire ownership represented by it.
Route selection applies this declaration in the same way as requirements derived
for built-in conversion operations, so request discovery and execution describe
one conversion.

An applicable specialization that omits `required_access<Source>` is rejected
instead of falling through to another conversion route. Default value
construction from an lvalue is borrowable only when it accepts a const view of
the source. Direct compatible lvalue-reference binding remains borrowable
because it aliases the existing object rather than constructing a value.

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
- `type_conversion_traits` for conversion availability and execution

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
