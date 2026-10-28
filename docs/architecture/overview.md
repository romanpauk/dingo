# Overview

Dingo is easiest to understand when separated into four concerns:

- registration deduction
- lookup shape normalization
- storage and factory ownership
- conversion and wrapper adaptation

## Header Layers

The headers under `include/dingo/` form a hierarchy. A header may include
headers of the layers below its own and of its own directory; directories that
share a layer do not include each other. The `layers-check` target enforces
this, and the layer list lives in [tools/layers.py](../../tools/layers.py).

| Layer | Directories | Owns | | ----- | ----------------------- |
\-----------------------------------------------------------------------------------------------
| | 0 | `base/` | configuration, `none`, `selected`, exceptions, `type_list`,
type descriptors | | 1 | `memory/`, `rtti/`, `type/` | allocators and object
stores, rtti providers, type traits, conversions, keys, annotations | | 2 |
`resolution/`, `lookup/` | resolution operations and conversion resolutions,
lookup indexes | | 3 | `factory/` | constructors, callables, constructor
detection, factory traits | | 4 | `context/` | resolution context base and
recursion guard | | 5 | `storage/` | storage tags, scope policies, storage
exposure traits | | 6 | `registration/` | registration options, requirements | |
7 | `binding/` | binding model, selection and resolution shared by runtime and
static | | 8 | `runtime/`, `static/` | runtime registry and static registry,
graph and activation | | 9 | `dingo/*.h` | `container`, `runtime_container`,
`static_container`, `container_operations`, `container_base` |

## Main Owners

### Public Containers

The public entry points are:

- [include/dingo/container.h](../../include/dingo/container.h): combined
  container
- [include/dingo/runtime_container.h](../../include/dingo/runtime_container.h):
  runtime-only facade
- [include/dingo/static_container.h](../../include/dingo/static_container.h):
  static-only facade

`container` is the primary public API. It supports:

- `container<>` for runtime-only registrations
- `container<bindings<...>>` for compile-time bindings with optional runtime
  registrations

Below that API, the runtime registration path owns:

- runtime binding state (`runtime_binding_state`)
- stable runtime binding entries that own lookup identities
- the runtime lookup table used for lookup, collection enumeration, counting,
  and duplicate checks
- dynamic type maps for outer per-interface dispatch
- the resolved-instance cache (`runtime_binding_state::type_cache`)
- parent lookup for nested containers
- the public registration and resolution entry points

The root container owns one runtime domain for itself and for the internal child
containers created by runtime bindings. Those child containers retain their own
lazily allocated lookup state, reached through a dense root-owned scope table,
while borrowing the root arena. Explicit public parent/child containers remain
independent owners.

The compile-time binding path owns:

- compile-time binding normalization
- static dependency graph validation
- compile-time binding selection and static resolution state

The container layer coordinates registration and resolution. It does not
construct objects directly. It stores or selects bindings and asks factories or
static resolution helpers for values, references, or pointers.

### Container Operations

[include/dingo/container_operations.h](../../include/dingo/container_operations.h)
owns the public operation surface for both container modes: `resolve`,
`construct`, `invoke`, the collection entries, and the parent/miss hand-off. It
is parameterized by the container and by whether that container has a runtime
source, so a static-only container and a runtime-backed one share the decision
order while keeping their own selection and execution.

The front-ends in [container.h](../../include/dingo/container.h) and
[static_container.h](../../include/dingo/static_container.h) are configurations
plus state: the static registry keeps its bindings inline, the runtime registry
keeps its lookup state, and the operations layer decides which source answers.
The static minimal-context and construct rules live beside the runtime ones in
the layer rather than in a separate front-end.

### Type Registration

[include/dingo/registration/type_registration.h](../../include/dingo/registration/type_registration.h)
reduces a `register_type<...>()` declaration into a complete policy set:

- `scope_type`
- `storage_type`
- `factory_type`
- `interface_type`
- `conversions_type`

That deduction step is the boundary between the public registration syntax and
the internal binding machinery.

For compile-time sources, the public spelling is `bindings<...>`. Those bindings
are normalized internally through the static registry path under
[include/dingo/static/registry.h](../../include/dingo/static/registry.h).

### Factory And Storage

[include/dingo/runtime/binding.h](../../include/dingo/runtime/binding.h) wraps a
storage object and the child container used to resolve constructor dependencies.

The factory owns the conversion-facing interface:

- `get_value(instance_request)`
- `get_lvalue_reference(instance_request)`
- `get_rvalue_reference(instance_request)`
- `get_pointer(instance_request)`
- scope-specific materialization support

The storage classes under [include/dingo/storage/](../../include/dingo/storage)
own lifetime policy:

- `unique`: construct a fresh instance per resolution
- `shared`: cache a single instance
- `external`: refer to an existing object
- `shared_cyclical`: shared storage with cycle support

### Resolution And Conversion Path

[include/dingo/runtime/binding.h](../../include/dingo/runtime/binding.h) now
owns the conversion-facing resolution flow. It is responsible for:

- asking storage to `resolve(...)` its native source shape
- applying guard and closure scope through `storage_materialization_traits`
- constructing cached conversion objects for shared and external cases

[include/dingo/resolution/resolution_operation.h](../../include/dingo/resolution/resolution_operation.h)
contains the last-mile conversion rules from the stored source shape to the
requested target shape.

The shared runtime/static binding and resolution kernel now lives under
[include/dingo/binding/](../../include/dingo/binding):

- `binding_model.h`
- `binding_selection.h`
- `binding_resolution.h`
- `binding_resolution_policy.h`

Runtime and static implementation code sits under:

- [include/dingo/runtime/](../../include/dingo/runtime)
- [include/dingo/static/](../../include/dingo/static)

## Resolution Sketch

At a high level, Dingo runs this sequence:

1. `register_type<...>()` deduces the missing registration policies.
2. The runtime path creates storage-backed factories for each exposed interface,
   or the static path normalizes `bindings<...>` into a validated static source.
3. `resolve<T>()` converts `T` into the internal lookup form.
4. The container selects the matching runtime or static binding source.
5. The selected source builds an `instance_request` with the lookup type and
   requested descriptor, then calls the matching factory method for value,
   lvalue reference, rvalue reference, or pointer resolution.
6. The factory or static resolution path gets or builds the stored instance
   using its scope-specific state.
7. The matched resolution operation converts that materialized source into the
   requested `T`.

For `container<bindings<...>>`:

- singular runtime/static conflicts are ambiguous
- collection requests merge runtime and static results
- registration child bindings take precedence for singular requests and merge
  child static and runtime collections with the root container

The pieces above are not independent extension systems. They cooperate in one
pipeline.

## Reading The Code

For the core path in source order, start here:

- [include/dingo/container.h](../../include/dingo/container.h)
- [include/dingo/container_operations.h](../../include/dingo/container_operations.h)
- [include/dingo/runtime_container.h](../../include/dingo/runtime_container.h)
- [include/dingo/static_container.h](../../include/dingo/static_container.h)
- [include/dingo/registration/type_registration.h](../../include/dingo/registration/type_registration.h)
- [include/dingo/runtime/registry.h](../../include/dingo/runtime/registry.h)
- [include/dingo/static/registry.h](../../include/dingo/static/registry.h)
- [include/dingo/static/graph.h](../../include/dingo/static/graph.h)
- [include/dingo/runtime/binding.h](../../include/dingo/runtime/binding.h)
- [include/dingo/resolution/resolution_operation.h](../../include/dingo/resolution/resolution_operation.h)
