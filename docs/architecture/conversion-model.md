# Conversion Model

Dingo resolves requests in two stages:

1. determine whether the requested shape is allowed for the stored instance
2. convert the stored source shape into the requested target shape

## Where The Rules Come From

The full conversion behavior is spread across a few layers:

- `storage_traits` says which target shapes are legal for a storage/scope pair
- `instance_request` carries the lookup type and requested descriptor
- `type_conversion` performs the runtime conversion
- `type_conversion_traits` handles explicit wrapper-to-wrapper conversions

The important source files are:

- [include/dingo/storage/type_storage_traits.h](../../include/dingo/storage/type_storage_traits.h)
- [include/dingo/resolution/runtime_binding_interface.h](../../include/dingo/resolution/runtime_binding_interface.h)
- [include/dingo/resolution/type_conversion.h](../../include/dingo/resolution/type_conversion.h)
- [include/dingo/type/type_conversion_traits.h](../../include/dingo/type/type_conversion_traits.h)

## Storage Exposure

`storage_traits` and `resolution_traits` combine into `type_storage_traits`.
That combined type lists the result forms a stored object may service:

- `value_types`
- `lvalue_reference_types`
- `rvalue_reference_types`
- `pointer_types`
- `conversion_types`

Those lists are the contract between storage policy and runtime conversion.

## Factory Dispatch

The container builds an `instance_request` and calls the matching factory
method:

- `T` uses `get_value(...)`
- `T&` and `const T&` use `get_lvalue_reference(...)`
- `T&&` uses `get_rvalue_reference(...)`
- `T*` uses `get_pointer(...)`

Each factory entry point then checks the request against the corresponding
conversion list exposed by the stored type.

## Runtime Conversion

[include/dingo/resolution/type_conversion.h](../../include/dingo/resolution/type_conversion.h)
implements the actual conversion logic.

Broadly, the rules cover:

- direct value resolution for unique storage
- borrowed references and pointers from borrowable wrappers
- wrapper-preserving conversions when two handle types share the same handle
  shape
- array-specific rules
- exact lookup handling for requests that must match a precise wrapper spelling

The file is intentionally concrete. The architecture docs should treat it as the
final execution layer, not as a place to duplicate rule-by-rule prose.

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

## Typical Customization Flow

If a request fails to resolve unexpectedly, inspect the layers in this order:

1. Does `storage_traits` expose the requested result shape?
2. Does the wrapper define the right borrow/reset/rebind semantics in
   `type_traits`?
3. Does `type_conversion` already have a generic rule for this pair?
4. If not, should `type_conversion_traits` bridge the two wrapper types?

That sequence usually leads to the real missing piece quickly.
