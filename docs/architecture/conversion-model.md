# Conversion Model

Dingo resolves a request in two stages:

1. It checks whether the stored instance can give the requested shape.
2. It converts the stored source shape into the requested target shape.

## Where The Rules Come From

The conversion rules are in these parts of the library:

- `scope_traits` gives the target shapes that a scope publishes for a stored
  type.
- `type_conversion_traits` checks and performs custom type conversions.
- `resolution` records the operation of an allowed route from a source to a
  result.
- `instance_request` carries a runtime request: its lookup type, its type
  descriptor, and a function that derives a request the binding does not
  publish.
- Each conversion of `type_conversion_traits.h` applies itself.

The main source files are:

- [include/dingo/storage/scope_traits.h](../../include/dingo/storage/scope_traits.h)
- [include/dingo/storage/type_storage_traits.h](../../include/dingo/storage/type_storage_traits.h)
- [include/dingo/binding/runtime_binding_interface.h](../../include/dingo/binding/runtime_binding_interface.h)
- [include/dingo/resolution/resolution_operation.h](../../include/dingo/resolution/resolution_operation.h)
- [include/dingo/type/type_conversion_traits.h](../../include/dingo/type/type_conversion_traits.h)

## Storage Exposure

The request lists of a registration, `detail::conversions`, come from the
`scope_traits` of the scope and the form of the stored type. The `type_traits`
semantics of the stored type decide its form. Dingo rejects a form that the
scope does not store. The lists name the results that a stored object can give:

- `value_types`
- `lvalue_reference_types`
- `rvalue_reference_types`
- `pointer_types`

The lists give the primitive request shapes of the storage. Dingo combines each
shape with the source category that the storage produces. The result is a
`resolution<Target, Operation>`, which both containers use. A resolution
records:

- the target type that requests are matched against;
- an operation that contains the source type and the selected conversion.

A binding has its own routes for each request category: value, lvalue reference,
rvalue reference and pointer.

A resolution matches a request of the same type, reference and pointer shape
(`is_resolution_request`). A value resolution matches the value with any `const`
or `volatile`. A reference or a pointer resolution also matches a request with
more `const`. The wrapper and the leaf must be the same.

These rules apply to the lists:

- `value_types` lists the values copied from borrowed storage.
- Each `T&&` in `rvalue_reference_types` publishes two consumed resolutions: the
  rvalue reference and the value. So unique storage declares its consumed
  results once. Shared and external values are borrowed resolutions.
- Raw pointer storage publishes resolutions for its stored pointee shape. If the
  pointee is a convertible wrapper, it also publishes conversions rebound to the
  interface.
- Shared and external storage copy out the leaf of a raw pointer, of a
  `unique_ptr` and of a `shared_ptr`, and the copyable layers of a nested
  wrapper. A pointee that is not copy constructible publishes no copy.
- Wrapper composition applies only if the registered interface is a leaf type.
  An interface that is a wrapper is published in exactly its shape. Dingo does
  not nest it in the stored wrapper again.
- Storage stability (`is_stable`) is a separate property. It controls the
  lifetime of references and pointers.

Publication finds the conversion from the source to the target as a path of
operations. These are the operations by which the
[specification](../specification.md) derives a request. An operation applies to
the source, unwraps the source before an inner conversion, or wraps what an
inner conversion returns:

- `conversion_leaf` applies to the source. It gives the source itself, a
  reference to it or to its base, its address, or the pointer an array decays
  to. It gives a pointer to the array that an owning pointer to its first
  element points to; a unique scope hands over an array it makes in this form.
  It also gives the address of the element of a wrapper, or what
  `type_conversion_traits` converts the source to.
- `conversion_unwrap` gives the element that a pointer or a wrapper lends, its
  `type_traits::borrow`. `alternative_type_conversion` unwraps the alternative
  that an alternative type holds, with one branch for each alternative.
- `conversion_wrap` makes an optional value, an alternative type, or a new
  owning handle of an object, of a wrapper or of an array.

An operation is a class with a static `apply`. The shape that uses the operation
calls it. Each operation also has a constexpr function that returns its path, or
an unavailable conversion if the operation does not apply. `select` tries the
paths in the order of `op` and takes the first available one. It does not
instantiate the paths of later operations. Pointer storage keeps the pointer as
its source, and the path borrows the pointee explicitly. The operation stores
the chosen conversion, and the binding executes it directly. Execution does not
classify the target and the source again. A source and target pair with a custom
`type_conversion_traits` uses only that conversion and tries no default path.

The resolution keeps its operation, so route selection and execution are one
decision. The traits that publish a route can add wrapper, alternative and array
operations, and the binding dispatcher needs no matching branches. A dispatcher
only materializes the source and executes the operation of the matched
resolution. A conversion makes a value. A reference or a pointer to a converted
value is not a conversion. It is a request derived from the value, which the
binding keeps or the context holds (see [Retention](#retention)). An operation
provides `apply`, a function that returns its path, and a place in `op`. The
conversion that its path makes provides `available`, `required_access` and
`apply`.

Factory construction and storage rebinding use the same operation model. So does
`construct<T>()` of a wrapper of a bound type that its binding does not publish:
the wrapper is the conversion from the value that the binding resolves. A
storage hands out an array like any other object, and the conversion decays it.
When a factory selects an alternative to construct, Dingo plans the conversion
from the selected value to the requested wrapper shape. It then executes the
wrapper and alternative operations of that conversion. Storage rebinding
converts through `convert_type`, which selects a conversion operation. No
separate recursive construction trait exists for wrapper composition. Conversion
planning marks a route that starts with the construction of the selected factory
type (`converted_construction`). Factory dispatch uses that route directly. It
does not inspect the conversion tree for alternatives or wrappers.

The `available` value and the required source access of a conversion come from
the same recursive decomposition:

- A leaf operation requires `borrow` or `consume`.
- An unwrapping or a wrapping conversion has the requirement of its inner
  conversion.
- An alternative has the requirements of its available branches.
- Conversion selection uses one rule: consumed storage satisfies both
  requirements, and borrowed storage satisfies only `borrow`.

Specialize `type_conversion_traits` if the default rules cannot infer a custom
conversion. A conversion is available only if `convert(Source)` is well-formed
and its result can construct the target. To restrict the source categories, use
`convert` overloads or SFINAE, not a separate availability declaration. Each
specialization also declares `required_access<Source>` as `borrow` or `consume`.
This states the access of the conversion directly. Callability does not encode
the access, and no separate ownership exception exists.

For a default conversion, Dingo infers the access:

- A value constructed from an lvalue borrows only if the conversion accepts a
  const view of the source. Execution then uses the same const view. A
  construction that needs a mutable lvalue consumes.
- A compatible lvalue reference binds directly and borrows. It aliases the
  object and keeps the requested reference qualification. It does not construct
  a value.
- An owning handle constructed from a source that is not an owning handle, such
  as a raw pointer, consumes.
- Wrapper and alternative operations keep the access of their inner conversion.
  So Dingo decides the access once, at the handle conversion itself, and does
  not infer it from the outer types.

## Factory Dispatch

Both containers match a request against the resolution list of the binding,
`detail::binding_resolutions`, and take the first resolution that matches:

- The static container matches at compile time (`matching_binding_resolution`).
  It forms only the resolutions of the request category, because the target
  shapes of the categories are disjoint.
- The runtime container matches the type descriptor of the request,
  `instance_request::requested_type`, against the descriptor of each target
  (`matches_resolution_request`). `runtime_binding::dispatch_request` then
  applies the resolution at the matched index.
- If no resolution matches, the binding derives the request from one that it
  resolves. In the runtime container, the `derive` function of the
  `instance_request` does this.

Leaf normalization applies only to the selection of the binding. Dingo does not
repeat it to choose a conversion route.

A runtime resolution returns a `resolved_address`. It holds the erased address
and the access of the conversion result. The typed caller
(`convert_resolved_binding`) moves a consumed value and copies a borrowed one.
Unique storage materializes an rvalue source. Shared and external storage
usually give an lvalue or a pointer source. A reference or a pointer request
uses only the address, because it borrows.

## Runtime Conversion

[include/dingo/resolution/resolution_operation.h](../../include/dingo/resolution/resolution_operation.h)
executes the operation that publication selected. `type_resolution` carries the
source and conversion types. The `resolution` that holds it carries the target.
`type_resolution` gets the source in the published value category and applies
the selected conversion. If the conversion is not available, it throws
`type_not_convertible`. Each conversion in
[include/dingo/type/type_conversion_traits.h](../../include/dingo/type/type_conversion_traits.h)
states what it requires of its source, `borrow` or `consume`, and how it
applies. An unwrapping or a wrapping conversion applies its inner conversion to
what it unwraps or wraps. The identity conversion forwards an exact source. A
custom trait conversion makes a value.

The rules cover:

- direct value resolution for unique storage
- borrowed references and pointers from wrappers that lend their element
- reference and pointer projections of fixed arrays
- requests that must match an exact wrapper type (`exact_lookup`)

Model each additional conversion behavior as a route operation first. Then
availability, ownership and execution stay one set of rules.

## Retention

A reference or a pointer to a wrapper that holds a copy sharing the instance is
derived from the wrapper. Examples are a `std::shared_ptr<I> &` of a kept
`std::shared_ptr<T>`, and the `std::optional<std::shared_ptr<T>> &` that a
request composes of one.

- A binding of a scope that keeps its instance keeps the wrapper in the
  retention of its cell,
  [include/dingo/storage/retention.h](../../include/dingo/storage/retention.h).
  The retention holds one object for each type, under the key of the type. It
  makes the object the first time a request asks for it. So every reference to
  it refers to the same object while the binding lives. A runtime binding also
  publishes the address in the request cache.
- A binding of a scope that makes an instance for each request keeps nothing. It
  places the wrapper in the context instead.

A runtime container makes the retained objects in its persistent memory. It
forgets them when the transaction that made them rolls back. A static container
can have no persistent memory, so its retention owns the objects.
