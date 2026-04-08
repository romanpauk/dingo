# Extending Dingo

Dingo's extensibility is mostly trait-driven. The key is to specialize the trait
that matches the problem actually being solved.

## The Three Main Extension Seams

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

Specialize `storage_traits` after including
[include/dingo/storage/storage_traits.h](../../include/dingo/storage/storage_traits.h)
when defining which result forms a given storage/scope combination can expose.

This trait defines whether a registration can service requests such as:

- value
- lvalue reference
- rvalue reference
- pointer
- wrapper-to-wrapper conversion
- and, when needed, custom guard / closure / source-materialization hooks for
  the storage tag

The same wrapper may have different exposure rules under `unique`, `shared`, or
`external` storage.

### 3. `type_conversion_traits`

Specialize `type_conversion_traits` in
[include/dingo/type/type_conversion_traits.h](../../include/dingo/type/type_conversion_traits.h)
when two wrapper types need a concrete conversion step that is not covered by a
direct converting constructor or pointer cast.

This is the last-mile "build target wrapper from source wrapper" hook.

## Quick Decision Guide

- If Dingo does not understand a wrapper's shape, start with `type_traits`.
- If a wrapper should resolve differently in `shared` and `unique` storage, use
  `storage_traits`.
- If two wrappers need an explicit conversion implementation, use
  `type_conversion_traits`.
- If interface storage rebinding should be possible, make the wrapper rebindable
  and compatible with interface-storage rebinding.

## Worked Example

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

The internal storage-rebinding check lives in
[include/dingo/storage/detail/interface_storage_traits.h](../../include/dingo/storage/detail/interface_storage_traits.h).

The container uses it to decide whether a storage handle can be rebound from a
concrete leaf type to an interface leaf type. That is the mechanism behind the
single-interface storage rewrite in registration.

In practice this means the wrapper must be rebindable in a way that preserves
correct ownership and deletion semantics for interface use.

## A Useful Mental Model

When adding a new wrapper, answer these questions in order:

1. What is the wrapper's leaf type?
2. Can the wrapper be rebound to another leaf type?
3. Is it pointer-like, borrowable, or reference-resolvable?
4. Which request shapes should each storage scope expose?
5. Does any wrapper-to-wrapper conversion need a custom implementation?

If those five answers are clear, the implementation is usually straightforward.

## Good Source Companions

- [include/dingo/type/type_traits.h](../../include/dingo/type/type_traits.h)
- [include/dingo/storage/storage_traits.h](../../include/dingo/storage/storage_traits.h)
- [include/dingo/storage/unique.h](../../include/dingo/storage/unique.h)
- [include/dingo/storage/shared.h](../../include/dingo/storage/shared.h)
- [include/dingo/storage/external.h](../../include/dingo/storage/external.h)
- [include/dingo/storage/shared_cyclical.h](../../include/dingo/storage/shared_cyclical.h)
- [include/dingo/type/type_conversion_traits.h](../../include/dingo/type/type_conversion_traits.h)
- [test/type/type_traits.cpp](../../test/type/type_traits.cpp)
