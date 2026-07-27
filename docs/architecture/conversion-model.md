# Conversion Model

Dingo resolves requests in two stages:

1. determine whether the requested shape is allowed for the stored instance
2. convert the stored source shape into the requested target shape

## Where The Rules Come From

The full conversion behavior is spread across a few layers:

- `storage_traits` says which target shapes are legal for a storage/scope pair
- `type_conversion_traits` validates and performs custom type conversions
- `resolution` records the operation for an allowed source-to-result route
- `instance_request` carries the lookup type and requested descriptor
- `resolution_operation` executes the selected route

The important source files are:

- [include/dingo/storage/type_storage_traits.h](../../include/dingo/storage/type_storage_traits.h)
- [include/dingo/resolution/runtime_binding_interface.h](../../include/dingo/resolution/runtime_binding_interface.h)
- [include/dingo/resolution/resolution_operation.h](../../include/dingo/resolution/resolution_operation.h)
- [include/dingo/type/type_conversion_traits.h](../../include/dingo/type/type_conversion_traits.h)

## Storage Exposure

`storage_traits` and `resolution_traits` combine into `type_storage_traits`.
That combined type lists the result forms a stored object may service:

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

Each descriptor exposes its concrete `request_types`. A mutable reference or
pointer resolution also accepts its const-borrowed form; wrapper and leaf
identity are otherwise preserved.

`value_types` declares values copied from borrowed storage. Each `T&&` in
`rvalue_reference_types` publishes both the rvalue result and its value delivery
as consumed resolutions. Unique storage therefore declares consumed results only
once, while shared and external values become borrowed resolutions. Raw pointer
storage publishes concrete resolutions for its stored pointee shape and, when
the pointee is a convertible wrapper, separate interface-rebound conversions.
Wrapper composition applies only when the registered interface is a leaf type;
an interface that is already a wrapper is published as that exact shape rather
than being nested inside the stored wrapper again. Storage stability remains a
separate property controlling reference and pointer lifetime.

Publication decomposes a conversion into identity forwarding, compatible
reference access, custom trait conversion, address-taking, array projection,
pointer dereferencing, wrapper pointer access, wrapper borrowing, alternative
selection, or retention of a converted wrapper. Pointer-backed storage keeps the
pointer as its source; wrapper composition inspects the pointee shape, and the
conversion route records the dereference explicitly. The chosen decomposition is
stored in the operation and executed directly; execution does not classify the
target and source a second time. Custom `type_conversion_traits` take precedence
over the default compatible-reference shortcut.

Keeping the operation in the resolution makes route selection and execution one
decision. New wrapper, alternative, and array operations can be introduced by
the traits that publish those routes without adding matching branches to the
binding dispatcher. Dispatchers only materialize the source and execute the
operation carried by the matched resolution. A retained conversion receives an
already-converted value; the resolver only stores or caches it and never
performs a second conversion. Operations also publish any cache,
source-retention, and static temporary-storage requirements created by their
execution. The runtime and static backends derive those resources from the same
resolution list instead of maintaining a parallel list of conversion types on
the storage. A new operation must provide `cache_types<Storage>`,
`temporary_types<Storage>`, and `requires_source_retention<Storage>` along with
its `apply` function, even when those requirements are empty.

Factory construction and storage rebinding use this operation model as well.
When a factory selects an alternative to construct, Dingo plans the conversion
from that selected value to the requested wrapper shape and executes the
resulting wrapper/alternative operations. Storage rebinding similarly selects a
conversion operation rather than calling `type_conversion_traits` directly.
There is no separate recursive construction trait for wrapper composition.
Conversion planning marks a route that starts by constructing the selected
factory type. Factory dispatch consumes that route directly; it does not inspect
the conversion tree for variants or wrappers.

The conversion's `available` value and required source access come from that
same recursive decomposition. Leaf operations require either `borrow` or
`consume`; dereference, wrapper borrowing, and retention inherit the inner
operation's requirement, while an alternative keeps the requirements of its
viable branches. Conversion selection applies one rule: consumed storage
satisfies both requirements, while borrowed storage satisfies only `borrow`.

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
published value category and delegates to the selected conversion node. Identity
conversion forwards an exact source, while a custom trait conversion creates a
new value.

Broadly, the rules cover:

- direct value resolution for unique storage
- borrowed references and pointers from borrowable wrappers
- retained wrapper conversions whose result must outlive one expression
- fixed-array reference and pointer projections
- exact lookup handling for requests that must match a precise wrapper spelling

New conversion behavior should be modeled as a route operation first. This keeps
availability, ownership, and execution from becoming independent rule sets
again.

## Conversion Cache

Shared and external resolution can preserve conversion objects through
[include/dingo/resolution/runtime_binding.h](../../include/dingo/resolution/runtime_binding.h)
and
[include/dingo/resolution/conversion_cache.h](../../include/dingo/resolution/conversion_cache.h).

That cache matters when a conversion object must outlive a single expression,
for example when a resolved shared object hands out references into a converted
wrapper form.

The current implementation is intentionally narrow: the conversions object is
closer to an expanded variant of cached conversion instances than to a general
conversion graph.
