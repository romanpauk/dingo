# Conversion Model

Dingo resolves requests in two stages:

1. determine whether the requested shape is allowed for the stored instance
2. convert the stored source shape into the requested target shape

## Where The Rules Come From

The full conversion behavior is spread across a few layers:

- `scope_traits` says which target shapes a scope publishes for a stored type
- `type_conversion_traits` validates and performs custom type conversions
- `resolution` records the operation for an allowed source-to-result route
- `instance_request` carries the lookup type and requested descriptor
- each conversion of `type_conversion_traits.h` applies itself

The important source files are:

- [include/dingo/storage/scope_traits.h](../../include/dingo/storage/scope_traits.h)
- [include/dingo/storage/type_storage_traits.h](../../include/dingo/storage/type_storage_traits.h)
- [include/dingo/binding/runtime_binding_interface.h](../../include/dingo/binding/runtime_binding_interface.h)
- [include/dingo/resolution/resolution_operation.h](../../include/dingo/resolution/resolution_operation.h)
- [include/dingo/type/type_conversion_traits.h](../../include/dingo/type/type_conversion_traits.h)

## Storage Exposure

The request lists of a registration, `detail::conversions`, are generated from
the `scope_traits` of the scope and the form of the stored type, which its
`type_traits` semantics decide. A form the scope does not store is rejected. The
lists name the result forms a stored object may service:

- `value_types`
- `lvalue_reference_types`
- `rvalue_reference_types`
- `pointer_types`

Those lists describe the storage's primitive request shapes. Dingo combines each
shape with the source category actually produced by the storage and turns it
into a `resolution<Target, Operation>` before either resolver sees it. A
resolution records:

- the concrete target used for request matching
- an operation containing the source type and selected conversion

A binding publishes one route per request category: the value, reference, rvalue
reference and pointer requests of an object each have a route of their own.

Each descriptor exposes its concrete `request_types`. A mutable reference or
pointer resolution also accepts its const-borrowed form; wrapper and leaf
identity are otherwise preserved.

`value_types` declares values copied from borrowed storage. Each `T&&` in
`rvalue_reference_types` publishes both the rvalue result and its value delivery
as consumed resolutions. Unique storage therefore declares consumed results only
once, while shared and external values become borrowed resolutions. Raw pointer
storage publishes concrete resolutions for its stored pointee shape and, when
the pointee is a convertible wrapper, separate interface-rebound conversions.
Shared and external storage alike copy out the leaf of a raw pointer, of a
`unique_ptr` and of a `shared_ptr`, together with the copyable layers of a
nested wrapper; a pointee that is not copy constructible publishes no copy.
Wrapper composition applies only when the registered interface is a leaf type;
an interface that is already a wrapper is published as that exact shape rather
than being nested inside the stored wrapper again. Storage stability remains a
separate property controlling reference and pointer lifetime.

Publication finds the conversion from the source to the target as a path of
operations, the operations the [specification](../specification.md) derives a
request by. An operation is applied to the source, unwraps it before an inner
conversion, or wraps what an inner conversion returns:

- `conversion_leaf`: the source itself, a reference to it or to its base, its
  address, the pointer an array decays to, a pointer to the array an owning
  pointer to its first element points to, as a unique scope hands over an array
  it makes, the address of the element of a wrapper, or what
  `type_conversion_traits` converts it to
- `conversion_unwrap`: the element a pointer or a wrapper lends, its
  `type_traits::borrow`; the alternative an alternative type holds is unwrapped
  by `alternative_type_conversion`, each alternative a branch of its own
- `conversion_wrap`: an optional value, an alternative type, or a new owning
  handle of an object, of a wrapper or an array of it

An operation is a class with a static `apply`, called by the shape it is used
in, and a constexpr function that returns its path, or an unavailable conversion
where the operation does not apply. `select` tries the paths in the order of
`op` and takes the first available one; the paths of later operations are not
instantiated once one is. Pointer-backed storage keeps the pointer as its
source, and the path borrows its pointee explicitly. The chosen conversion is
stored in the operation and executed directly; execution does not classify the
target and source a second time. Custom `type_conversion_traits` take precedence
over the default compatible-reference shortcut.

Keeping the operation in the resolution makes route selection and execution one
decision. New wrapper, alternative, and array operations can be introduced by
the traits that publish those routes without adding matching branches to the
binding dispatcher. Dispatchers only materialize the source and execute the
operation carried by the matched resolution. A conversion makes a value; a
reference or a pointer to a converted value is not a conversion, but a request
derived from the value, which the binding keeps or the context holds (see
[Retention](#retention)). A new operation provides `apply`, a function returning
its path, and a place in `op`; the conversion its path makes provides
`available`, `required_access` and `apply`.

Factory construction and storage rebinding use this operation model as well, and
so does `construct<T>()` of a wrapper of a bound type that its binding does not
publish: the wrapper is the conversion from the value the binding resolves. A
storage hands out an array as it hands out any other object, and the conversion
decays it. When a factory selects an alternative to construct, Dingo plans the
conversion from that selected value to the requested wrapper shape and executes
the resulting wrapper/alternative operations. Storage rebinding similarly
selects a conversion operation rather than calling `type_conversion_traits`
directly. There is no separate recursive construction trait for wrapper
composition. Conversion planning marks a route that starts by constructing the
selected factory type. Factory dispatch consumes that route directly; it does
not inspect the conversion tree for variants or wrappers.

The conversion's `available` value and required source access come from that
same recursive decomposition. Leaf operations require either `borrow` or
`consume`; an unwrapping or a wrapping conversion inherits the inner operation's
requirement, while an alternative keeps the requirements of its viable branches.
Conversion selection applies one rule: consumed storage satisfies both
requirements, while borrowed storage satisfies only `borrow`.

Specialize `type_conversion_traits` when a custom conversion cannot be inferred
from the default rules. A conversion is available exactly when `convert(Source)`
is well-formed and its result can construct the target. Restrict accepted source
categories through `convert` overloads or SFINAE rather than a separate
availability declaration. Every specialization also declares
`required_access<Source>` as either `borrow` or `consume`. This describes the
conversion directly instead of encoding source access in callability or relying
on a separate ownership exception.

For default value construction from an lvalue, Dingo infers `borrow` only when
the conversion accepts a const view of the source and then uses that same const
view during execution. Construction available only from a mutable lvalue
requires `consume`. Direct compatible lvalue-reference binding is also a borrow:
it aliases the existing object and preserves the requested reference
qualification instead of constructing a value. For other default conversions,
Dingo requires `consume` when an owning leaf handle is constructed from a
non-owning leaf handle. Wrapper and alternative operations preserve their inner
conversion's access requirement, so this decision is made once at the actual
handle conversion rather than inferred recursively from the outer types.

## Factory Dispatch

The container builds an `instance_request` and calls the matching factory
method:

- `T` uses `get_value(...)`
- `T&` and `const T&` use `get_lvalue_reference(...)`
- `T&&` uses `get_rvalue_reference(...)`
- `T*` uses `get_pointer(...)`

Each factory entry point dispatches through the same concrete resolution list.
Static lookup compares the caller against each resolution's accepted request
types at compile time; runtime lookup compares the corresponding concrete type
descriptors. Leaf normalization is limited to binding selection and is not
repeated when choosing a conversion route.

Runtime value resolution returns a `resolved_address`, which accompanies the
erased address with the access of the actual conversion result. The typed caller
moves a consumable result and copies a borrowed result. Unique storage
materializes an rvalue source, while shared and external storage normally expose
an lvalue or pointer source. Reference and pointer entry points continue to
return only an address because they are borrowed by definition.

## Runtime Conversion

[include/dingo/resolution/resolution_operation.h](../../include/dingo/resolution/resolution_operation.h)
executes the operation selected during publication. `type_resolution` carries
the target, source, and conversion types. It extracts the source with the
published value category and applies the selected conversion. Every conversion
in
[include/dingo/type/type_conversion_traits.h](../../include/dingo/type/type_conversion_traits.h)
says what it requires of its source, `borrow` or `consume`, and how it is
applied; an unwrapping or a wrapping conversion applies its inner conversion to
what it unwraps or wraps. Identity conversion forwards an exact source, while a
custom trait conversion creates a new value.

Broadly, the rules cover:

- direct value resolution for unique storage
- borrowed references and pointers from borrowable wrappers
- fixed-array reference and pointer projections
- exact lookup handling for requests that must match a precise wrapper spelling

New conversion behavior should be modeled as a route operation first. This keeps
availability, ownership, and execution from becoming independent rule sets
again.

## Retention

A reference or a pointer to a wrapper that holds a copy sharing the instance,
such as a `std::shared_ptr<I> &` of a kept `std::shared_ptr<T>`, or the
`std::optional<std::shared_ptr<T>> &` a request composes of one, is derived from
the wrapper: a binding of a scope that keeps its instance keeps the wrapper in
the retention of its cell,
[include/dingo/storage/retention.h](../../include/dingo/storage/retention.h):
one object per type, under the key of the type, made the first time it is asked
for, so that every reference to it refers to the same object for as long as the
binding lives, and a runtime binding publishes its address in the request cache.
A binding of a scope that makes an instance for each request places the wrapper
in the context instead. A runtime container makes the objects in its persistent
memory and forgets them when the transaction that made them rolls back; a static
container, which may have no persistent memory, owns them in the retention. A
binding of a scope that makes an instance for each request keeps nothing.
