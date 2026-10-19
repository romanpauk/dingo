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
`static_container`, `container_base` |

## Runtime And Static Mirror

`runtime/` and `static/` answer the same questions with different means: the
runtime side keeps mutable state and discovers bindings by lookup, the static
side derives everything from the `bindings<...>` type list. They are kept
mirrored, so both directories contain headers of the same names with the same
roles. The `layers-check` target enforces the file names.

- `container_traits.h`: container configuration and its detection. Runtime holds
  rtti, allocator and lookup definitions; static holds the same for
  `static_container`.
- `context.h`: per-resolution state, construction and the type path. Runtime is
  backed by an arena and a transaction; static uses fixed frames sized by the
  graph.
- `lookup.h`: finding the bindings of an interface and key. Runtime uses lookup
  indexes and routes; static uses a type-list index answered by overload
  resolution.
- `selection.h`: the outcome of a lookup, found, missing or ambiguous. Runtime
  returns the value `runtime_binding_selection`; static yields the type
  `binding_choice`.
- `registration.h`: how bindings come to exist. Runtime has `prepare_binding`,
  key commit and rollback; static has `static_bindings` with dependency and key
  validation.
- `state.h`: where binding state lives. Runtime has `runtime_registry_data` and
  scopes; static has `static_binding_storage` and the activation sets.
- `binding.h`: one binding, its storage, factory and conversion cache. Runtime
  has `runtime_binding`; static has the storage slots and `binding_activation`.
- `resolution.h`: turning a selected binding and a request into a value. Runtime
  dispatches by type descriptor; static uses `static_binding_resolver`.
- `registry.h`: the source that selects, resolves and enumerates bindings.
  Runtime has `runtime_registry`; static has `static_registry`.

Some headers exist on one side only. The runtime side owns its storage and rolls
it back (`container_runtime.h`, `transaction.h`) and holds the index data
structure (`lookup_index.h`); the static side proves its resolution graph at
compile time (`graph.h`). Those names are listed in
[tools/layers.py](../../tools/layers.py).

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
are normalized internally through the static registration path under
[include/dingo/static/registration.h](../../include/dingo/static/registration.h).

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
- [include/dingo/runtime_container.h](../../include/dingo/runtime_container.h)
- [include/dingo/static_container.h](../../include/dingo/static_container.h)
- [include/dingo/registration/type_registration.h](../../include/dingo/registration/type_registration.h)
- [include/dingo/runtime/registry.h](../../include/dingo/runtime/registry.h)
- [include/dingo/static/registry.h](../../include/dingo/static/registry.h)
- [include/dingo/static/graph.h](../../include/dingo/static/graph.h)
- [include/dingo/runtime/binding.h](../../include/dingo/runtime/binding.h)
- [include/dingo/resolution/resolution_operation.h](../../include/dingo/resolution/resolution_operation.h)
