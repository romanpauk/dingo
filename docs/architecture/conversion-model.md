# Conversion Model

Dingo resolves requests in two stages:

1. determine whether the requested shape is allowed for the stored instance
1. convert the stored source shape into the requested target shape

## Where The Rules Come From

The full conversion behavior is spread across a few layers:

- `resolution::ir` describes the requested shape, binding, and conversion intent
- `execution_plan` lowers the common runtime conversion opcode
- `execution_plan` also carries the binding's source acquisition shape
  and runtime lookup type for the lowered path
- `storage_traits` says which target shapes are legal for a storage/scope pair
- `type_conversion` owns final result adaptation and storage-source conversion
  execution for both request-shaped and runtime-plan execution paths
- `type_conversion_traits` handles explicit wrapper-to-wrapper conversions

The important source files are:

- [include/dingo/resolution/ir.h](../../include/dingo/resolution/ir.h)
- [include/dingo/resolution/runtime_execution_plan.h](../../include/dingo/resolution/runtime_execution_plan.h)
- [include/dingo/storage/type_storage_traits.h](../../include/dingo/storage/type_storage_traits.h)
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

## Request Dispatch

The container itself does not know how to convert every shape. It delegates the
request form through the structural plan and the runtime backend:

- `T` uses `get_value`
- `T&` and `const T&` use `get_lvalue_reference`
- `T&&` uses `get_rvalue_reference`
- `T*` uses `get_pointer`

For the erased runtime path, the selected binding record now preserves the
source access shape and lookup type, so execution can ask the factory for the
source form it actually owns and apply the lowered conversion op afterward.
That is what makes pointer-to-reference/value and reference-to-pointer
adaptation explicit in the runtime backend.

## Runtime Conversion

[include/dingo/resolution/type_conversion.h](../../include/dingo/resolution/type_conversion.h)
is the shared conversion core. It owns:

- final pointer-to-`T` result adaptation for request-shaped resolution
- runtime-plan conversion opcode execution
- storage-source conversion rules for wrapper, borrow, handle, array, and
  alternative paths

The current runtime backend lowers explicit conversion routes and then calls
that shared conversion core:

- request-shaped identity conversions
- pointer-backed bindings resolved as values or references
- lvalue-reference-backed bindings resolved as pointers
- lvalue-reference-backed bindings resolved as values

Complex wrapper, borrow, and alternative-type paths still execute through the
request-shaped factory path, but they now lower to concrete request-shaped
opcodes rather than an untyped fallback mode.

Bounded array registrations deliberately stay on the request-shaped path. Their
`Class*` exposure is an element view, not a general "source object pointer", so
the runtime backend does not reinterpret that path as `Class&` or value
construction.

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
[include/dingo/resolution/instance_resolver.h](../../include/dingo/resolution/instance_resolver.h)
and
[include/dingo/resolution/conversion_cache.h](../../include/dingo/resolution/conversion_cache.h).

That cache matters when a conversion object must outlive a single expression,
for example when a resolved shared object hands out references into a converted
wrapper form.

The current implementation is intentionally narrow: the conversions object is
closer to an expanded variant of cached conversion instances than to a general
conversion graph.

## Typical Customization Flow

If a request fails to resolve in the way you expect, inspect the layers in this
order:

1. Does `storage_traits` expose the requested result shape?
1. Does the wrapper define the right borrow/reset/rebind semantics in
   `type_traits`?
1. Does `type_conversion` already have a generic rule for this pair?
1. If not, should `type_conversion_traits` bridge the two wrapper types?

That sequence usually leads to the real missing piece quickly.
