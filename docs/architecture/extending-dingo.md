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

A specialization derives from the semantics of the wrapper and provides its
operations. The standard wrappers are specialized the same way, so a user
wrapper takes part in conversions and storage exactly as `std::unique_ptr` or
`std::optional` does:

```cpp
template <typename T>
struct type_traits<my_handle<T>> : semantics::shared_owner {
  using value_type = T;
  template <typename U> using rebind_t = my_handle<U>;

  static T *get(const my_handle<T> &handle) { return handle.get(); }
  static T &borrow(const my_handle<T> &handle) { return *handle; }
  static bool empty(const my_handle<T> &handle) { return !handle; }

  template <typename... Args> static my_handle<T> make(Args &&...args);
};
```

The semantics are:

- `semantics::observer`: a handle to an object it does not own, such as a raw
  pointer
- `semantics::sole_owner`: the sole owner of an object, such as
  `std::unique_ptr`; it also declares `share_t<U>`, the sharing handle its
  object can be handed to
- `semantics::shared_owner`: a handle sharing ownership, such as
  `std::shared_ptr`
- `semantics::optional_value`: a value that holds an object or none, such as
  `std::optional`

The semantics say whether the wrapper is pointer-like and whether it owns its
element, and they select the storage form, which decides the requests each scope
publishes for the wrapper. A wrapper whose element can be borrowed as an object
declares `borrow`; an owning handle without it holds an array. Dingo does not
create an owning handle from a borrowed non-owning source through a default
conversion. This rule applies to the leaf handle conversion. Alternative and
optional-style conversions first select or unwrap their contained conversion,
then inherit its `borrow` or `consume` requirement.

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

The built-in scopes do not specialize `storage_traits` per wrapper. Each scope
is described once by a `storage_scope_policy` specialization (stability,
consumed versus borrowed source, recursion guard, and the request lists it
publishes for each `storage_form`), and
[include/dingo/storage/storage_scope_policy.h](../../include/dingo/storage/storage_scope_policy.h)
generates `storage_traits` and `storage_materialization_traits` from it. A
(scope, form) cell that deviates from that rule specializes
`detail::storage_cell`. A wrapper is classified into a form by its semantics, so
a custom wrapper needs no `storage_traits` of its own. `storage_traits` remains
a specialization point for a shape the forms do not describe.

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

and then specializes the traits Dingo needs:

- `type_traits` for the semantics and operations of each wrapper
- `type_conversion_traits` for the conversion between handles of different
  elements

Their definitions are in
[test/support/custom_wrappers.h](../../test/support/custom_wrappers.h).

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
