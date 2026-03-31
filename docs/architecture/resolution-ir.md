# Resolution IR

Dingo now has an internal compile-time IR for resolution in
[include/dingo/resolution/ir.h](../../include/dingo/resolution/ir.h).

This is not a user-facing DSL and not an execution engine. It is a structural
description of what a resolution means:

- the requested type and request shape
- the selected binding
- the acquisition policy for the binding's storage
- the invocation intent used to produce the stored object
- the conversion intent from the stored source shape to the requested result

## Why It Exists

Before this layer, the same semantics were split across ad hoc template
dispatch in several places. The IR makes those semantics explicit and gives the
codebase one canonical vocabulary for:

- compile-time resolution
- future runtime execution plans
- diagnostics and introspection

## Canonical Pieces

The current IR vocabulary is intentionally small:

- `ir::request<...>`: request spelling, lookup type, runtime lookup type, and
  request kind (`value`, `lvalue reference`, `rvalue reference`, `pointer`)
- `ir::collection_request<...>` and `ir::collection_resolution<...>`:
  collection type, element request, element lookup type, and aggregation kind
- `ir::lookup_request<...>` and `ir::lookup_resolution<...>`: single or indexed
  lookup mode, request shape, lookup type, and key type when applicable
- `ir::registration<...>` and `ir::registration_plan<...>`: normalized
  registration shape, including exposed interfaces, scope/storage, stored type,
  invocation intent, conversion policy, index-key metadata, payload kind, and
  single-vs-shared materialization
- `ir::binding<...>`: interface, registered type, stored type, scope/storage,
  factory, and conversion policy
- `ir::acquisition<...>`: storage owner and cacheability
- `ir::factory_invocation<...>`, `ir::constructor_invocation<...>`,
  `ir::detected_constructor_invocation<...>`, `ir::function_invocation<...>`:
  object-production intent
- `ir::conversion<...>`: source/target/storage-tag conversion intent
- `ir::resolution<...>`: one full resolution plan shape
- `ir::resolution_plan<...>`: a naming alias for the full plan type

The important design point is that these are type descriptions only. They do
not perform resolution by themselves.

## Current Consumers

The first consumers are the parts of the system that previously hard-coded the
same decisions in parallel:

- [include/dingo/resolution/instance_factory.h](../../include/dingo/resolution/instance_factory.h)
  now exposes `plan_for<T>()` for concrete factories, including binding,
  acquisition, invocation, and selected conversion structure
- [include/dingo/resolution/runtime_execution_plan.h](../../include/dingo/resolution/runtime_execution_plan.h)
  lowers structural request plans into compact value-level execution plans for
  the runtime backend
- [include/dingo/resolution/type_conversion.h](../../include/dingo/resolution/type_conversion.h)
  now owns the final conversion boundary used by both the runtime backend and
  the remaining static request-shaped result adaptation
- [include/dingo/container.h](../../include/dingo/container.h) now uses a
  structural registration plan to materialize normalized registration shape,
  stores lowered registration-plan records for registry introspection, and
  duplicate registration / index-collision / indexed lookup failures now append
  or inspect that registration plan; the actual registration materialization
  policy now lives in
  [include/dingo/materialization/registration_builder.h](../../include/dingo/materialization/registration_builder.h),
  then uses a
  structural collection plan for `construct_collection(...)` failure reporting,
  and structural lookup plans for ambiguity and not-found diagnostics
- [include/dingo/factory/constructor_detection.h](../../include/dingo/factory/constructor_detection.h)
  now exposes auto-construction as structural invocation IR:
  exact constructor dependency IR when a type provides
  `dingo_constructor_type`, and slot-level detected dependency IR otherwise
- explicit factories such as
  [include/dingo/factory/constructor.h](../../include/dingo/factory/constructor.h)
  and [include/dingo/factory/function.h](../../include/dingo/factory/function.h)
  map themselves to structural invocation IR

That is enough to make request-shape and factory-invocation semantics explicit
without changing the public API.

## Concrete Versus Erased Plans

There are now two plan shapes in active use:

- concrete factory plans: produced by `instance_factory::plan_for<T>()`, with
  structural binding/acquisition/invocation/conversion types
- erased factory plans: produced for
  `instance_factory_interface<...>`, with erased binding/acquisition/conversion
  placeholders but the same request-plan shape

The container resolves through the erased plan path because the registry still
stores factories behind a virtual interface. That keeps execution routed
through one plan boundary today, while preserving room for a richer static path
later.

The runtime path now adds one more step after that structural plan:

```text
resolution_plan -> execution_plan(binding record + conversion op)
                -> factory API call chosen by source acquisition shape
                -> final C++ conversion through type_conversion
```

That keeps type erasure on the execution side instead of in the structural
meaning of resolution.

The runtime binding record now preserves the selected binding's source
acquisition shape and runtime lookup type. That lets the runtime plan describe
cases such as pointer-backed shared bindings resolved as references or
reference-backed external bindings resolved as pointers without re-deriving the
factory call shape from the request alone.

The runtime backend now always lowers to an explicit conversion opcode. Common
raw/native adaptation cases use dedicated source-shaped ops:

- pointer-backed bindings resolved as values or references
- reference-backed bindings resolved as pointers
- reference-backed bindings resolved as values

Array registrations and more complex wrapper, borrow, and alternative-type
paths still stay on the request-shaped acquisition path, but they are no
longer represented as an untyped fallback bucket in the runtime plan.

## Present Limit

True automatic constructor detection still cannot recover arbitrary concrete
constructor parameter types from the language alone. Its IR now preserves the
selected detection tag plus one structural dependency slot per parameter
position, which is enough to make constructor arity and per-slot intent visible
in the plan.

Exact typed constructor dependencies are available when a type opts in with
`dingo_constructor_type`. That keeps the IR honest about what the implementation
really knows instead of pretending generic constructor reflection exists.

Constructor invocation boundaries now also use that IR for failure reporting:
exceptions raised while resolving constructor arguments can append a
`constructor path` that shows the selected constructor shape or detected
dependency slots.
